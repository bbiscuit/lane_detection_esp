import serial
import numpy as np
import cv2
import time
import threading
import queue
import functools
import json

def disp_threshold_frame(frame: cv2.Mat, thresh_color: dict, win_name: str):
    """"""

def read_frame(s: serial.Serial) -> tuple[cv2.Mat, str]:
    """Reads a frame from the serial port. Returned with it is the type of the frame,
    so that it can be intelligently processed."""
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
    channels = int(s.read(size=4).decode(), base=16)
    type = s.read(size=7).decode()

    print(f'Found mat of size {rows}x{cols}x{channels} of type {type}')

    # Read the following data as a matrix of integers with the rows/cols vals.
    mat_data = np.zeros((rows, cols, channels), dtype=np.uint8)
    pre_read = time.time()

    for row in range(0, rows):
        for col in range(0, cols):
            for channel in range(0, channels):
                try:
                    data_bytes = s.read(size=2)
                except Exception:
                    return np.zeros((rows, cols, channels), dtype=np.uint8)

                mat_data[row, col, channel] = int(data_bytes.decode(), base=16)

    post_read = time.time()
    print(f'time: {post_read - pre_read}')

    # Return the BGR image.
    return (mat_data, type)


def main_loop(s: serial.Serial, thresh_color: dict):
    print("Starting main loop...")
    # Create the thread for reading the frame.
    frame_queue = queue.Queue()
    def frame_reader():
        result = read_frame(s)
        frame_queue.put(result)
    reader_thread = threading.Thread(target=frame_reader)
    reader_thread.start()

    # Eternal loop
    while True:
        # If the reader thread is complete, show the frame and restart the thread.
        if not reader_thread.is_alive():
            frame, frame_type = frame_queue.get()

            # If the received thread was of type CV_8UC2, up it to eight-bit color and display.
            if 'CV_8UC2' == frame_type:
                frame = cv2.cvtColor(frame, cv2.COLOR_BGR5652BGR)
                cv2.imshow('Pre-processed Frame', frame)

            # Otherwise if the received frame was a binary mask (CV_8UC1 or CV_8U__), display
            # without any changes.
            elif 'CV_8UC1' == frame_type or 'CV_8U__' == frame_type:
                cv2.imshow('Mask', frame)

            # Respawn the reader thread (so that the window can still read updates)
            reader_thread = threading.Thread(target=frame_reader)
            reader_thread.start()

        # Use waitkey to make the window responsive.
        key_v = cv2.waitKey(1)

        # Exit if the user hits "enter."
        if key_v == 13:
            break


def setup_color_thresh_window(window_name: str, thresh_color: dict):
    """Sets up the window which has the trackbars for BGR thresholding (for calibration)."""

    def on_trackbar(val, color_to_update, dim):
        color_to_update[dim] = val

    cv2.namedWindow(window_name)
    cv2.createTrackbar("Hue", window_name, thresh_color["hue"], 179, functools.partial(on_trackbar, color_to_update=thresh_color, dim="hue"))
    cv2.createTrackbar("Saturation", window_name, thresh_color["saturation"], 255, functools.partial(on_trackbar, color_to_update=thresh_color, dim="saturation"))
    cv2.createTrackbar("Value", window_name, thresh_color["value"], 255, functools.partial(on_trackbar, color_to_update=thresh_color, dim="value"))

def load_settings(filename: str) -> dict:
    """Loads settings for the app from the given json file."""
    with open(filename, 'r') as f:
        return json.load(f)

def main():
    """The main subroutine."""

    settings = load_settings('debugger_settings.json')

    s = serial.Serial()
    s.port = settings['default_com_port']
    s.baudrate = 115200
    s.bytesize = 8
    s.stopbits = 1
    s.parity = 'N'
    #s.timeout = 0.5
    s.setDTR(False)
    s.setRTS(False)
    s.open()

    # The BGR threshold for the image.
    thresh_color = {
        "hue": 0,
        "saturation": 0,
        "value": 0
    }

    main_loop(s, thresh_color)
    s.close()


if __name__ == '__main__':
    main()
