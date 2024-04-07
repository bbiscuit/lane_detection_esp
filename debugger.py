"""
The debugger for the lane detection app, used to read ESP32 output as well as configure certain
settings.
"""

import time
import threading
import queue
import functools
import json
import sys
import serial
import numpy as np
import cv2

LINE_LOC_WIN_ROWS = 255
LINE_LOC_WIN_COLS = LINE_LOC_WIN_ROWS*2
LINE_LOC_WIN_TITLE = 'Outside Line Calibration'
LINE_LOC_BUTTON_TEXT = 'Click this window to record values.'

frame_to_thresh: cv2.Mat = None
frame_threshed_outside: cv2.Mat = None
frame_threshed_stop: cv2.Mat = None

detected_center: int = -1
detected_outside_line: int = -1


def get_largest_contour(img: cv2.Mat):
    """Finds the contours in the image and returns the largest."""
    contours, _ = cv2.findContours(img, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

    # Find the largest contour, area-wise.
    def bounding_rect_area(contour):
        _, _, w, h = cv2.boundingRect(contour)
        return int(w) * int(h) # Just to make absolutely sure this is an int mult.

    contours = sorted(contours, key=bounding_rect_area, reverse=True)
    if 0 == len(contours):
        return None
    return contours[0]


def setup_white_line_loc_calibration_window(settings: dict):
    """Sets up a window which, when clicked, records detected data about the white line.
    This is taken to be its "natural state," where the line should be. These values are
    then compared on the ESP-32 with the detected values, and changes are made such that
    the line is closer to the ideal."""



    def on_click(event, _, __, ___, ____):
        # We only care about the click event.
        if event != cv2.EVENT_LBUTTONDOWN:
            return

        # If we don't yet have an image to extract data from, report and exit.
        if frame_threshed_outside is None:
            print('Have not receieved data yet.')
            return

        # Get the largest contour.
        line = get_largest_contour(frame_threshed_outside)
        if line is None:
            print('Did not find a line.')
            return

        # Extract data from the bounding-rect and write it to the settings.
        x_line, _, w_line, _ = cv2.boundingRect(line)
        settings['outside_line_data']['x'] = x_line + (w_line // 2)
        print('Recorded line data.')


    img = np.ones((LINE_LOC_WIN_ROWS, LINE_LOC_WIN_COLS, 3), np.uint8) * 255
    font = cv2.FONT_HERSHEY_SIMPLEX
    fontScale = 0.5
    color = (255, 0, 255)  # BGR color
    thickness = 1
    img = cv2.putText(img, LINE_LOC_BUTTON_TEXT, (50, 50), font, fontScale, color, thickness, cv2.LINE_AA)
    cv2.imshow(LINE_LOC_WIN_TITLE, img)
    cv2.setMouseCallback(LINE_LOC_WIN_TITLE, on_click)


def disp_thresh_frame(win_name: str, settings: dict) -> cv2.Mat:
    """Displays a thresholded version of the thresh_frame image, given the parameters set in the debugger.
    It will also scale the frame to that which is in the settings before displaying. The frame post-threshold
    is written into the 'frame_threshed' global variable.."""
    global frame_to_thresh

    if frame_to_thresh is not None:
        working_frame = frame_to_thresh.copy()

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
        working_frame: cv2.Mat = cv2.inRange(working_frame, low, high)

        # Find the contours of the thresholded frame.
        contours, _ = cv2.findContours(working_frame, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

        # Find the largest contour, area-wise.
        def bounding_rect_area(contour):
            _, _, w, h = cv2.boundingRect(contour)
            return int(w) * int(h) # Just to make absolutely sure this is an int mult.

        contours = sorted(contours, key=bounding_rect_area, reverse=True)
        if 0 == len(contours):
            return
        largest_contour = contours[0]

        # Tell the user whether the area of the largest contour is greater than the minimum
        # for detection.
        area = bounding_rect_area(largest_contour)
        detected = area >= settings['min_detect_area']

        FONT_SIZE = 0.25
        with_text = working_frame.copy()
        cv2.putText(with_text, f'Detected: {detected}', (0, 30), cv2.FONT_HERSHEY_SIMPLEX, FONT_SIZE, 0xff)

        cv2.imshow(win_name, with_text)
        return working_frame


def setup_thresh_window(window_name: str, native_frame_height: int, native_frame_width: int, settings: dict):
    """Sets up the window which has the trackbars for BGR thresholding (for calibration)."""

    thresh_color_min = settings['thresh_color_min']
    thresh_color_max = settings['thresh_color_max']
    cropping = settings['cropping']

    def on_trackbar(val, color_to_update, dim):
        color_to_update[dim] = val
        disp_thresh_frame(window_name, settings)

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
        disp_thresh_frame(window_name, settings)

    # Create cropping trackbars.
    cv2.createTrackbar('Top cropping', window_name, cropping['top'], native_frame_height, functools.partial(cropping_callback, crop_settings=cropping, crop_direction='top'))
    cv2.createTrackbar('Left cropping', window_name, cropping['left'], native_frame_width, functools.partial(cropping_callback, crop_settings=cropping, crop_direction='left'))
    cv2.createTrackbar('Right cropping', window_name, cropping['right'], native_frame_width, functools.partial(cropping_callback, crop_settings=cropping, crop_direction='right'))
    cv2.createTrackbar('Bottom cropping', window_name, cropping['bottom'], native_frame_height, functools.partial(cropping_callback, crop_settings=cropping, crop_direction='bottom'))

    # Create area detection trackbars.
    MAX_MIN_DETECT_AREA = 96*96 # The whole screen

    def area_detection_callback(val: int, settings: dict):
        settings['min_detect_area'] = val

    cv2.createTrackbar('Min Area for Detection', window_name, settings['min_detect_area'], MAX_MIN_DETECT_AREA, functools.partial(area_detection_callback, settings=settings))


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
    f_type = s.read(size=7).decode()

    print(f'Found mat of size {rows}x{cols}x{channels} of type {f_type}')

    # Read the following data as a matrix of integers with the rows/cols vals.
    mat_data = np.zeros((rows, cols, channels), dtype=np.uint8)
    pre_read = time.time()

    for row in range(0, rows):
        for col in range(0, cols):
            for channel in range(0, channels):
                data_bytes = s.read(size=2)
                mat_data[row, col, channel] = int(data_bytes.decode(), base=16)

    post_read = time.time()
    print(f'time: {post_read - pre_read}')

    # Return the BGR image.
    return (mat_data, f_type)


def handle_received_frame_8uc2(recv_frame: cv2.Mat, settings: dict):
    """Handles the reception of a frame of type 8UC2 from the ESP32."""
    global frame_to_thresh
    global frame_threshed_outside
    global frame_threshed_stop

    # Convert to a color that we can process.
    frame = cv2.cvtColor(recv_frame, cv2.COLOR_BGR5652BGR)

    # If we have read a center, draw it in read on the image.
    if detected_center != -1:
        cv2.line(frame, (detected_center, 0), (detected_center, frame.shape[0]), (0, 0, 255), 5)

    # Blow it up so that it's easier to see.
    big_frame = cv2.resize(frame, (settings['scaled_frame_size']['height'], settings['scaled_frame_size']['width']))
    cv2.imshow('Pre-processed Frame', big_frame)

    # Display the big frame also in HSV.
    big_frame = cv2.cvtColor(big_frame, cv2.COLOR_BGR2HSV)
    cv2.imshow('HSV Frame', big_frame)

    # Display the thresholded frame. If the thresh_frame is None, that means that the frame hasn't been set up yet; therefore,
    # set it up.
    OUTSIDE_THRESH_WINNAME = 'Outside Line Thresholding'
    STOP_THRESH_WINNAME = 'Stop Line Thresholding'

    if frame_to_thresh is None:
        setup_thresh_window(OUTSIDE_THRESH_WINNAME, frame.shape[0], frame.shape[1], settings['outside_thresh'])
        setup_thresh_window(STOP_THRESH_WINNAME, frame.shape[0], frame.shape[1], settings['stop_thresh'])

    frame_hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    frame_to_thresh = frame_hsv

    frame_threshed_outside = disp_thresh_frame(OUTSIDE_THRESH_WINNAME, settings['outside_thresh'])
    frame_threshed_stop = disp_thresh_frame(STOP_THRESH_WINNAME, settings['stop_thresh'])


def main_loop(s: serial.Serial, settings: dict):
    """The main loop for the debugger, which reads the input and displays windows."""

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
                handle_received_frame_8uc2(frame, settings)

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


def load_settings(filename: str) -> dict:
    """Loads settings for the app from the given json file."""
    with open(filename, 'r', encoding='ascii') as f:
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
    setup_white_line_loc_calibration_window(settings)
    main_loop(s, settings)

    # Write-back convenience values to settings.
    with open('debugger_settings.json', 'w', encoding='ascii') as f:
        json.dump(settings, f, indent=4)

    s.close()


if __name__ == '__main__':
    main()
