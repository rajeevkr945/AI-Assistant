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

def is_task_running():
    """Checks if the main task is currently running.

    Returns:
        bool: True if the task is running, False otherwise.
    """
    try:
        result = subprocess.run(['pgrep', '-f', MAIN_TASK_PATH], stdout=subprocess.PIPE)
        return len(result.stdout) > 0
    except Exception as e:
        print(f"Error checking task status: {e}")
        return False

def start_task():
    """Starts the main task and the GUI.

    This function uses subprocess.Popen to start both the main task and the GUI
    in separate processes, using the Python executable from the virtual environment.
    """
    try:
        # Call the main task using the venv Python executable
        subprocess.Popen([VENV_PYTHON, MAIN_TASK_PATH])
        subprocess.Popen([VENV_PYTHON, GUI_PATH])
        print("Main task started.")
    except Exception as e:
        print(f"Error starting main task: {e}")

def stop_task():
    """Stops the main task and the GUI.

    This function uses `pkill` to find and terminate the processes for both the
    main task and the GUI.
    """
    try:
        subprocess.run(['pkill', '-f', MAIN_TASK_PATH])
        subprocess.run(['pkill', '-f', GUI_PATH])
        
        print("Main task stopped.")
    except Exception as e:
        print(f"Error stopping main task: {e}")

# Setup GPIO for the button using gpiod
with gpiod.request_lines(
    "/dev/gpiochip0",
    consumer="control-script",
    config={
        LINE: gpiod.LineSettings(
            direction=Direction.INPUT, output_value=Value.ACTIVE
        )
    },
) as request:
    
    # Main control loop to monitor the button
    while True:
        # Check the state of the GPIO button
        if request.get_value(LINE):
            print("Button pressed: Restarting the main task...")
            # If the task is running, stop it before restarting
            if is_task_running():
                stop_task()
                time.sleep(2)  # Wait for the process to terminate

            # Start the main task
            start_task()
             
        else:
            # The script is idle when the button is not pressed
            print("idle")
        
        # Check the button state every second
        time.sleep(1)
