import serial
import numpy as np
import cv2

def read_frame(s: serial.Serial):
    """Reads a frame from the serial port. This will be of size 96x96x2, and must be processed into BGR before reading."""
    # Busy-wait until we recieve the start bit (1)
    while True:
        if s.read() == b'S':
            if s.read() == b'T':
                if s.read() == b'A':
                    if s.read() == b'R':
                        if s.read() == b'T':
                            break
    
    # Read the 32-bit number of rows
    rows = int(s.read(size=4).decode(), base=16)
    cols = int(s.read(size=4).decode(), base=16)
    channels = 2

    # Read the following data as a matrix of integers with the rows/cols vals.
    mat_data = np.zeros((rows, cols, channels), dtype=np.uint8)
    for row in range(0, cols):
        for col in range(0, rows):
            for channel in range(0, channels):
                mat_data[row, col, channel] = int(s.read(size=2).decode(), base=16)

    # Return the BGR image.
    return mat_data

def rgb565_to_rgb888(frame: cv2.Mat):
    """Takes an RGB565 frame and converts it to BGR888."""

    # Extract the fields from the frame data.
    pixel_data = (frame[:, :, 0].astype(np.uint16) << 8) | (frame[:, :, 1].astype(np.uint16))

    blue = pixel_data & 0x1f
    green = (pixel_data >> 5) & 0x3f
    red = (pixel_data >> 11) & 0x1f

    # Scale the channels to the full 8-bit range.
    red = ((red / 31.0) * 255).astype(np.uint8)
    green = ((green / 63.0) * 255).astype(np.uint8)
    blue = ((blue / 31.0) * 255).astype(np.uint8)
    
    result = np.stack((blue, green, red), axis=-1)

    return result
    


def display_frames(s: serial.Serial):
    while True:
        frame = read_frame(s)
        rgb888_frame = rgb565_to_rgb888(frame)
        rgb888_frame = cv2.resize(rgb888_frame, (300, 300))
        cv2.imshow('Frame', rgb888_frame)
        
        key_v = cv2.waitKey(1)
        if key_v == 13:
            break

def main():
    """The main subroutine."""
    s = serial.Serial()
    s.port = 'COM4'
    s.baudrate = 115200
    s.bytesize = 8
    s.stopbits = 1
    s.parity = 'N'
    #s.timeout = 0.5
    s.setDTR(False)
    s.setRTS(False)
    s.open()

    display_frames(s)
    s.close()

if __name__ == '__main__':
    main()