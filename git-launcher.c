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
const char *GitPath="c:\\git-wrapper.exe"; //Do not escape this string for the shell

//Numeric constants that define how parts of the program work
const size_t StrBufGrowSize				=1024;
const size_t ReadFileBufferSize			=4096;
const long int NoFileActivityMSTimeout	=10*1000;
const int MSTimeBetweenFileReads		=100;
const int InitialGetFileData_NumAttempts=100;
const int InitialGetFileData_MSInterval	=50;
enum ErrorCodes
{
	EC_SUCCESS							=0  ,
	EC_FAILED_READ_MAIN_OUT				=101,
	EC_FAILED_READ_ERR_OUT				=102,
	EC_TIMEOUT							=103,
	EC_GOT_SIGNAL						=104,
	EC_READ_EXIT_CODE_FAIL				=105,
};

//Generic constants
enum
{
	MAX_CHARS_IN_ERROR_CODE				=3,
	CHAR_SIZE							=sizeof(char),
	MILLI_SECONDS_TO_MICRO_SECONDS		=1000,
	ZERO								=0,
};
const long int SECONDS_TO_MILLI_SECONDS	=1000L;
const char NULL_TERM					='\0';

//These must match the constants in git-wrapper.sh
const char *FileTempPath="C:\\git.tmp", *FileTempPathErr="C:\\git.tmp.err", *FileTempPathExitCode="C:\\git.tmp.exit";
const char EndSeq[]={NULL_TERM, '!', NULL_TERM, '!', NULL_TERM, '!', NULL_TERM, '!', NULL_TERM, '!'}; //This sequence marks the end of the output
enum { SeqSize=sizeof(EndSeq) }; //Using enum for compile time constant

//Simple use functions
void MilliSleep(int Milliseconds) { usleep(Milliseconds*MILLI_SECONDS_TO_MICRO_SECONDS); }
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
		B->Cap+=StrBufGrowSize;
	B->Str=realloc(B->Str, B->Cap);
}
static void AddCh(StrBuf *B, char c) //Note: We aren’t worrying about the string terminator here
{
	Grow(B, CHAR_SIZE);
	B->Str[B->Len++]=c;
}
static void AddStr(StrBuf *B, const char *Str)
{
	size_t StrLen=strlen(Str);
	Grow(B, StrLen);
	memcpy(B->Str+B->Len, Str, StrLen);
	B->Len+=StrLen;
}
#define CmdCh(AddArg) AddCh(&Command, AddArg)
#define CmdStr(AddArg) AddStr(&Command, AddArg)

//Handle reading a file to an output stream
typedef struct
{
	FILE *File, *OutStream;
	const char *FilePath, *PipeType;
	size_t CheckSize;
	bool IsComplete;
	char Check[SeqSize];
} OutPipe;
bool OutPipe_Process(OutPipe *Pipe);
void OutPipe_Close(OutPipe *Pipe, bool DoNotRemove);
void OutPipe_Open(OutPipe *Pipe);

//Handle signals
volatile sig_atomic_t CaughtSig=ZERO;
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
	return EC_GOT_SIGNAL;
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
	StrBuf Command={NULL, ZERO, ZERO};
	CmdCh('"'); //Quote the entire string (windows is dumb)
	argv[ZERO]=GitPath;
	for(int i=ZERO; argv[i]; i++) //Add the quoted arguments
	{
		const char EscapeQuote='"';
		if(i)
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
	CmdCh(NULL_TERM); //Terminate the string
	ReturnFromSig();
	int RetCode=system(Command.Str);
	free(Command.Str);
	ReturnFromSig();

	//Prepare to read in the outputs
	OutPipe MainOut={NULL, stdout, FileTempPath, "Standard", ZERO, false, {NULL_TERM}};
	OutPipe ErrOut={NULL, stderr, FileTempPathErr, "Error", ZERO, false, {NULL_TERM}};
	OutPipe_Open(&MainOut);
	OutPipe_Open(&ErrOut);
	if(CaughtSig || !MainOut.File || !ErrOut.File)
	{
		OutPipe_Close(&MainOut, false);
		OutPipe_Close(&ErrOut, false);
		ReturnFromSig();
		return !MainOut.File ? EC_FAILED_READ_MAIN_OUT : EC_FAILED_READ_ERR_OUT;
	}

	//Process both stdout and stderr pipes
	clock_t LastUpdate=clock();
	while(true) {
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

		//Exit if more than $NoFileActivityMSTimeout milliseconds has passed since the last update
		if((clock()-LastUpdate)*SECONDS_TO_MILLI_SECONDS/CLOCKS_PER_SEC>NoFileActivityMSTimeout) {
			fprintf(stderr, "Process has timed out (%.3f+ seconds)\n", (float)NoFileActivityMSTimeout/(float)SECONDS_TO_MILLI_SECONDS);
			RetCode=RetCode ?: EC_TIMEOUT;
			break;
		}

		//Sleep for $MSTimeBetweenFileReads ms before trying to read the pipes again
		MilliSleep(MSTimeBetweenFileReads);
	}

	//Clean up and close out
	OutPipe_Close(&MainOut, !CaughtSig);
	OutPipe_Close(&ErrOut, !CaughtSig);
	ReturnFromSig();
	if(RetCode!=EC_SUCCESS)
		return RetCode;

	//Get the real return code
	char ExitCodeBuff[MAX_CHARS_IN_ERROR_CODE+CHAR_SIZE]={NULL_TERM};
	FILE *FileExitCode=fopen(FileTempPathExitCode, "r");
	if(!FileExitCode)
		return EC_READ_EXIT_CODE_FAIL;
	int AmountRead=fread(ExitCodeBuff, CHAR_SIZE, MAX_CHARS_IN_ERROR_CODE, FileExitCode);
	fclose(FileExitCode);
	remove(FileTempPathExitCode);
	if(!AmountRead)
		return EC_READ_EXIT_CODE_FAIL;
	for(int i=ZERO; i<AmountRead; i++)
		if(ExitCodeBuff[i]<'0' || ExitCodeBuff[i]>'9')
			return EC_READ_EXIT_CODE_FAIL;
	return atoi(ExitCodeBuff); //Allows error codes up to 999
}

