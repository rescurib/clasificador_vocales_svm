#!/usr/bin/env python3
"""
UART to feature vector for ML training

This script listens to a serial port for MFCC data streamed from an STM32 microcontroller and saves it as a numpy array.

Usage:
    python serial_to_features.py [--port COM_PORT] [--output OUTPUT_FILE]

Arguments:
    --port      Serial COM port to listen to (default: COM12)
    --output    Output numpy file name (default: features.npy)

Example:
    python serial_to_features.py --port COM3 --output mfcc_features_data.npy

"""
import argparse
import serial
import struct
import numpy as np

DEFAULT_COM_PORT    = 'COM12'
DEFAULT_SAMPLE_RATE = 8000
DEFAULT_NPY_FILE    = 'features.npy'

MFCC_FEATURES_PER_FRAME = 26
training_data = []  # List to hold all feature vectors

# Windows: use "mode" to see available COM ports
# Linux: use "ls /dev/tty*"

def parse_args():
    parser = argparse.ArgumentParser(description='UART to feature vector for ML training')
    parser.add_argument('--port', type=str, default=DEFAULT_COM_PORT, help='Serial COM port (default: COM3)')
    parser.add_argument('--output', type=str, default=DEFAULT_NPY_FILE, help='Output numpy file name (default: features.npy)')
    return parser.parse_args()


def main():
    args = parse_args()
    ser = serial.Serial(args.port, baudrate=460800, timeout=1)
    print('Listening on {}...'.format(args.port))
    samples = []
    streaming = False
    buffer = b''
    sample_count = 0

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

            # Check for MFCC feature vector start
            if b'MFCC_START' in buffer:
                print('MFCC_START detected')
                buffer = b''
                continue

            # Check for MFCC feature vector stop
            if b'MFCC_END' in buffer:
                if samples and len(samples) % MFCC_FEATURES_PER_FRAME == 0:
                    training_data.append(samples)
                    samples = []
                    sample_count += 1
                    print("MFCC sample saved: {}".format(sample_count))
                else:
                    print("Wrong number of features received: {}".format(len(samples)))
                    samples = []
                buffer = b''
                continue

            # Process 4 bytes + newline (float32 + '\n')
            if len(buffer) == 5 and buffer[4] == ord('\n'):
                float_bytes = buffer[:4]
                try:
                    # Unpack little-endian float32
                    f = struct.unpack('<f', float_bytes)[0]
                    samples.append(f)
                except Exception as e:
                    print('Error decoding float:', e)
                buffer = b''
    ser.close()
    if training_data:
        print('Saving {} features to {}'.format(len(training_data), args.output))
        arr = np.array(training_data, dtype=np.float32)
        np.save(args.output, arr)
        print('Numpy file saved.')
    else:
        print('No samples received.')

if __name__ == '__main__':
    main()
