import serial
import numpy
import cv2

def read_frame(s: serial.Serial):
    """Reads a frame from the serial port."""
    # Busy-wait until we recieve the start bit (1)
    while True:
        bin_data = s.read()
        if bin_data == b'S':
            break
    
    # Read the 32-bit number of rows
    rows = int(s.read(size=4).decode(), base=16)
    cols = int(s.read(size=4).decode(), base=16)
    channels = 3
    print(f'Frame size: ({rows}, {cols})')

    # Read the following data as a matrix of integers with the rows/cols vals.
    mat_data = []
    for _ in range(0, cols):
        curr_row = []
        for __ in range(0, rows):
            curr_pix = []
            for ___ in range(0, channels):
                curr_pix.append(int(s.read(size=2).decode(), base=16))
            curr_row.append(curr_pix)
        mat_data.append(curr_row)

    # Return the BGR image.
    return numpy.array(mat_data)

def display_frames(s: serial.Serial):
    while True:
        frame = read_frame(s)
        print(frame.shape)
        #frame = cv2.resize(frame, (300, 300))
        cv2.imshow('Frame', frame)
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