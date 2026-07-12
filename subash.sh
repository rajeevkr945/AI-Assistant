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

echo "subash function loaded. Type 'subash <target>' to navigate."
