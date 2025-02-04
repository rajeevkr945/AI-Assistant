import argparse
import queue
import sys
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

def vosk_listen(args):
    device_info = sd.query_devices(args.device, "input")
    args.samplerate = int(device_info["default_samplerate"])

    model = Model(lang=args.model if args.model else "en-us")

    with sd.RawInputStream(samplerate=args.samplerate, blocksize=8000, device=args.device,
                           dtype="int16", channels=1, callback=callback):
        rec = KaldiRecognizer(model, args.samplerate)

        while not stop_flag.is_set():
            data = q.get()
            if rec.AcceptWaveform(data):
                result_json = json.loads(rec.Result())
                result_text = result_json["text"]
                if result_text:
                    print(f"Recognized: {result_text}")
                    return result_text

def handle_recognized_text(text):
    if "stop" in text.lower():
        stop_flag.set()
        # print("Stopping TTS and resetting...")
    elif WAKE_WORD in text.lower():
        print("**********************************************************************************")
        print("****************************PROGRAM START*****************************************")
        print("**********************************************************************************")
        command = listen_for_command()
        if command:
            respond_to_text(command)

def listen_for_command():
    while not stop_flag.is_set():
        text = vosk_listen(args)
        if text:
            if "stop and exit" in text.lower():
                stop_flag.set()
                print("Exiting...")
                break
            else:
                return text
            
def get_ollama_response(prompt):
    if internet_connection():
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
        response, total_time = measure_total_time(requests.post, ollama_url, headers=headers, json=data)
        full_response = process_ollama_response(response)

        print(f"Ollama LLM response ({total_time * 1000:.2f} ms)")
        return full_response
    else:
        return False

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
    api_key = ""  # Hardcoded OpenAI API key
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
        if internet_connection():
                response, total_time = measure_total_time(
                    requests.post, openai_url, headers=headers, json=data
            )
                print("Internet Connection OK")
                result = process_openai_response(response)
                print(f"Time taken by OpenAI: {total_time * 1000:.2f} ms")
                return result
    except requests.exceptions.RequestException as e:
            return f"OpenAI API Request Failed: {str(e)}"

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


def respond_to_text(text):
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
        speak(ollama_response)
        speak(openai_response)

def speak(text):
    tts_active.set()  # TTS is active

    # Use Dimits for TTS
    dt = Dimits("en_US-amy-low")  # Adjust the voice model as needed
    dt.text_2_speech(text, engine="aplay", sample_rate=16000)

    tts_active.clear()  # TTS is done

def continuous_chat_with_vosk(args):
    print("Say 'stop' to end the chat. Use the wake word 'hello' to activate.")

    while not stop_flag.is_set():
        recognized_text = vosk_listen(args)
        if recognized_text:
            handle_recognized_text(recognized_text)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Vosk with Ollama and OpenAI Integration")
    parser.add_argument("-d", "--device", type=int, help="input device ID")
    parser.add_argument("-m", "--model", type=str, default="en-us", help="language model")
    args = parser.parse_args()

    continuous_chat_with_vosk(args)
