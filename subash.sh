# subash Bash Wrapper
# Source this file to enable the 'subash' command: source ./subash.sh

# Resolve the absolute path of the directory containing this script at sourcing time
SUBASH_BIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

subash() {
    # Clear out any stale IPC file
    rm -f /tmp/subash_action
    
    # Run the compiled binary, passing all parameters
    "$SUBASH_BIN_DIR/subash-bin" "$@"
    
    # Handle the IPC action for directory switching
    if [ -f /tmp/subash_action ]; then
        local action
        action=$(cat /tmp/subash_action)
        rm -f /tmp/subash_action
        
        # Execute the action (e.g. cd /path/to/project)
        eval "$action"
    fi
}

_subash_complete() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local prev="${COMP_WORDS[COMP_CWORD-1]}"
    local conf="$HOME/.subash.conf"
    if [ ! -f "$conf" ]; then
        return 0
    fi

    if [ "$COMP_CWORD" -eq 1 ]; then
        # List all projects
        local projects=()
        while IFS= read -r line; do
            projects+=("$line")
        done < <(awk '
            /^[ \t]*project[ \t]*=/ {
                sub(/^[ \t]*project[ \t]*=[ \t]*/, "");
                sub(/[ \t]*[#;].*$/, "");
                print $0;
            }
        ' "$conf")

        COMPREPLY=()
        for proj in "${projects[@]}"; do
            if [[ "$proj" == "$cur"* ]]; then
                COMPREPLY+=( "$(printf '%q' "$proj")" )
            fi
        done

    elif [ "$COMP_CWORD" -eq 2 ]; then
        # List all tasks under the selected project ($prev)
        local tasks=()
        while IFS= read -r line; do
            tasks+=("$line")
        done < <(awk -v target_proj="$prev" '
            BEGIN { in_target = 0; }
            /^[ \t]*project[ \t]*=/ {
                sub(/^[ \t]*project[ \t]*=[ \t]*/, "");
                sub(/[ \t]*[#;].*$/, "");
                if ($0 == target_proj) {
                    in_target = 1;
                } else {
                    in_target = 0;
                }
            }
            /^[ \t]*(product|task)[ \t]*=/ {
                if (in_target) {
                    sub(/^[ \t]*(product|task)[ \t]*=[ \t]*/, "");
                    sub(/[ \t]*[#;].*$/, "");
                    print $0;
                }
            }
        ' "$conf")

        COMPREPLY=()
        for task in "${tasks[@]}"; do
            if [[ "$task" == "$cur"* ]]; then
                COMPREPLY+=( "$(printf '%q' "$task")" )
            fi
        done
    fi

    return 0
}

complete -F _subash_complete subash

echo "subash function loaded. Type 'subash <target>' to navigate."
