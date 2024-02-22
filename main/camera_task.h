///////////////////////////////////////////////////////////////////////////////////////////////////
/// A file which defines an RTOS task to continually fetch from the ESP-32 camera into a queue of
/// OpenCV matrices.
///
/// Author: Andrew Huffman
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#undef EPS
#include "opencv2/core.hpp"
#define EPS 192
#include "esp_camera.h"

namespace lane_detect
{
    /// @brief Configures the ESP-32-CAM.
    void config_cam();

    /// @brief A FreeRTOS task which fetches frames from an ESP32-CAM and outputs them as OpenCV
    /// frames.
    /// @param arg Should be of type "TaskParameters." The input queue is not used in this task;
    /// only the output and the max output num.
    void camera_task(void* arg);


    /// @brief Gets a frame from the ESP-32 camera and interprets it as an OpenCV matrix.
    /// @param fb The frame buffer. If this is not nullptr, this will be freed back to the ESP-32
    /// cam prior to overwriting. Note that this pointer ought to be freed before it ever
    /// goes out of scope.
    /// @param mat The matrix into which the frame is read. Note that the data from `fb` was not
    /// copied; just the reference. So if fb is freed this Mat is invalidated.
    cv::Mat get_frame(camera_fb_t* fb);
}