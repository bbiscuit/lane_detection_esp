///////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper functions for communicating with the LCD screen.
///
/// Author: Andrew Huffman
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// Stdlib imports
#include <vector>
#include <string>

// LCD imports
#include "ssd1306.h"

// Opencv Imports
#undef EPS
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#define EPS 192

namespace lane_detect
{
    /// @brief Writes a number of lines to the LCD screen, from top to bottom.
    /// @param screen The screen to write to.
    /// @param lines The lines to write to the screen.
    void write_val_lcd(SSD1306_t& screen, std::vector<std::string>& lines);


    /// @brief Writes an OpenCV matrix to the screen.
    /// @param screen The screen to write to.
    /// @param bin_mat The matrix to write. Should be a binary mask.
    /// @param vert_bar If this is a positive value, writes a vertical bar in one column on the screen.
    void write_bin_mat(SSD1306_t& screen, const cv::Mat& bin_mat, const int vert_bar = -1);
}
