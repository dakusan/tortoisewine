//x86_64-w64-mingw32-gcc git-launcher.c -o git.exe -mconsole -s -Os -Wl,--gc-sections

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <signal.h>
#include <unistd.h> //usleep
#include <fcntl.h> //_O_BINARY

//Set this
const char *GitPath="c:\\git-wrapper.exe"; //Do not try to escape this string

//No need to update these
const char *FileTempPath="C:\\git.tmp", *FileTempPathErr="C:\\git.tmp.err";
const char EndSeq[]={0, '!', 0, '!', 0, '!', 0, '!', 0, '!'}; //This sequence marks the end of the output
const int GrowSize=1024;
enum { SeqSize=sizeof(EndSeq) }; //Using enum for compile time constant

//Simple use functions
void MilliSleep(int Milliseconds) { usleep(Milliseconds*1000); }
int min(int A, int B) { return A < B ? A : B; }

//Simple string handling
typedef struct
{
	char *Str;
	size_t Len, Cap;
} StrBuf;
static void Grow(StrBuf *B, size_t Need)
{
	//Return early if no need to grow
	int Required=B->Len+Need;
	if(Required<=B->Cap)
		return;

	//Grow to the appropriate size
	while(B->Cap<Required)
		B->Cap+=GrowSize;
	B->Str=realloc(B->Str, B->Cap);
}
static void AddCh(StrBuf *B, char c) //Note: We aren’t worrying about the string terminator here
{
	Grow(B, 1);
	B->Str[B->Len++]=c;
}
static void AddStr(StrBuf *B, const char *Str)
{
	size_t StrLen=strlen(Str);
	Grow(B, StrLen);
	memcpy(B->Str+B->Len, Str, StrLen);
	B->Len+=StrLen;
}
#define CmdCh(Arg1) AddCh(&Command, Arg1)
#define CmdStr(Arg1) AddStr(&Command, Arg1)

//Handle reading a file to an output stream
typedef struct
{
	FILE *File, *OutStream;
	const char *FilePath, *PipeType;
	size_t CheckSize;
	bool IsComplete;
	char Check[SeqSize];
} OutPipe;
bool OutPipe_Process(OutPipe* Pipe);
void OutPipe_Close(OutPipe* Pipe, bool DoNotRemove);
void OutPipe_Open(OutPipe* Pipe);

//Handle signals
volatile sig_atomic_t CaughtSig=0;
void SignalHandler(int Sig) { signal(CaughtSig=Sig, SIG_DFL); } // Reset to default after handling
int ReturnFromSigFunc()
{
	fprintf(
		stderr, "Interrupted by signal %d(%s)\n", CaughtSig,
		  CaughtSig==SIGINT ? "SIGINT"
#ifdef SIGHUP
		: CaughtSig==SIGHUP ? "SIGHUP"
#endif
		: CaughtSig==SIGTERM? "SIGTERM"
		: "Unknown signal"
	);
	return 104;
}
#define ReturnFromSig() if(CaughtSig) return ReturnFromSigFunc()

int main(int argc, const char *argv[])
{
	//Signal handling
	signal(SIGINT , SignalHandler); //Ctrl+C
	signal(SIGTERM, SignalHandler); //Kill (graceful)
#ifdef SIGHUP
	signal(SIGHUP , SignalHandler); //Terminal disconnect
#endif

	//Create and execute the command string
	StrBuf Command={0};
	CmdCh('"'); //Quote the entire string (windows is dumb)
	argv[0]=GitPath;
	for(int i=0; argv[i]; i++) //Add the quoted arguments
	{
		const char EscapeQuote='"';
		if(i!=0)
			CmdCh(' ');
		CmdCh(EscapeQuote);
		for(const char *P=argv[i]; *P; ++P) {
			if(*P==EscapeQuote)
				CmdCh('\\');
			CmdCh(*P);
		}
		CmdCh(EscapeQuote);
	}
	CmdStr(" 2>&1 > NUL"); //Ignore all output, as real output is written to a file
	CmdCh('"'); //End the entire string quote
	CmdCh(0); //Terminate the string
	ReturnFromSig();
	int RetCode=system(Command.Str);
	free(Command.Str);
	ReturnFromSig();

	//Prepare to read in the outputs
	OutPipe MainOut={0, stdout, FileTempPath, "Standard", 0, false, {0}};
	OutPipe ErrOut={0, stderr, FileTempPathErr, "Error", 0, false, {0}};
	OutPipe_Open(&MainOut);
	OutPipe_Open(&ErrOut);
	if(CaughtSig || !MainOut.File || !ErrOut.File)
	{
		OutPipe_Close(&MainOut, false);
		OutPipe_Close(&ErrOut, false);
		ReturnFromSig();
		return !MainOut.File ? 101 : 102;
	}

	//Process both stdout and stderr pipes
	clock_t LastUpdate=clock();
	while(1) {
		//If both are done, then complete the process
		if(MainOut.IsComplete && ErrOut.IsComplete)
			break;

		//Check for interruption via signal
		if(CaughtSig)
			break;

		//Process both pipes. If either had data, update the LastUpdate time
		bool MainWrote=OutPipe_Process(&MainOut), ErrWrote=OutPipe_Process(&ErrOut);
		if(MainWrote || ErrWrote) {
			LastUpdate=clock();
			continue;
		}

		//Exit if more than 10 seconds has passed since the last update
		if((clock()-LastUpdate)*1000L/CLOCKS_PER_SEC>10*1000) {
			fprintf(stderr, "%s\n", "Process has timed out (10 seconds)");
			RetCode=RetCode ?: 103;
			break;
		}

		//Sleep for 100 ms before trying to read the pipes again
		MilliSleep(100);
	}

	//Clean up and close out
	OutPipe_Close(&MainOut, !CaughtSig);
	OutPipe_Close(&ErrOut, !CaughtSig);
	ReturnFromSig();
	return RetCode;
}

