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
    void write_val_lcd(SSD1306_t& screen, std::vector<std::string>& lines);
    void write_bin_mat(SSD1306_t& screen, const cv::Mat& bin_mat, const int vert_bar = -1);
}
