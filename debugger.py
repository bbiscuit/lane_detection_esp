import serial
import numpy as np
import cv2
import time
import threading
import queue
import functools
import json
import sys

thresh_frame = None

detected_center: int = -1
detected_outside_line: int = -1

def disp_outside_thresh_frame(win_name: str, settings: dict):
    """Displays a thresholded version of the thresh_frame image, given the parameters set in the debugger. 
    It will also scale the frame to that which is in the settings before displaying."""
    global thresh_frame

    if thresh_frame is not None:
        working_frame = thresh_frame.copy()

        # Perform cropping of the image based upon the cropping parameters.
        cropping = settings['cropping']
        rows, cols, _ = working_frame.shape

        top_cropping = cropping['top']
        if top_cropping > 0:
            cv2.rectangle(working_frame, (0, 0), (cols, top_cropping), (0, 0, 0), -1)

        bottom_cropping = cropping['bottom']
        if bottom_cropping > 0:
            cv2.rectangle(working_frame, (0, rows), (cols, rows - bottom_cropping), (0, 0, 0), -1)

        left_cropping = cropping['left']
        if left_cropping > 0:
            cv2.rectangle(working_frame, (0, 0), (left_cropping, rows), (0, 0, 0), -1)
        
        right_cropping = cropping['right']
        if right_cropping > 0:
            cv2.rectangle(working_frame, (cols, 0), (cols - right_cropping, rows), (0, 0, 0), -1)

        # Perform color thresholding.
        thresh_color_min = settings['thresh_color_min']
        thresh_color_max = settings['thresh_color_max']

        low = (thresh_color_min['hue'], thresh_color_min['saturation'], thresh_color_min['value'])
        high = (thresh_color_max['hue'], thresh_color_max['saturation'], thresh_color_max['value'])
        working_frame = cv2.inRange(working_frame, low, high)

        cv2.imshow(win_name, working_frame)


def read_frame(s: serial.Serial) -> tuple[cv2.Mat, str]:
    """Reads a frame from the serial port. Returned with it is the type of the frame,
    so that it can be intelligently processed."""

    global detected_outside_line
    global detected_center

    # Busy-wait until we recieve the start bit (1)
    while True:
        if s.read() == b'S':
            if s.read() == b'T':
                if s.read() == b'A':
                    if s.read() == b'R':
                        if s.read() == b'T':
                            break
        if s.read() == b'c':
            if s.read() == b'e':
                if s.read() == b'n':
                    if s.read() == b't':
                        if s.read() == b'e':
                            if s.read() == b'r':
                                print('Read center line.')
                                detected_center = int(s.readline().decode())
        if s.read() == b's':
            if s.read() == b'o':
                if s.read() == b'l':
                    if s.read() == b'i':
                        if s.read() == b'd':
                            detected_outside_line = int(s.readline().decode())
                            print(f'Read solid line loc: {detected_outside_line}')
    
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


def main_loop(s: serial.Serial, settings: dict):

    global thresh_frame

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
                # Convert to a color that we can process.
                frame = cv2.cvtColor(frame, cv2.COLOR_BGR5652BGR)

                # If we have read a center, draw it in read on the image.
                if detected_center != -1:
                    cv2.line(frame, (detected_center, 0), (detected_center, frame.shape[0]), (0, 0, 255), 5)

                # Blow it up so that it's easier to see.
                big_frame = cv2.resize(frame, (settings['scaled_frame_size']['height'], settings['scaled_frame_size']['width']))
                cv2.imshow('Pre-processed Frame', big_frame)

                # Display the thresholded frame. If the thresh_frame is None, that means that the frame hasn't been set up yet; therefore,
                # set it up.
                if thresh_frame is None:
                    setup_outside_thresh_window('Outside Line Thresholding', frame.shape[0], frame.shape[1], settings)

                frame_hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
                thresh_frame = frame_hsv
                disp_outside_thresh_frame('Outside Line Thresholding', settings)

            # Otherwise if the received frame was a binary mask (CV_8UC1 or CV_8U__), display
            # without any changes.
            elif 'CV_8UC1' == frame_type or 'CV_8U__' == frame_type:
                cv2.imshow('Mask', frame)
            
            elif 'CV_8UC3' == frame_type:
                cv2.imshow('8-bit Color', frame)

            # Respawn the reader thread (so that the window can still read updates)
            reader_thread = threading.Thread(target=frame_reader)
            reader_thread.start()

        # Use waitkey to make the window responsive.
        key_v = cv2.waitKey(1)

        # Exit if the user hits "enter."
        if key_v == 13:
            break


def setup_outside_thresh_window(window_name: str, native_frame_height: int, native_frame_width: int, settings: dict):
    """Sets up the window which has the trackbars for BGR thresholding (for calibration)."""

    thresh_color_min = settings['thresh_color_min']
    thresh_color_max = settings['thresh_color_max']
    cropping = settings['cropping']

    def on_trackbar(val, color_to_update, dim):
        color_to_update[dim] = val
        disp_outside_thresh_frame(window_name, settings)
    
    cv2.namedWindow(window_name)
    cv2.createTrackbar("Min Hue", window_name, thresh_color_min["hue"], 179, functools.partial(on_trackbar, color_to_update=thresh_color_min, dim="hue"))
    cv2.createTrackbar("Min Saturation", window_name, thresh_color_min["saturation"], 255, functools.partial(on_trackbar, color_to_update=thresh_color_min, dim="saturation"))
    cv2.createTrackbar("Min Value", window_name, thresh_color_min["value"], 255, functools.partial(on_trackbar, color_to_update=thresh_color_min, dim="value"))
    
    cv2.createTrackbar("Max Hue", window_name, thresh_color_max["hue"], 179, functools.partial(on_trackbar, color_to_update=thresh_color_max, dim="hue"))
    cv2.createTrackbar("Max Saturation", window_name, thresh_color_max["saturation"], 255, functools.partial(on_trackbar, color_to_update=thresh_color_max, dim="saturation"))
    cv2.createTrackbar("Max Value", window_name, thresh_color_max["value"], 255, functools.partial(on_trackbar, color_to_update=thresh_color_max, dim="value"))

    def cropping_callback(val, crop_settings: dict, crop_direction: str):
        """The callback for trackbars related to image cropping."""
        crop_settings[crop_direction] = val
        disp_outside_thresh_frame(window_name, settings)

    # Create cropping trackbars.
    cv2.createTrackbar('Top cropping', window_name, cropping['top'], native_frame_height, functools.partial(cropping_callback, crop_settings=cropping, crop_direction='top'))
    cv2.createTrackbar('Left cropping', window_name, cropping['left'], native_frame_width, functools.partial(cropping_callback, crop_settings=cropping, crop_direction='left'))
    cv2.createTrackbar('Right cropping', window_name, cropping['right'], native_frame_width, functools.partial(cropping_callback, crop_settings=cropping, crop_direction='right'))
    cv2.createTrackbar('Bottom cropping', window_name, cropping['bottom'], native_frame_height, functools.partial(cropping_callback, crop_settings=cropping, crop_direction='bottom'))


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

    if len(sys.argv) > 1:
        s.port = sys.argv[1]

    s.open()

    # The BGR threshold for the image.
    main_loop(s, settings)

    # Write-back convenience values to settings.
    with open('debugger_settings.json', 'w') as f:
        json.dump(settings, f)

    s.close()


if __name__ == '__main__':
    main()