//Read in and output up to $ReadFileBufferSize bytes at a time from OutPipe (stdout and stderr). When the end of the read bytes equals the end sequence then we are done with the stream (do not output the end sequence)
bool OutPipe_Process(OutPipe *Pipe)
{
	//If already complete, nothing to do
	if(Pipe->IsComplete)
		return false;

	//Read in bytes. Return false if nothing read.
	char Chunk[ReadFileBufferSize]; //Buffer
	int Bytes;
	if(!(Bytes=fread(Chunk, CHAR_SIZE, ReadFileBufferSize, Pipe->File)))
		return false;

	//Write the current chunk
	int ChunkBytesToMoveToCheck=min(Bytes, SeqSize);
	fwrite(Chunk, CHAR_SIZE, Bytes-ChunkBytesToMoveToCheck, Pipe->OutStream);
	fflush(Pipe->OutStream);

	//Shift Check bytes out to make room for bytes from Chunk. As bytes are shifted out, write them to stdout
	int ShiftCheckBytes=ChunkBytesToMoveToCheck-(SeqSize-Pipe->CheckSize);
	if(ShiftCheckBytes>ZERO) {
		fwrite(Pipe->Check, CHAR_SIZE, ShiftCheckBytes, Pipe->OutStream);
		memmove(Pipe->Check, Pipe->Check+ShiftCheckBytes, Pipe->CheckSize-=ShiftCheckBytes);
	}

	//Fill bytes from Chunk into Check
	memcpy(Pipe->Check+Pipe->CheckSize, Chunk+Bytes-ChunkBytesToMoveToCheck, ChunkBytesToMoveToCheck);
	Pipe->CheckSize+=ChunkBytesToMoveToCheck;

	//If the sequence doesn’t match, return that the pipe had data
	if(Pipe->CheckSize!=SeqSize || memcmp(Pipe->Check, EndSeq, SeqSize))
		return true;

	//Finish out the pipe
	Pipe->CheckSize=ZERO;
	OutPipe_Close(Pipe, true);
	return true;
}

//Clean up and close out the pipe
void OutPipe_Close(OutPipe *Pipe, bool RemoveTempFile)
{
	if(Pipe->IsComplete)
		return;
	if(Pipe->CheckSize) //Output the remainder of the check buffer
		fwrite(Pipe->Check, CHAR_SIZE, Pipe->CheckSize, Pipe->OutStream);
	fflush(Pipe->OutStream);
	if(Pipe->File)
		fclose(Pipe->File);
	if(RemoveTempFile)
		remove(Pipe->FilePath);
	Pipe->IsComplete=true;
}

//Open the file to pass through
void OutPipe_Open(OutPipe *Pipe)
{
	//Keep attempting to open the file until it has data
	FILE *TestOpen=NULL;
	for(int Attempts=ZERO; Attempts<InitialGetFileData_NumAttempts; Attempts++) {
		MilliSleep(InitialGetFileData_MSInterval);
		if(CaughtSig)
			return;
		if(!(TestOpen=fopen(Pipe->FilePath, "rb")))
			continue;
		fseek(TestOpen, ZERO, SEEK_END);
		if(ftell(TestOpen)>ZERO) {
			fseek(TestOpen, ZERO, SEEK_SET);
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