//Read in and output up to ChunkCapacity bytes at a time from OutPipe (stdout and stderr). When the end of the read bytes equals the end sequence then we are done with the stream (do not output the end sequence)
bool OutPipe_Process(OutPipe* Pipe)
{
	//If already complete, nothing to do
	if(Pipe->IsComplete)
		return false;

	//Buffers
	const size_t ChunkCapacity=4096;
	char Chunk[ChunkCapacity];

	//Read in bytes. Return false if nothing read.
	int Bytes;
	if(!(Bytes=fread(Chunk, 1, ChunkCapacity, Pipe->File)))
		return false;

	//Write the current chunk
	int ChunkBytesToMoveToCheck=min(Bytes, SeqSize);
	fwrite(Chunk, 1, Bytes-ChunkBytesToMoveToCheck, Pipe->OutStream);
	fflush(Pipe->OutStream);

	//Shift Check bytes out to make room for bytes from Chunk. As bytes are shifted out, write them to stdout
	int ShiftCheckBytes=ChunkBytesToMoveToCheck-(SeqSize-Pipe->CheckSize);
	if(ShiftCheckBytes>0) {
		fwrite(Pipe->Check, 1, ShiftCheckBytes, Pipe->OutStream);
		memmove(Pipe->Check, Pipe->Check+ShiftCheckBytes, Pipe->CheckSize-=ShiftCheckBytes);
	}

	//Fill bytes from Chunk into Check
	memcpy(Pipe->Check+Pipe->CheckSize, Chunk+Bytes-ChunkBytesToMoveToCheck, ChunkBytesToMoveToCheck);
	Pipe->CheckSize+=ChunkBytesToMoveToCheck;

	//If the sequence doesn’t match, return that the pipe had data
	if(Pipe->CheckSize!=SeqSize || memcmp(Pipe->Check, EndSeq, SeqSize))
		return true;

	//Finish out the pipe
	Pipe->CheckSize=0;
	OutPipe_Close(Pipe, true);
	return true;
}

//Clean up and close out the pipe
void OutPipe_Close(OutPipe* Pipe, bool RemoveTempFile)
{
	if(Pipe->IsComplete)
		return;
	if(Pipe->CheckSize) //Output the remainder of the check buffer
		fwrite(Pipe->Check, 1, Pipe->CheckSize, Pipe->OutStream);
	fflush(Pipe->OutStream);
	if(Pipe->File)
		fclose(Pipe->File);
	if(RemoveTempFile)
		remove(Pipe->FilePath);
	Pipe->IsComplete=true;
}

//Open the file to pass through
void OutPipe_Open(OutPipe* Pipe)
{
	//Keep attempting to open the file until it has data
	FILE *TestOpen=NULL;
	for(int Attempts=0; Attempts<100; Attempts++) {
		MilliSleep(50);
		if(CaughtSig) return;
		if(!(TestOpen=fopen(Pipe->FilePath, "rb")))
			continue;
		fseek(TestOpen, 0, SEEK_END);
		if(ftell(TestOpen)>0) {
			fseek(TestOpen, 0, SEEK_SET);
			break;
		}
		fclose(TestOpen);
		TestOpen=NULL;
	}

	if(Pipe->File=TestOpen) //On success
		_setmode(_fileno(Pipe->OutStream), _O_BINARY); //Set the output stream to binary (nulls are used)
	else //On error
		fprintf(stderr, "Could not open %s output file: %s\n", Pipe->PipeType, Pipe->FilePath);
}