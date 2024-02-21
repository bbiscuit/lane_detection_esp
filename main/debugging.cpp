#include "debugging.h"

#include <stdio.h>

namespace lane_detect::debug
{

    /// @brief Sends a frame over the serial port for the purposes of testing.
    /// @param frame The OpenCV matrix to send.
    void send_matrix(const cv::Mat& frame)
    {
        #ifdef DEBUG_MODE
        // Begin transmission
        printf("START"); // Start of transmission

        // Transmit the number of rows and columns
        const int channels = frame.channels();

        printf("%04x", frame.rows);
        printf("%04x", frame.cols);
        printf("%04x", channels);

        // Transmit the data of the frame.
        for (int row = 0; row < frame.rows; row++) 
        {
            for (int col = 0; col < frame.cols; col++)
            {
                for (int channel = 0; channel < channels; channel++)
                {
                    printf("%02x", frame.at<uint8_t>(row, col, channel));
                }
            }
        }

        // End transmission and flush
        printf("E\n");
        fflush(stdout);
        #endif
    }

}
