# Intro
This allows the use of **TortoiseGit** in **Linux** via **Wine**. It also allows use of the native `/usr/bin/git` inside Linux with TortoiseGit.

Run commands in your Git repo directory via <code>tgit **COMMAND_NAME** .</code> .

> [!IMPORTANT]
> For all wine commands make sure to have the environment variable `WINEPREFIX=~/.wine-tortoise` set.<br>
> You can set this in a session via `export WINEPREFIX=~/.wine-tortoise`.<br>
> The `tgit` command does this for you automatically.

# Install
- Run install.sh
- You should now be good to go to run TortoiseGit commands via `tgit`

# Integration with RabbitVCS
- I recommend using [RabbitVCS](http://rabbitvcs.org/) to get status icons in your repository’s directories.
- If you have RabbitVCS installed, this will install an extension over it that will reroute the following **Nemo** dropdown selections to TortoiseGit:
  - `diff`, `commit`, `log`, `push`, `update`→`fetch`
  - Adding more is very simplistic. I would just need testers to test and give confirmations for me.
- The logic inside the extension is super sketchy and may need to be fixed up.

# TODO
- Right now it’s only set to install with **Nemo**, but if anyone is willing to test with **Nautilus**, that could be easily added.
- This is a work in progress at the moment, and some commands may not work properly, but most should. The `tgit` command always expects the first parameter to be the command and the second parameter to be a wine-path. That may need to change. [Here is a full list of TortoiseGit commands](https://tortoisegit.org/docs/tortoisegit/tgit-automation.html).
- For now, I have everything just being created in `C:\` for simplicity. Adding code to change that directory would be pretty simple, if needed.

# Why this is needed
The native Linux `git` executable has some issues with TortoiseGit.
1. It needs to report as a Windows version of Git with `.windows.1` at the end of the version. So there has to be pass through wrappers.
1. The git.exe needs to be an actual executable. Attempting to just use bash scripts always fails.
1. `git diff-index` required a `git update-index` first.
1. There are issues with Wine not listening to the process long enough to get all the output and there are file syncing issues across file systems. For this reason, all Git output is routed through temporary files with a custom EOF terminator.
	> Originally, the only way I found to get the output for `git.exe diff-index --raw HEAD --numstat -C50% -M50% -z --` to TortoiseGit required 3 things in the compiled executable: A `git update-index`, a redirect to an output file, and a sub-second timeout after the command. There was *absolutely no other* way to get it to work. Trust me, I tried.

I realized after a lot of painful C/C++ work that it was best to handle the logic in bash and just use the C executable to return the rerouted output and the return code. The original C++ version I was creating is still available on the first commit to this repository.