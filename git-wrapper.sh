#!/bin/bash
#Variables
GIT_PATH=/usr/bin/git

#Source code constants in git-launcher.c
          TMP_FILE=$(winepath -u 'c:\git.tmp'     ) #This must match FileTempPath
      TMP_FILE_ERR=$(winepath -u 'c:\git.tmp.err' ) #This must match FileTempPathErr
TMP_FILE_EXIT_CODE=$(winepath -u 'c:\git.tmp.exit') #This must match FileTempPathExitCode
          END_SEQ() { echo -ne '\0!\0!\0!\0!\0!'; } #This must match EndSeq (Cannot store in variable due to nulls)

#Truncate the temp file if it already exists. Needs to happen immediately so executable doesn’t try to access the wrong file
> "$TMP_FILE"
> "$TMP_FILE_ERR"
> "$TMP_FILE_EXIT_CODE"

#Handle version request. Need to append .windows.1 to the end for TortoiseGit.
if [[ "$1" == "version" || "$1" == "--version" ]]; then
    output=$("$GIT_PATH" "$@")
    exit_code=$?
    printf '%s\n' "${output}.windows.1" >> "$TMP_FILE" 2>> "$TMP_FILE_ERR"
    echo -n $exit_code >> "$TMP_FILE_EXIT_CODE"
    END_SEQ >> "$TMP_FILE"
    END_SEQ >> "$TMP_FILE_ERR"
    exit $exit_code
fi

#Change windows to linux paths
new_args=()
for arg in "$@"; do
    if [[ $arg =~ ^[a-zA-Z]:\\ ]]; then
        converted=$(winepath -u "$arg")
        new_args+=("$converted")
    else
        new_args+=("$arg")
    fi
done

#Action “diff-index” needs to have “update-index” run first or it doesn’t work
if [[ ${new_args[0]} == "diff-index" ]]; then
    "$GIT_PATH" update-index --refresh 2>&1 > /dev/null
fi

#Save the git command output to file
"$GIT_PATH" "${new_args[@]}" >> "$TMP_FILE" 2>> "$TMP_FILE_ERR"
exit_code=$?

#Logging
if [[ -v TGIT_LOG ]]; then
    TGIT_LOG_PATH=/tmp/tgit-wrapper.log
    echo `date +"%Y-%m-%d %H:%M:%S"` "[$exit_code]: ${new_args[@]}" >> "$TGIT_LOG_PATH"
    cat "$TMP_FILE" >> "$TGIT_LOG_PATH"
    if [ -s "$TMP_FILE_ERR" ]; then
        echo -e "\n---------------------ERR---------------------" >> "$TGIT_LOG_PATH"
        cat "$TMP_FILE_ERR" >> "$TGIT_LOG_PATH"
    fi
    echo -e "\n---------------------------------------------" >> "$TGIT_LOG_PATH"
fi

#Send finishing data to our executable
echo -n $exit_code >> "$TMP_FILE_EXIT_CODE"
END_SEQ >> "$TMP_FILE"
END_SEQ >> "$TMP_FILE_ERR"
exit $exit_code