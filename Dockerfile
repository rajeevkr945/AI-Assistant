# Use an official Python runtime as a parent image
FROM python:3.12-slim-bookworm

# Set the working directory in the container
WORKDIR /app

# Install system dependencies required for the application
# - libportaudio2: for sounddevice (audio I/O)
# - portaudio19-dev: build-time dependency for sounddevice
# - alsa-utils: for aplay, used by dimits TTS
# - libgpiod-dev: for gpiod (Raspberry Pi hardware control)
# - python3-tk: for the Tkinter GUI
RUN apt-get update && apt-get install -y \
    libportaudio2 \
    portaudio19-dev \
    alsa-utils \
    libgpiod-dev \
    python3-tk \
    && rm -rf /var/lib/apt/lists/*

# Copy the requirements file and install Python dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy the rest of the application code into the container
COPY . .