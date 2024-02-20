///////////////////////////////////////////////////////////////////////////////////////////////////
/// A file which defines an RTOS task to continually fetch from the ESP-32 camera into a queue of
/// OpenCV matrices.
///
/// Author: Andrew Huffman
///////////////////////////////////////////////////////////////////////////////////////////////////


#pragma once

namespace lane_detect
{
    /// @brief Configures the ESP-32-CAM.
    void config_cam();

    /// @brief A FreeRTOS task which fetches frames from an ESP32-CAM and outputs them as OpenCV
    /// frames.
    /// @param arg Should be of type "TaskParameters." The input queue is not used in this task;
    /// only the output and the max output num.
    void camera_task(void* arg);
}