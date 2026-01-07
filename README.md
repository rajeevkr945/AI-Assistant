
# Linux/Raspberry Pi Voice Assistant

A voice-activated assistant designed for Linux (specifically Pi 4/5). It listens for a wake word, queries a local LLM (**Ollama**) and a cloud LLM (**OpenAI**) simultaneously, and reads both responses back to you. It includes a physical "panic/restart" button script and runs inside Docker to avoid dependency hell with audio drivers and GPIO.

## What it does

* **Wake word:** Listens for "hi" via Vosk.
* **Dual-Brain:** Sends your prompt to `tinydolphin` (local) and GPT (cloud) at the same time.
* **Voice out:** TTS support for both responses.
* **Physical Toggle:** Uses GPIO27 to let you restart the service with a hardware button.
* **Dockerized:** No need to manually install audio libs or Python packages on your host.

## Quick Start

### 1. Hardware & Local Deps

* **Mic & Speakers:** Ensure your Linux host recognizes your audio hardware (`arecord -l`).
* **Ollama:** Must be running on the host machine.
```bash
ollama pull tinydolphin

```


* **GPIO (Optional):** If using the hardware restart, wire a button to GPIO27.

### 2. Repo Setup

```bash
git clone <your-repository-url>
cd AI-Assistant

```

### 3. Launching

Since this uses GUI (Tkinter) and Audio inside Docker, you need to allow X11 access and pass your API key:

```bash
# Allow the container to open the status window
xhost +

# Set your key for the current session
export OPENAI_API_KEY='your_key_here'

# Build and fire it up
docker-compose up --build

```

---

## Usage

* **To start:** Just say "Hi".
* **To quit:** Say "Stop and exit" or `Ctrl+C` in the terminal.
* **Hardware Reset:** Hit the physical button on GPIO27 to kill and restart the process.

## Environment & Config

* **OpenAI:** Requires `OPENAI_API_KEY` in your environment.
* **Ollama:** Defaults to `localhost:11434`. If the container can't see the host's Ollama, you might need to change the connection string to `host.docker.internal` or use `network_mode: "host"` in the compose file.

## Troubleshooting

* **Audio Issues:** If you get `No audio input devices found`, check `docker-compose.yml`. Ensure `/dev/snd` is mapped correctly. Some kernels require `--privileged` mode to access the mic.
* **No GUI Window:** Make sure `DISPLAY` is set. If you're on a headless Pi via SSH, the Tkinter window will fail unless you're using X-forwarding.
* **Ollama Connection:** If the local model isn't responding, ensure the Ollama service isn't bound strictly to `127.0.0.1`. You may need to set `OLLAMA_HOST=0.0.0.0`.
* **Missing `pkill`:** If the restart script fails, ensure `procps` is installed in your Dockerfile.

---
