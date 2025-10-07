# AI Assistant

This project is an AI assistant that runs on a Raspberry Pi. It uses a physical button to start and stop the main application, which listens for a wake word, processes spoken commands using both a local Ollama model and the OpenAI API, and speaks the responses back to the user.

## Project Overview

The project consists of three main Python scripts:

-   `control_script.py`: This script runs on the Raspberry Pi and listens for a button press to start and stop the main application. It uses `gpiod` to interact with the GPIO pins.
-   `MainTask.py`: This is the core of the application. It uses the Vosk library for speech recognition, and when the wake word is detected, it sends the user's command to both a local Ollama model and the OpenAI API. The responses are then spoken back to the user using the Dimits text-to-speech engine.
-   `GUI.py`: A simple Tkinter-based GUI that displays the status of the application.

## How it Works

1.  The `control_script.py` is the entry point of the application. It continuously monitors a GPIO pin for a button press.
2.  When the button is pressed, the script starts both the `MainTask.py` and `GUI.py` scripts in separate processes.
3.  The `MainTask.py` script initializes the Vosk speech recognition engine and listens for the wake word ("hi").
4.  Once the wake word is detected, the script listens for a command from the user.
5.  The command is then sent to both the local Ollama model and the OpenAI API for processing.
6.  The responses from both models are printed to the console and spoken back to the user using the Dimits TTS engine.
7.  The `GUI.py` script displays a simple window with the status of the application.
8.  If the button is pressed again, the `control_script.py` stops the `MainTask.py` and `GUI.py` scripts.

## Setup and Installation

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/your-username/AI-Assistant.git
    cd AI-Assistant
    ```
2.  **Create a virtual environment:**
    ```bash
    python3 -m venv myenv
    source myenv/bin/activate
    ```
3.  **Install the dependencies:**
    The `Detailed_Requirement.txt` file contains a list of all the necessary packages. You can install them using pip:
    ```bash
    pip install -r Detailed_Requirement.txt
    ```
    *Note: The `requirements.txt` file contains a subset of the dependencies. It is recommended to use `Detailed_Requirement.txt` for a complete installation.*

4.  **Set up the OpenAI API Key:**
    The `KEY` file contains instructions on how to set up the OpenAI API key. You can either place the key directly in the `MainTask.py` script or set it as an environment variable.

5.  **Run the application:**
    The `control_script.py` is designed to be run on a Raspberry Pi with a button connected to GPIO pin 27. To run the application, simply execute the script:
    ```bash
    python3 control_script.py
    ```

## Usage

1.  Press the button connected to GPIO pin 27 to start the application.
2.  The GUI window will appear, indicating that the application is running.
3.  Say the wake word "hi" to activate the assistant.
4.  Speak your command to the assistant.
5.  The assistant will speak the responses from both the Ollama and OpenAI models.
6.  To stop the application, press the button again.

*Note: The application requires an active internet connection to work, as it relies on the OpenAI API.*
