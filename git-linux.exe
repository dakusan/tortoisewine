#!/bin/bash
#Variables
GIT_PATH=/usr/bin/git
TMP_FILE=$(winepath -u 'c:\git.tmp') #This must match FileTempPath in git-launcher.c
END_SEQ() { echo -ne '\0!\0!\0!\0!\0!' >> "$TMP_FILE"; } #This must match EndSeq in git-launcher.c (Cannot store in variable due to nulls)

#Truncate the temp file if it already exists. Needs to happen immediately so executable doesn’t try to access the wrong file
echo -n "" > "$TMP_FILE"

#Handle version request. Need to append .windows.1 to the end for TortoiseGit.
if [[ "$1" == "version" || "$1" == "--version" ]]; then
    output=$("$GIT_PATH" "$@")
    exit_code=$?
    printf '%s\n' "${output}.windows.1" >> "$TMP_FILE"
    END_SEQ
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

#Save the git command output to file followed by the ending sequence
"$GIT_PATH" "${new_args[@]}" >> "$TMP_FILE"
exit_code=$?
END_SEQ
exit $exit_code