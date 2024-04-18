# ESP-Image-USB
This repository contains the source code for the "senior" safe-town robot, which seeks to navigate
its course using image processing techniques with an OpenCV fork rather than IR sensors.

## Project Structure
`main/`\
&nbsp; &nbsp;|___ `lane_detection.cpp` - Contains the driver code.\
&nbsp; &nbsp;|___ `lcd.(h|cpp)` - Contains helper functions for writing data to an attached LCD.\
&nbsp; &nbsp;|___ `params.h` - See debugger section. Contains configured constants for detection.\
&nbsp; &nbsp;|___ `camera_task.(h|cpp)` - Contains helper functions for camera/OpenCV interfacing.\
\
`debugger.py` - The debugger application. See debugger section.\
`gen_params.py` - Generates debugger parameters. See debugger section.

## Build Instructions
This project uses the ESP-IDF toolchain for building and flashing the ESP-32 CAM. Therefore,
installing that toolchain is a prerequisite to building this project. Install instructions may
be found [here](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).
The easiest way to install the toolchain is to install it through an IDE, such as the VSCode
extension.

Once the ESP-IDF toolchain has been installed, building ought to be as simple as running the
command `idf.py build` from the terminal (or, if using an extension, by clicking the "build"
button). The board may then be flashed by running `idf.py -p <serial port of ESP32-CAM> flash`.
It's important to note that because of hardware limitations the ESP-32 cannot be flashed while
the LCD screen is connected to the board, so make sure that it is removed prior to running
`idf.py flash`.

## Using the Debugger
Debugger.py is a Python script which allows environmental parameters to be configured for the
detection done on the ESP-32 CAM. _Finish!_

## Transmission to the Junior Robot
The define `UART_NUM` specifies the UART port which the ESP-32 transmits its control data over.
This includes a steering parameter and whether or not the stop-line has been detected. The protocol
for these is specified in the table below. The character format for the transmission is ASCII, and
all transmitted numbers are converted to ASCII prior to the send.

| Transmission | Format | Description |
| ------------ | ------ | ----------- |
| Steering | `D<val>E` | The distance between the position of the white line detected and its "ideal" value. A negative value if the car is too far to the right; a positive value if the car is too far to the left. |
| Stop Detect | `S<val>E` | Whether or not the stop line has been detected. A 1 if detected, 0 otherwise. |
