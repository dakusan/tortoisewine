//x86_64-w64-mingw32-g++ git-launcher.cpp -o git.exe -mconsole -static-libgcc -static-libstdc++ -s -Os -Wl,--gc-sections

#include <process.h>
#include <unistd.h>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <iostream>
#include <fstream>

using namespace std;

//Set this
const char* GitPath="c:\\git-linux.exe"; //You must include the escape slash before spaces

//Leave these be
const char* TmpFileName="git.result";
const char* ForceTempPath="C:\\git.result";

string GetGitResultSafe(char* const* argv, int& ProcRet, bool UseInPlace=false);
int OutputGitResultSafe(char* const* argv, bool UseInPlace=false);
string& AddArg(const char* Argument, string& Command, bool SkipSpace=false);

int main_real(int argc, char* const* argv)
{
	//Handle --version call
	if(argc>1 && (!strcmp(argv[1], "version") || !strcmp(argv[1], "--version"))) {
		//Get the git version
		int ProcRet;
		string GitVersion=GetGitResultSafe(argv, ProcRet);
		if(GitVersion.length()<=0)
			return ProcRet ? ProcRet : -1;

		//Output the git version minus the newline
		if(!ProcRet) {
			if(GitVersion.length()>0)
				fwrite(GitVersion.c_str(), 1, GitVersion.length()-1, stdout);
			printf(".windows.1");
			fflush(stdout);
		}

		//Return the result
		return ProcRet;
	}

	//Only need modifications on diff-index
	if(!(argc>1 && !strcmp(argv[1], "diff-index"))) {
		_execv(argv[0], argv);
		perror("execv");
		return -1;
	}

	//Run an index update
	string UpdateIndex;
	system(AddArg(argv[0], UpdateIndex, true).append(" update-index --refresh 2>&1 > NUL").c_str());

	//This needs to be ran through the safe function as the only method to get the output was: Write output to file and pause for 100ms
	return OutputGitResultSafe(argv, false);
}

int main(int argc, const char *argv[])
{
	argv[0]=GitPath;
	int ProcRet=main_real(argc, (char* const*)argv);
	return ProcRet==-1 ? 100 : ProcRet;
}

//Run a process and send output to TmpPath using spawnv
//This seems to work when run from the command line, but not when running from TortoiseGit
int ProcessRedirect_InPlace(char* const* argv, string& TmpPath)
{
	//Replace stdout with a file
	class ReplaceStdOut
	{
		private:
			int FD=-1, SavedOut=-1;
		public:
		ReplaceStdOut(string& TmpPath)
		{
			if((FD=open(TmpPath.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666))==-1)
				return;
			if((SavedOut=dup(STDOUT_FILENO))==-1)
				return;
			dup2(FD, STDOUT_FILENO);
		}
		~ReplaceStdOut()
		{
			if(FD>=0)
				close(FD);
			if(SavedOut!=-1) {
				dup2(SavedOut, STDOUT_FILENO);
				close(SavedOut);
			}
		}
		bool Success() { return SavedOut!=-1; }
	};
	ReplaceStdOut Rep(TmpPath);
	if(!Rep.Success())
		return -1;

	//Execute the process
	return _spawnv(_P_WAIT, argv[0], (const char* const*)argv);
}

//Run a process and send output to TmpPath using system
int ProcessRedirect_System(char* const* argv, string& TmpPath)
{
	//Create the command string
	string Command=argv[0]; //It doesn’t like having this quoted >.<
	for(int i=1; argv[i]; i++)
		AddArg(argv[i], Command, i==0);

	//Add the output redirect to file
	Command.append(" >");
	AddArg(TmpPath.c_str(), Command);

	//Execute the process
	return system(Command.c_str());
}

int PrepGitResultSafe(char* const* argv, string& TmpPath, bool UseInPlace)
{
	//Get the temp path where we’ll store the result
	char* WinTmpPath=getenv("TEMP");
	TmpPath=(WinTmpPath ? string(WinTmpPath)+"\\"+TmpFileName : ForceTempPath);

	//Run the attempt
	int ProcRet=(UseInPlace ? ProcessRedirect_InPlace(argv, TmpPath) : ProcessRedirect_System(argv, TmpPath));
	if(!ProcRet)
		usleep(100*1000); //Wait 100ms for the file to be accessible (Wine syncing issue)
	else
		remove(TmpPath.c_str());
	return ProcRet;
}

//Output the result to stdout
int OutputGitResultSafe(char* const* argv, bool UseInPlace)
{
	//Execute the command
	string TmpPath;
	int ProcRet=PrepGitResultSafe(argv, TmpPath, UseInPlace);
	if(ProcRet)
		return ProcRet;

	//Read back the result
	{
		ifstream file(TmpPath.c_str(), ios::binary);
		_setmode(_fileno(stdout), _O_BINARY);
		cout << file.rdbuf();
		fflush(stdout);
	}

	//Delete the file and return the result
	remove(TmpPath.c_str());
	return ProcRet;
}

//Get the result as a string
string GetGitResultSafe(char* const* argv, int& ProcRet, bool UseInPlace)
{
	//Execute the command
	string TmpPath;
	if((ProcRet=PrepGitResultSafe(argv, TmpPath, UseInPlace)))
		return "";

	//Read back the result
	string Output;
	{
		ifstream ifs(TmpPath, ios::binary);
		Output=string((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
	}

	//Delete the file and return the result
	if(Output.length()<=0)
		ProcRet=-1;
	return Output;
}

//Add an argument to the command
string& AddArg(const char* Argument, string& Command, bool SkipSpace)
{
	const char EscapeQuote='"';
	if(!SkipSpace)
		Command.push_back(' ');
	Command.push_back(EscapeQuote);
	for(char c : string(Argument)) {
		if(c==EscapeQuote)
			Command.push_back('\\');
		Command.push_back(c);
	}
	Command.push_back(EscapeQuote);
	return Command;
}
