import serial
import numpy as np
import cv2
import time
import threading
import queue
import functools

def disp_threshold_frame(frame: cv2.Mat, thresh_color: dict, win_name: str):
    """"""

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
    channels = int(s.read(size=4).decode(), base=16)

    # Read the following data as a matrix of integers with the rows/cols vals.
    mat_data = np.zeros((rows, cols, channels), dtype=np.uint8)
    pre_read = time.time()
    for row in range(0, cols):
        for col in range(0, rows):
            for channel in range(0, channels):
                mat_data[row, col, channel] = int(s.read(size=2).decode(), base=16)
    post_read = time.time()
    print(f'time: {post_read - pre_read}')

    # Return the BGR image.
    return mat_data


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
            frame = frame_queue.get()

            # Display the image, upped to three channels so that it may be displayed.
            frame_disp = cv2.cvtColor(frame, cv2.COLOR_BGR5652BGR)
            frame_disp = cv2.resize(frame_disp, (300, 300))
            cv2.imshow('Frame', frame_disp)

            # Display the frame thresholded according to the thresh_color parameter.
            #frame_mask = cv2.inRange()

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

    # The BGR threshold for the image.
    thresh_color = {
        "hue": 0,
        "saturation": 0,
        "value": 0
    }

    setup_color_thresh_window("Thresholding color", thresh_color)
    main_loop(s, thresh_color)
    s.close()

if __name__ == '__main__':
    main()