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
OUTSIDE_THRESH_WINNAME = 'Outside Line Thresholding'
STOP_THRESH_WINNAME = 'Stop Line Thresholding'
RED_LINE_CALIBRATION_WIN_TITLE = 'Stop Line Calibration'
MAX_MIN_DETECT_AREA = 96*96 # The minimum detect area for the line areas.


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


class FrameHandler:
    """A class which takes a received frame and makes the different windows."""
    def __init__(self, settings: dict):
        self.frame_to_thresh: cv2.Mat = None
        self._frame_threshed_outside: cv2.Mat = None
        self._frame_threshed_stop: cv2.Mat = None
        self.settings = settings

    def handle_received_frame_8uc2(self, recv_frame: cv2.Mat):
        """Handles the reception of a frame of type 8UC2 from the ESP32."""

        # Convert to a color that we can process.
        frame = cv2.cvtColor(recv_frame, cv2.COLOR_BGR5652BGR)

        # Blow it up so that it's easier to see.
        big_frame = cv2.resize(
            frame,
            (
                self.settings['scaled_frame_size']['height'],
                self.settings['scaled_frame_size']['width']
            )
        )
        cv2.imshow('Pre-processed Frame', big_frame)

        # Display the big frame also in HSV.
        big_frame = cv2.cvtColor(big_frame, cv2.COLOR_BGR2HSV)
        cv2.imshow('HSV Frame', big_frame)

        # Display the thresholded frame. If the thresh_frame is None, that means that the frame
        # hasn't been set up yet; therefore, set it up.


        if self.frame_to_thresh is None:
            self.setup_thresh_window(
                OUTSIDE_THRESH_WINNAME,
                frame.shape[0],
                frame.shape[1],
                self.settings['outside_thresh']
            )
            self.setup_thresh_window(
                STOP_THRESH_WINNAME,
                frame.shape[0],
                frame.shape[1],
                self.settings['stop_thresh']
            )

        frame_hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        self.frame_to_thresh = frame_hsv

        self._frame_threshed_outside = self.disp_thresh_frame(
            OUTSIDE_THRESH_WINNAME,
            self.settings['outside_thresh']
        )
        self._frame_threshed_stop = self.disp_thresh_frame(
            STOP_THRESH_WINNAME,
            self.settings['stop_thresh']
        )
        self._disp_red_line_loc_calibration_frame()


    def setup_white_line_loc_calibration_window(self):
        """Sets up a window which, when clicked, records detected data about the white line.
        This is taken to be its "natural state," where the line should be. These values are
        then compared on the ESP-32 with the detected values, and changes are made such that
        the line is closer to the ideal."""

        def on_click(event, _, __, ___, ____):
            # We only care about the click event.
            if event != cv2.EVENT_LBUTTONDOWN:
                return

            # If we don't yet have an image to extract data from, report and exit.
            if self._frame_threshed_outside is None:
                print('Have not receieved data yet.')
                return

            # Get the largest contour.
            line = get_largest_contour(self._frame_threshed_outside)
            if line is None:
                print('Did not find a line.')
                return

            # Extract data from the bounding-rect and write it to the settings.
            x_line, _, w_line, _ = cv2.boundingRect(line)
            self.settings['outside_line_data']['x'] = x_line + (w_line // 2)
            print('Recorded line data.')


        img = np.ones((LINE_LOC_WIN_ROWS, LINE_LOC_WIN_COLS, 3), np.uint8) * 255
        font = cv2.FONT_HERSHEY_SIMPLEX
        font_scale = 0.5
        color = (255, 0, 255)  # BGR color
        thickness = 1
        img = cv2.putText(
            img,
            LINE_LOC_BUTTON_TEXT,
            (50, 50),
            font,
            font_scale,
            color,
            thickness,
            cv2.LINE_AA
        )
        cv2.imshow(LINE_LOC_WIN_TITLE, img)
        cv2.setMouseCallback(LINE_LOC_WIN_TITLE, on_click)


    def setup_red_line_loc_calibration_window(self):
        """Sets up a few sliders for red-line detection."""

        pertinent_settings = self.settings['stop_thresh']['detect_loc']
        cv2.namedWindow(RED_LINE_CALIBRATION_WIN_TITLE)

        def callback(val, settings, subscript):
            settings[subscript] = val

        cv2.createTrackbar(
            'Stop Line Y Position',
            RED_LINE_CALIBRATION_WIN_TITLE,
            pertinent_settings['y'],
            96,
            functools.partial(
                callback,
                settings=pertinent_settings,
                subscript='y'
            )
        )
        cv2.createTrackbar(
            'Stop Line Tolerance Square Radius',
            RED_LINE_CALIBRATION_WIN_TITLE,
            pertinent_settings['radius'],
            50,
            functools.partial(
                callback,
                settings=pertinent_settings,
                subscript='radius'
            )
        )


    def disp_thresh_frame(self, win_name: str, thresh_settings: dict) -> cv2.Mat:
        """Displays a thresholded version of the thresh_frame image, given the parameters set in
        the debugger. The thresholded frames is written back to the class."""

        if self.frame_to_thresh is not None:
            working_frame = self.frame_to_thresh.copy()

            # Perform cropping of the image based upon the cropping parameters.
            cropping = thresh_settings['cropping']
            rows, cols, _ = working_frame.shape

            top_cropping = cropping['top']
            if top_cropping > 0:
                cv2.rectangle(working_frame, (0, 0), (cols, top_cropping), (0, 0, 0), -1)

            bottom_cropping = cropping['bottom']
            if bottom_cropping > 0:
                cv2.rectangle(
                    working_frame,
                    (0, rows),
                    (cols, rows - bottom_cropping),
                    (0, 0, 0),
                    -1
                )

            left_cropping = cropping['left']
            if left_cropping > 0:
                cv2.rectangle(working_frame, (0, 0), (left_cropping, rows), (0, 0, 0), -1)

            right_cropping = cropping['right']
            if right_cropping > 0:
                cv2.rectangle(
                    working_frame,
                    (cols, 0),
                    (cols - right_cropping, rows),
                    (0, 0, 0),
                    -1
                )

            # Perform color thresholding.
            thresh_color_min = thresh_settings['thresh_color_min']
            thresh_color_max = thresh_settings['thresh_color_max']

            low = (
                thresh_color_min['hue'],
                thresh_color_min['saturation'],
                thresh_color_min['value']
            )
            high = (
                thresh_color_max['hue'],
                thresh_color_max['saturation'],
                thresh_color_max['value']
            )
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
            detected = area >= thresh_settings['min_detect_area']

            font_size = 0.25
            with_text = working_frame.copy()
            cv2.putText(
                with_text,
                f'Detected: {detected}',
                (0, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                font_size,
                0xff
            )

            cv2.imshow(win_name, with_text)
            return working_frame


    def setup_thresh_window(
            self,
            window_name: str,
            native_frame_height: int,
            native_frame_width: int,
            thresh_settings: dict
    ):
        """Sets up the window which has the trackbars for BGR thresholding (for calibration)."""

        thresh_color_min = thresh_settings['thresh_color_min']
        thresh_color_max = thresh_settings['thresh_color_max']
        cropping = thresh_settings['cropping']

        def on_trackbar(val, color_to_update, dim):
            color_to_update[dim] = val
            self.disp_thresh_frame(window_name, thresh_settings)

        cv2.namedWindow(window_name)
        cv2.createTrackbar(
            "Min Hue",
            window_name,
            thresh_color_min["hue"],
            179,
            functools.partial(on_trackbar, color_to_update=thresh_color_min, dim="hue")
        )
        cv2.createTrackbar(
            "Min Saturation",
            window_name,
            thresh_color_min["saturation"],
            255,
            functools.partial(on_trackbar, color_to_update=thresh_color_min, dim="saturation")
        )
        cv2.createTrackbar(
            "Min Value",
            window_name,
            thresh_color_min["value"],
            255,
            functools.partial(on_trackbar, color_to_update=thresh_color_min, dim="value")
        )

        cv2.createTrackbar(
            "Max Hue",
            window_name,
            thresh_color_max["hue"],
            179,
            functools.partial(on_trackbar, color_to_update=thresh_color_max, dim="hue")
        )
        cv2.createTrackbar(
            "Max Saturation",
            window_name,
            thresh_color_max["saturation"],
            255,
            functools.partial(on_trackbar, color_to_update=thresh_color_max, dim="saturation")
        )
        cv2.createTrackbar(
            "Max Value",
            window_name,
            thresh_color_max["value"],
            255,
            functools.partial(on_trackbar, color_to_update=thresh_color_max, dim="value")
        )

        def cropping_callback(val, crop_settings: dict, crop_direction: str):
            """The callback for trackbars related to image cropping."""
            crop_settings[crop_direction] = val
            self.disp_thresh_frame(window_name, thresh_settings)

        # Create cropping trackbars.
        cv2.createTrackbar(
            'Top cropping',
            window_name,
            cropping['top'],
            native_frame_height,
            functools.partial(cropping_callback, crop_settings=cropping, crop_direction='top')
        )
        cv2.createTrackbar(
            'Left cropping',
            window_name,
            cropping['left'],
            native_frame_width,
            functools.partial(cropping_callback, crop_settings=cropping, crop_direction='left')
        )
        cv2.createTrackbar(
            'Right cropping',
            window_name,
            cropping['right'],
            native_frame_width,
            functools.partial(cropping_callback, crop_settings=cropping, crop_direction='right')
        )
        cv2.createTrackbar(
            'Bottom cropping',
            window_name,
            cropping['bottom'],
            native_frame_height,
            functools.partial(cropping_callback, crop_settings=cropping, crop_direction='bottom')
        )

        # Create area detection trackbars.


        def area_detection_callback(val: int, settings: dict):
            settings['min_detect_area'] = val

        cv2.createTrackbar(
            'Min Area for Detection',
            window_name,
            thresh_settings['min_detect_area'],
            MAX_MIN_DETECT_AREA,
            functools.partial(area_detection_callback, settings=thresh_settings)
        )

    def _disp_red_line_loc_calibration_frame(self):
        """Displays the frame for red line calibration in the same window as the trackbars."""
        # Draw the detection rectangle on the frame.
        detect_y = self.settings["stop_thresh"]["detect_loc"]["y"]
        detect_radius = self.settings["stop_thresh"]["detect_loc"]["radius"]

        to_disp = self._frame_threshed_stop.copy()
        to_disp = cv2.cvtColor(to_disp, cv2.COLOR_GRAY2BGR)

        top_coord = (0, detect_y - detect_radius)
        bottom_coord = (to_disp.shape[1], detect_y + detect_radius)

        to_disp = cv2.rectangle(to_disp, top_coord, bottom_coord, (0, 0, 255), -1)

        # If the area of the red line is above the minimum, and the line intersects the rectangle,
        # then mark a detection.
        detected = False

        cv2.putText(
            to_disp,
            f'Detected: {detected}',
            (5, 10),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.25,
            (255,255,255),
            1,
            cv2.LINE_AA
        )

        cv2.imshow(RED_LINE_CALIBRATION_WIN_TITLE, to_disp)


def serial_reader(s: serial.Serial) -> tuple[cv2.Mat, str]:
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


def main_loop(s: serial.Serial, frame_handler: FrameHandler):
    """The main loop for the debugger, which reads the input and displays windows."""

    print("Starting main loop...")
    # Create the thread for reading the frame.
    frame_queue = queue.Queue()
    def frame_reader():
        result = serial_reader(s)
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
                frame_handler.handle_received_frame_8uc2(frame)

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

    # Setup the frame handler.
    frame_handler = FrameHandler(settings)

    # The BGR threshold for the image.
    frame_handler.setup_white_line_loc_calibration_window()
    frame_handler.setup_red_line_loc_calibration_window()
    main_loop(s, frame_handler)

    # Write-back convenience values to settings.
    with open('debugger_settings.json', 'w', encoding='ascii') as f:
        json.dump(settings, f, indent=4)

    s.close()


if __name__ == '__main__':
    main()
