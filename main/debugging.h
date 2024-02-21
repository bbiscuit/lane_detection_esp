///////////////////////////////////////////////////////////////////////////////////////////////////
/// Contains debugging functions for the project.
///
/// Author: Andrew Huffman
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#undef EPS
#include "opencv2/core.hpp"
#define EPS 192

// Remove this line to turn all debugging functions into stubs.
#define DEBUG_MODE 1

namespace lane_detect::debug
{
    /// @brief Sends an OpenCV matrix over the serial port of the ESP-32, for debugging purposes.
    /// @param mat The matrix to send.
    void send_matrix(const cv::Mat& mat);
}
