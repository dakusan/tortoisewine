//x86_64-w64-mingw32-gcc git-launcher.c -o git.exe -mconsole -s -Os -Wl,--gc-sections

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <unistd.h> //usleep
#include <fcntl.h> //_O_BINARY

//Set this
const char* GitPath="c:\\git-wrapper.exe"; //Do not try to escape this string

//No need to update these
const char* FileTempPath="C:\\git.tmp";
const int GrowSize=1024;
const char EndSeq[]={0, '!', 0, '!', 0, '!', 0, '!', 0, '!'}; //This sequence marks the end of the output

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

int main(int argc, const char *argv[])
{
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
	int RetCode=system(Command.Str);
	free(Command.Str);

	//Prepare to read in the file
	MilliSleep(100); //Allow the file time to get truncated
	FILE *File=fopen(FileTempPath, "rb");
	if(!File)
		return RetCode ?: 101;
	_setmode(_fileno(stdout), _O_BINARY);
	const size_t ChunkCapacity=4096, SeqSize=sizeof(EndSeq);
	char Check[SeqSize], Chunk[ChunkCapacity];
	size_t CheckSize=0, Bytes;
	clock_t LastUpdate=clock();

	//Read in and output up to ChunkCapacity bytes at a time. When the end of the read bytes equals the end sequence then exit (do not output the end sequence)
	while(1) {
		//Read in bytes
		if(!(Bytes=fread(Chunk, 1, ChunkCapacity, File))) {
			//Exit if more than 10 seconds has passed since the last update
			if((clock()-LastUpdate)*1000L/CLOCKS_PER_SEC>10*1000) {
				fwrite(Check, 1, CheckSize, stdout); //Output the remainder of the check buffer
				RetCode=RetCode ?: 101;
				break;
			}

			//Sleep for 100 ms
			MilliSleep(100);
			continue;
		}
		LastUpdate=clock(); //Update the LastUpdate time

		//Write the current chunk
		int ChunkBytesToMoveToCheck=min(Bytes, SeqSize);
		fwrite(Chunk, 1, Bytes-ChunkBytesToMoveToCheck, stdout);
		fflush(stdout);

		//Shift Check bytes out to make room for bytes from Chunk. As bytes are shifted out, write them to stdout
		int ShiftCheckBytes=ChunkBytesToMoveToCheck-(SeqSize-CheckSize);
		if(ShiftCheckBytes>0) {
			fwrite(Check, 1, ShiftCheckBytes, stdout);
			memmove(Check, Check+ShiftCheckBytes, CheckSize-=ShiftCheckBytes);
		}

		//Fill bytes from Chunk into Check
		memcpy(Check+CheckSize, Chunk+Bytes-ChunkBytesToMoveToCheck, ChunkBytesToMoveToCheck);
		CheckSize+=ChunkBytesToMoveToCheck;

		//If the CheckSize is large enough and matches our EndSeq, then exit here
		if(CheckSize==SeqSize && !memcmp(Check, EndSeq, SeqSize))
			break;
	}

	//Clean up and close out
	fflush(stdout);
	fclose(File);
	remove(FileTempPath);

	return RetCode;
}