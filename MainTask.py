import argparse
import queue
import sys
import os
import sounddevice as sd
import json 
import requests
import time
import threading
from dimits import Dimits                                                                           # New import for Dimits
from concurrent.futures import ThreadPoolExecutor
from vosk import Model, KaldiRecognizer

# Global variables and events
q = queue.Queue()
stop_flag = threading.Event()
tts_active = threading.Event()

# Wake word for activation
WAKE_WORD = "hi"

def callback(indata, frames, time, status):
    if status:
        print(status, file=sys.stderr)
    q.put(bytes(indata))


def internet_connection():
    try:
        response = requests.get("https://www.google.com/", timeout=5)
        print("The Internet is connected.")
        return True
    except requests.ConnectionError:
        print("The Internet Down.")
        return False   

def vosk_listen(recognizer):
    """Listens for a single phrase and returns the recognized text."""
    while not stop_flag.is_set():
        data = q.get()
        if recognizer.AcceptWaveform(data):
            result_json = json.loads(recognizer.Result())
            result_text = result_json["text"]
            if result_text:
                print(f"Recognized: {result_text}")
                return result_text
        # Optional: process partial results for more responsive feedback
        # else:
        #     print(recognizer.PartialResult())

def listen_for_command(recognizer):
    while not stop_flag.is_set():
        text = vosk_listen(recognizer)
        if text:
            if "stop and exit" in text.lower():
                stop_flag.set()
                print("Exiting...")
                break
            else:
                return text
            
def get_ollama_response(prompt):
    ollama_url = "http://localhost:11434/api/generate"
    headers = {"Content-Type": "application/json"}
    data = {
        "model": "tinydolphin",
        "prompt": prompt,
        "stream": False,
        "system": "You are a helpful assistant. You will answer the question in 30 to 40 words.",
        "options": {
            "num_keep": 0,
            "top_k": 10, 
            "temperature": 0.8,
        }
    }
    try:
        response, total_time = measure_total_time(requests.post, ollama_url, headers=headers, json=data, timeout=10)
        full_response = process_ollama_response(response)

        print(f"Ollama LLM response ({total_time * 1000:.2f} ms)")
        return full_response
    except requests.exceptions.RequestException as e:
        return f"Ollama API Request Failed: {e}"

def process_ollama_response(response):
    response_lines = response.iter_lines()
    full_response = ""
    for line in response_lines:
        if line:
            response_json = json.loads(line)
            full_response += response_json.get('response', '')
    return full_response

def get_openai_response(prompt):
    openai_url = "https://api.openai.com/v1/chat/completions"
    api_key = os.environ.get("OPENAI_API_KEY")
    if not api_key:
        return "Error: OPENAI_API_KEY environment variable not set."

    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }
    data = {
        "model": "gpt-3.5-turbo",
        "messages": [{"role": "user", "content": prompt}],
        "max_tokens": 100
    }

    try:
        response, total_time = measure_total_time(
            requests.post, openai_url, headers=headers, json=data, timeout=10
        )
        result = process_openai_response(response)
        print(f"Time taken by OpenAI: {total_time * 1000:.2f} ms")
        return result
    except requests.exceptions.RequestException as e:
        return f"OpenAI API Request Failed: {e}"

def process_openai_response(response):
    if response.status_code == 200:
        return response.json().get("choices", [{}])[0].get("message", {}).get("content", "No response content.")
    else:
        return f"Error: {response.status_code} - {response.text}"

def measure_total_time(func, *args, **kwargs):
    start_time = time.perf_counter()
    result = func(*args, **kwargs)
    end_time = time.perf_counter()
    total_time = end_time - start_time
    return result, total_time


def respond_to_text(text, tts_engine):
    if not internet_connection():
        speak("I am sorry, I cannot connect to the internet.")
        return

    with ThreadPoolExecutor() as executor:
        ollama_future = executor.submit(get_ollama_response, text)
        openai_future = executor.submit(get_openai_response, text)

        ollama_response = ollama_future.result()
        openai_response = openai_future.result()

        print("\nOllama LLM Response:")
        print(ollama_response)

        print("\nOpenAI Response:")
        print(openai_response)

        # Speak both responses using Dimits
        speak(ollama_response, tts_engine)
        speak(openai_response, tts_engine)

def speak(text, tts_engine):
    tts_active.set()  # TTS is active

    # Use Dimits for TTS
    tts_engine.text_2_speech(text, engine="aplay", sample_rate=16000)

    tts_active.clear()  # TTS is done

def main(args):
    print(f"Say 'stop' to end the chat. Use the wake word '{WAKE_WORD}' to activate the assistant.")

    try:
        # Check for available input devices before proceeding
        devices = sd.query_devices()
        input_devices = [d for d in devices if d['max_input_channels'] > 0]
        if not input_devices:
            print("Error: No audio input devices found. This script requires a microphone.")
            print("This is expected if you are running in an environment like Google Cloud Shell.")
            return

        device_info = sd.query_devices(args.device, "input")
        samplerate = int(device_info["default_samplerate"])
        model = Model(lang=args.model if args.model else "en-us")

        with sd.RawInputStream(samplerate=samplerate, blocksize=8000, device=args.device,
                               dtype="int16", channels=1, callback=callback) as stream:
            print("Vosk model loaded. Ready to listen...")
            print("Loading TTS model...")
            tts_engine = Dimits("en_US-amy-low")
            print("TTS model loaded.")
            recognizer = KaldiRecognizer(model, samplerate)
            
            while not stop_flag.is_set():
                # This initial listen is for the wake word or 'stop'
                recognized_text = vosk_listen(recognizer)
                if recognized_text:
                    if WAKE_WORD in recognized_text.lower():
                        print("**********************************************************************************")
                        print("****************************WAKE WORD DETECTED************************************")
                        print("**********************************************************************************")
                        command = listen_for_command(recognizer)
                        if command:
                            respond_to_text(command, tts_engine)
                    elif "stop" in recognized_text.lower():
                        stop_flag.set()
                        print("Stopping...")
    except sd.PortAudioError as e:
        print(f"A sound device error occurred: {e}")
        print("Please ensure your microphone is configured correctly and you have selected the correct device ID if necessary.")
        stop_flag.set()
    except Exception as e:
        print(f"An error occurred: {e}")
        stop_flag.set()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Vosk with Ollama and OpenAI Integration")
    parser.add_argument("-d", "--device", type=int, help="input device ID")
    parser.add_argument("-m", "--model", type=str, default="en-us", help="language model")
    args = parser.parse_args()

    # To use the OpenAI functionality, set the environment variable like this before running:
    # export OPENAI_API_KEY='your_api_key_here'
    main(args)
