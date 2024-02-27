#include "debugging.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

namespace lane_detect::debug
{

    /// @brief Sends a frame over the serial port for the purposes of testing.
    /// @param frame The OpenCV matrix to send.
    void send_matrix(const cv::Mat& frame)
    {
        #ifdef DEBUG_MODE
        // Begin transmission
        printf("START"); // Start of transmission

        printf("%04x", frame.rows);
        printf("%04x", frame.cols);
        printf("%04x", frame.channels());

        // Transmit the data of the frame.
        for (int row = 0; row < frame.rows; row++) 
        {
            for (int col = 0; col < frame.cols; col++)
            {
                printf("%04x", frame.at<uint16_t>(row, col));
            }
            vTaskDelay(1); // To avoid the watchdog on especially large matrices.
        }

        // End transmission and flush
        printf("E\n");
        fflush(stdout);
        #endif
    }

}
