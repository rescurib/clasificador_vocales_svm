#!/usr/bin/env python3
"""
UART to WAV Recorder for STM32 Audio Streaming

This script listens to a serial port for audio data streamed from an STM32 microcontroller and saves it as a 24-bit mono WAV file.

Usage:
    python serial_to_wav.py [--port COM_PORT] [--rate SAMPLE_RATE] [--output OUTPUT_FILE]

Arguments:
    --port      Serial COM port to listen to (default: COM12)
    --rate      Sample rate in Hz for the WAV file (default: 8000)
    --output    Output WAV file name (default: output.wav)

Example:
    python serial_to_wav.py --port COM3 --rate 16000 --output my_audio.wav

"""
import argparse
import serial
import struct
import numpy as np
from scipy.io import wavfile

DEFAULT_COM_PORT = 'COM12'
DEFAULT_SAMPLE_RATE = 8000
DEFAULT_WAV_FILE = 'output.wav'

# Windows: use "mode" to see available COM ports
# Linux: use "ls /dev/tty*"

def parse_args():
    parser = argparse.ArgumentParser(description='UART to WAV recorder for STM32 audio streaming')
    parser.add_argument('--port', type=str, default=DEFAULT_COM_PORT, help='Serial COM port (default: COM3)')
    parser.add_argument('--rate', type=int, default=DEFAULT_SAMPLE_RATE, help='Sample rate in Hz (default: 8000)')
    parser.add_argument('--output', type=str, default=DEFAULT_WAV_FILE, help='Output WAV file name (default: output.wav)')
    return parser.parse_args()


def main():
    args = parse_args()
    ser = serial.Serial(args.port, baudrate=460800, timeout=1)
    print('Listening on {}...'.format(args.port))
    samples = []
    streaming = False
    buffer = b''
    while True:
        byte = ser.read(1)
        if not byte:
            continue
        buffer += byte
        # Only check for text commands if not streaming
        if not streaming and b'\n' in buffer:
            try:
                text = buffer.decode(errors='ignore').strip()
            except Exception:
                buffer = b''
                continue
            if text == 'Mic acquisition: START':
                print('Mic acquisition: START')
                streaming = True
                buffer = b''
                continue
            buffer = b''
            continue

        if streaming:
            # Check for stop command
            if  b"Mic acquisition: STOP" in buffer:
                print('Mic acquisition: STOP')
                break
            # Process 4 bytes + newline (float32 + '\n')
            if len(buffer) == 5 and buffer[4] == ord('\n'):
                float_bytes = buffer[:4]
                try:
                    # Unpack little-endian float32
                    f = struct.unpack('<f', float_bytes)[0]
                    # Clamp to [-1.0, 1.0]
                    f = max(-1.0, min(1.0, f))
                    samples.append(f)
                except Exception as e:
                    print('Error decoding float:', e)
                buffer = b''
    ser.close()
    if samples:
        print('Saving {} samples to {}'.format(len(samples), args.output))
        arr = np.array(samples, dtype=np.float32)
        wavfile.write(args.output, args.rate, arr)
        print('WAV file saved.')
    else:
        print('No samples received.')

if __name__ == '__main__':
    main()
