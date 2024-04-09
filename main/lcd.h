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
    /// @brief The native width of the screen.
    constexpr uint8_t SCREEN_WIDTH = 128;


    /// @brief The native heigth of the screen.
    constexpr uint8_t SCREEN_HEIGHT = 64;

    /// @brief If passed into one of the print functions as a row, prints oen after the previous.
    constexpr int APPEND_ROW = -1;


    /// @brief Draws a string to the LCD screen.
    /// @param screen The screen to draw to.
    /// @param string The string.
    /// @param row The row of the screen to draw at.
    void lcd_draw_string(SSD1306_t& screen, std::string& string, int row = APPEND_ROW);


    /// @brief Writes a number of lines to the LCD screen, from top to bottom.
    /// @param screen The screen to write to.
    /// @param lines The lines to write to the screen.
    /// @param start_row The row to start printing on.
    void lcd_draw_string(SSD1306_t& screen, std::vector<std::string>& lines, int start_row = APPEND_ROW);


    /// @brief Draws a string of this format in a row: "{preamble} {data}"
    /// @param screen The scren to print to.
    /// @param preamble The preamble.
    /// @param data The data.
    /// @param row The row on which to print.
    void lcd_draw_data(SSD1306_t& screen, std::string preamble, int data, int row = APPEND_ROW);


    /// @brief Writes an OpenCV matrix to the screen. This also clears the screen.
    /// @param screen The screen to write to.
    /// @param bin_mat The matrix to write. Should be a binary mask.
    void lcd_draw_matrix(SSD1306_t& screen, const cv::Mat& bin_mat);
}
