#!/bin/bash

# dev-flow Bash Wrapper
# Source this file to enable the 'dflow' command: source ./dflow.sh

dflow() {
    # Run the compiled release binary
    /home/rajeevkr945/bashscripts/AI-Assistant/target/release/dev-flow "$@"
    
    # Handle the IPC action for directory switching
    if [ -f /tmp/dev_flow_action ]; then
        local action=$(cat /tmp/dev_flow_action)
        rm /tmp/dev_flow_action
        
        # Execute the action (e.g., cd /path/to/project)
        eval "$action"
        
        # UI Feedback
        echo -e "\n\033[1;32m   Context Switched: \033[0m\033[1;34m$(basename "$PWD")\033[0m"
        ls --color=auto -F | head -n 5
    fi
}

echo "dflow function loaded. Type 'dflow' to start."
