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

## Transmission to the Junior Robot
The define `UART_NUM` specifies the UART port which the ESP-32 transmits its control data over.
This includes a steering parameter and whether or not the stop-line has been detected. The protocol
for these is specified in the table below. The character format for the transmission is ASCII, and
all transmitted numbers are converted to ASCII prior to the send.

| Transmission | Format | Description |
| ------------ | ------ | ----------- |
| Steering | `D<val>E` | The distance between the position of the white line detected and its "ideal" value. A negative value if the car is too far to the right; a positive value if the car is too far to the left. |
| Stop Detect | `S<val>E` | Whether or not the stop line has been detected. A 1 if detected, 0 otherwise. |
