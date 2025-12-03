This allows the use of TortoiseGit in Linux via Wine. It also allows use of the native `/usr/bin/git` inside Linux with TortoiseGit.

This is a work in progress at the moment, but the commands `commit`, `log` and `settings` (end everything under them) are currently fully working. Run commands in your Git repo directory via <code>tgit **COMMAND_NAME** .</code> .

For all wine commands make sure to have the environment variable `WINEPREFIX=~/.wine-tortoise`. You can set this in a session via `export WINEPREFIX=~/.wine-tortoise`.

I recommend using [RabbitVCS](http://rabbitvcs.org/) to get status icons in your repository’s directories. I plan on rerouting RabbitVCS shell commands to automatically run TortoiseGit.

For now, I have everything just being put in `C:\` for simplicity.

# Install
- Run install.sh
- You should be good to go now to run TortoiseGit commands via `tgit`.

# Why this is needed
The native Linux `git` executable has some issues with TortoiseGit.
1. It needs to report as a Windows version of Git with `.windows.1` at the end of the version. So there has to be pass through wrappers.
1. The git.exe needs to be an actual executable. Attempting to just use bash scripts always fails.
1. `git diff-index` required a `git update-index` first.
1. There are issues with Wine not listening to the process long enough to get all the output and there are file syncing issues across file systems. For this reason, all Git output is routed through a temporary file with a custom EOF terminator.
	> Originally, the only way I found to get the output for `git.exe diff-index --raw HEAD --numstat -C50% -M50% -z --` to TortoiseGit required 3 things in the compiled executable: A `git update-index`, a redirect to an output file, and a sub-second timeout after the command. There was *absolutely no other* way to get it to work. Trust me, I tried.

I realized after a lot of painful C/C++ work that it was best to handle the logic in bash and just use the C executable to return the rerouted output and the return code. The original C++ version I was creating is still available on the first commit to this repository.