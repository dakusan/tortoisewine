This allows the use of TortoiseGit in Linux via Wine. It also allows use of the native `/usr/bin/git` inside Linux with TortoiseGit.

This is a work in progress at the moment, but the commands `commit`, `log` and `settings` are currently fully working. Run commands in your Git repo directory via <code>tgit **COMMAND_NAME** .</code> .

For all wine commands make sure to have the environment variable `WINEPREFIX=~/.wine-tortoise`. You can set this in a session via `export WINEPREFIX=~/.wine-tortoise`.

I recommend using [RabbitVCS](http://rabbitvcs.org/) to get status icons in your repository’s directories. I plan on rerouting RabbitVCS shell commands to automatically run TortoiseGit.

# Install
> [!IMPORTANT]
> Run these commands starting inside this repository’s root directory.<br>
> **Always** make sure `WINEPREFIX` environment variable is set
1. Create the wine sandbox
	```
	#Set session wine prefix folder
	export WINEPREFIX=~/.wine-tortoise

	#Run any wine command to create your new wine sandbox
	wine dir

	#Set the wine theme to None, as other themes have issues with the log window
	wine reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ThemeManager" /v ThemeActive /t REG_SZ /d "0" /f
	```
1. Symlink the project files
	```
	#Copy source code to C:\
	ln -s "`realpath git-launcher.c`" "$WINEPREFIX/drive_c/"

	#Copy the git wrapper to C:\
	ln -s "`realpath git-wrapper.sh`" "$WINEPREFIX/drive_c/git-wrapper.exe"

	#Copy the tgit command to the bin folder
	sudo bash -c 'cp tgit /usr/bin/ && chown root:root /usr/bin/tgit'
	```
1. Compile the `git.exe` executable
	```
	#Install compiler
	sudo apt install mingw-w64

	#Compile the file
	cd "$WINEPREFIX/drive_c"
	x86_64-w64-mingw32-gcc git-launcher.c -o git.exe -mconsole -s -Os -Wl,--gc-sections
	```
1. Prepare TortoiseGit
	1. Install [TortoiseGit 2.17](https://download.tortoisegit.org/tgit/2.17.0.0/) (2.18 refuses to install)
		1. Run on the downloaded file: `wine TortoiseGit-2.17.0.2-64bit.msi`
		1. During install:
			1. Choose SSH Client > OpenSSH
			1. Git.exe Path: `C:`

You should be good to go now to run TortoiseGit commands via `tgit`.

# Why this is needed
The native Linux `git` executable has some issues with TortoiseGit.
1. It needs to report as a Windows version of Git with `.windows.1` at the end of the version. So there has to be pass through wrappers.
1. The git.exe needs to be an actual executable. Attempting to just use bash scripts always fails.
1. `git diff-index` required a `git update-index` first.
1. There are issues with Wine not listening to the process long enough to get all the output and there are file syncing issues across file systens. For this reason, all Git output is routed through a temporary file with a custom EOF terminator.
	> Originally, the only way I found to get the output for `git.exe diff-index --raw HEAD --numstat -C50% -M50% -z --` to TortoiseGit required 3 things in the compiled exeutable: A `git update-index`, a redirect to an output file, and a sub-second timeout after the command. There was *absolutely no other* way to get it to work. Trust me, I tried.

I realized after a lot of painful C/C++ work that it was best to handle the logic in bash and just use the C executable to return the rerouted output and the return code. The original C++ version I was creating is still available on the first commit to this repository.