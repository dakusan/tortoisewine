#------------------------------------Variables-----------------------------------
export WINEPREFIX=~/.wine-tortoise

INSTALL_LOCATION="$WINEPREFIX/drive_c" #This cannot be changed at the moment

#--------------------------------Helper functions--------------------------------
RED=31
GREEN=32
print_color() {
	[ -t 1 ] && echo -ne "\e[$2m";
	echo -n "$1"
	[ -t 1 ] && echo -ne "\e[0m"
	[ -z "${3+x}" ] && echo #Add the newline if no third parameter
}

confirm() {
	print_color "$1 (y/n) " $GREEN 1
	read -p "" -n 1 -r;
	[[ $REPLY =~ ^[Yy]$ ]]
}

error_exit() {
	print_color "$1" $RED
	exit ${2:-1}
}
show_step()     { print_color "$1" $GREEN; }
error_handler() { error_exit "Command failed: $BASH_COMMAND" $?; }
trap 'error_handler' ERR

#----------------------------Create the wine sandbox-----------------------------

show_step "Running a wine command to create new wine sandbox"
wine hostname 2>&1 > /dev/null

show_step "Setting the wine theme to None, as other themes have issues with the log window"
wine reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ThemeManager" /v ThemeActive /t REG_SZ /d "0" /f

#---------------------------Compile and copy executable--------------------------
#Install MinGW
if ! apt list --installed 2>/dev/null | grep -q '^mingw-w64/'; then
	confirm "Install compiler MinGW? [Required]" || error_exit "Mingw required to compile" 101
	show_step "Installing MinGW"
	sudo apt install mingw-w64 -y
fi

show_step "Compiling the executable"
[[ -f git-launcher.c ]] || error_exit "Missing git-launcher.c" 102
x86_64-w64-mingw32-gcc git-launcher.c -o git.exe -mconsole -s -Os -Wl,--gc-sections

#Copy to wine
show_step "Copying compiled executable to wine"
cp -f git.exe "$INSTALL_LOCATION/"
show_step "Setting execute permission"
chmod +x "$INSTALL_LOCATION/git.exe"

#-------------------------Install the other project files------------------------
#Copy the git wrapper to /usr/bin
GIT_WRAPPER_SH=git-wrapper.sh
GIT_WRAPPER_SH_PATH="/usr/bin/$GIT_WRAPPER_SH"
show_step "Installing $GIT_WRAPPER_SH command to $GIT_WRAPPER_SH_PATH"
[[ -f git-wrapper.sh ]] || error_exit "Missing git-wrapper.sh" 103
sudo bash -c "cp '$GIT_WRAPPER_SH' '$GIT_WRAPPER_SH_PATH' && chmod 755 '$GIT_WRAPPER_SH_PATH' && chown root:root '$GIT_WRAPPER_SH_PATH'"

#Link the git wrapper to $GIT_WRAPPER_SH
GIT_WRAPPER_EXE=git-wrapper.exe
GIT_WRAPPER_EXE_PATH="$INSTALL_LOCATION/$GIT_WRAPPER_EXE"
if ! [[ -L "$GIT_WRAPPER_EXE_PATH" && "$(readlink -f "$GIT_WRAPPER_EXE_PATH")" == "$GIT_WRAPPER_SH_PATH" ]]; then
	show_step "Installing $GIT_WRAPPER_EXE to wine"
	[[ -e "$GIT_WRAPPER_EXE_PATH" ]] && rm -f "$GIT_WRAPPER_EXE_PATH"
	ln -s "$GIT_WRAPPER_SH_PATH" "$GIT_WRAPPER_EXE_PATH"
fi

show_step "Copy the tgit command to /usr/bin"
sudo bash -c 'cp tgit /usr/bin/ && chmod 755 /usr/bin/tgit && chown root:root /usr/bin/tgit'

#------------------------Installing RabbitVCS Nemo hooks-------------------------
RAB_EXT_DIR="$HOME/.local/share/nemo-python/extensions"
RABBIT_VCS=RabbitVCS.py
RABBIT_VCS_PATH="$RAB_EXT_DIR/$RABBIT_VCS"
RABBIT_TO_TGIT=rabbit_to_tgit.py
RABBIT_TO_TGIT_PATH="$RAB_EXT_DIR/$RABBIT_TO_TGIT"
RABBIT_TO_TGIT_IMPORT="import rabbit_to_tgit"
if [[ ! -f "$RABBIT_VCS_PATH" ]]; then
	print_color "RabbitVCS for Nemo not detected. Not installing extension" $RED
else
	show_step "Installing RabbitVCS for Nemo extension"
	[[ -f "$RABBIT_TO_TGIT" ]] || error_exit "Missing $RABBIT_TO_TGIT" 104
	cp -f "$RABBIT_TO_TGIT" "$RAB_EXT_DIR/"
	if ! grep -q "^$RABBIT_TO_TGIT_IMPORT" "$RABBIT_VCS_PATH"; then
		show_step "Hooking into $RABBIT_VCS"
		last_import=$(grep -n '^import ' "$RABBIT_VCS_PATH" | tail -1 | cut -d: -f1)
		[[ -n "$last_import" ]] && sed -i "${last_import}a ${RABBIT_TO_TGIT_IMPORT}" "$RABBIT_VCS_PATH" || echo -e "\n$RABBIT_TO_TGIT_IMPORT" >> "$RABBIT_VCS_PATH"
	fi
fi

#-------------------------------Final instructions-------------------------------
show_step "Install complete. Make sure TortoiseGit is installed from https://download.tortoisegit.org/tgit/2.17.0.0/"
show_step "During setup, set “Git.exe Path” to “C:”"