# TortoiseGit on Linux via Wine

This project enables **TortoiseGit** on **Linux** using **Wine**, integrated with the native `/usr/bin/git`.

Run Git repo commands with <code>tgit <**COMMAND_NAME**> <**PATH**></code>.

> [!IMPORTANT]
> Always set `WINEPREFIX=~/.wine-tortoise` for Wine commands (e.g., `export WINEPREFIX=~/.wine-tortoise`). `tgit` handles this automatically.

## Installation

1. Run `./install.sh`.
1. Install TortoiseGit from [tortoisegit.org](https://tortoisegit.org/download/) ([v2.17.0.0](https://download.tortoisegit.org/tgit/2.17.0.0/) recommended; 2.18+ will fail to install — Let me know if you find a way around the Windows version check).
1. In TortoiseGit setup, set **Git.exe Path** to `C:\`.

## RabbitVCS Integration
- I recommend using [RabbitVCS](http://rabbitvcs.org/) for repo status icons.
- If RabbitVCS is installed, the installer adds a **Nemo** extension rerouting dropdown actions to TortoiseGit:
   - `diff`, `commit`, `log`, `push`, `update` → `fetch`
   - Adding/extending commands is straightforward; testers welcome for validation and patches.
- Note: Extension logic is experimental; refinements very likely needed.

## Debugging
Add `export TGIT_LOG=1` to `tgit` (below shebang) and/or `.bashrc` to log to `/tmp/tgit-command.log` and `/tmp/tgit-wrapper.log`.

## TODO
- Add **Nautilus** support (*simple to do*; **Nemo**-only for now; testers needed).
- Refine `tgit` args (currently expects `<command> <wine-path>`); see [full list of TortoiseGit commands](https://tortoisegit.org/docs/tortoisegit/tgit-automation.html).
- Option to customize install dir (*simple to do*; currently `C:\` for simplicity).
- Work in progress: Some commands may not work properly, but most should.

## Why This Exists
Native Linux `git` has TortoiseGit incompatibilities:
1. Must report as Windows Git (vX.Y.Z.windows.1); requires wrappers.
1. `git.exe` must be a true executable (script-only fails).
1. `git diff-index` needs prior `git update-index`.
1. Wine output/timeout/file-sync issues; outputs routed via temp files with custom EOF terminator.
   > Originally, `git diff-index --raw HEAD --numstat -C50% -M50% -z --` for TortoiseGit required a compiled exe with `update-index`, output redirect via file, and sub-second timeout. There was *absolutely no other* way to get it to work. Trust me, I tried.

After painful C/C++ work, I realized it best for Bash to handle core logic and for C exe to manage output/return codes. Original C++ version available in first commit.