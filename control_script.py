import os
import subprocess
import time
import gpiod
from gpiod.line import Direction, Value

# Path to the main task
MAIN_TASK_PATH = "/home/pi/Downloads/OllamaProject/myenv/mainTask.py"
# Path to the virtual environment's Python executable
VENV_PYTHON = "/home/pi/Downloads/OllamaProject/myenv/bin/python"
GUI_PATH = "/home/pi/Downloads/OllamaProject/myenv/GUI.py"
# GPIO line configuration
LINE = 27

# Function to check if the main task is running
def is_task_running():
    try:
        result = subprocess.run(['pgrep', '-f', MAIN_TASK_PATH], stdout=subprocess.PIPE)
        return len(result.stdout) > 0
    except Exception as e:
        print(f"Error checking task status: {e}")
        return False

# Function to start the main task
def start_task():
    try:
        # Call the main task using the venv Python executable
        subprocess.Popen([VENV_PYTHON, MAIN_TASK_PATH])
        subprocess.Popen([VENV_PYTHON, GUI_PATH])
        print("Main task started.")
    except Exception as e:
        print(f"Error starting main task: {e}")

# Function to stop the main task (if needed)
def stop_task():
    try:
        subprocess.run(['pkill', '-f', MAIN_TASK_PATH])
        subprocess.run(['pkill', '-f', GUI_PATH])
        
        print("Main task stopped.")
    except Exception as e:
        print(f"Error stopping main task: {e}")

# Setup GPIO for the button
with gpiod.request_lines(
    "/dev/gpiochip0",
    consumer="control-script",
    config={
        LINE: gpiod.LineSettings(
            direction=Direction.INPUT, output_value=Value.ACTIVE
        )
    },
) as request:
    
    # Main control loop
    while True:
        # Check the button state
        if request.get_value(LINE):
            print("Button pressed: Restarting the main task...")
            # Stop the current task if running
            if is_task_running():
                stop_task()
                time.sleep(2)

            # Start the main task
            start_task()
             
        else:
            print("idle")
        
        time.sleep(1)  # Check the button state every second
