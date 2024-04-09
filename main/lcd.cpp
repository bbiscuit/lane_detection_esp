#include "lcd.h"

int curr_row = 0;


void lane_detect::lcd_draw_string(SSD1306_t& screen, std::string& string, int row)
{
    if (-1 == row)
    {
        row = curr_row;
    }

    ssd1306_display_text(&screen, row, string.data(), string.size(), false);
    curr_row = row + 1;
}


/// @brief Writes a parameter to the LCD screen.
/// @param screen The screen to write to.
/// @param lines The lines to write to the screen.
void lane_detect::lcd_draw_string(SSD1306_t& screen, std::vector<std::string>& lines, int start_row)
{
    if (-1 == start_row)
    {
        start_row = curr_row;
    }

    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string& str = lines[i];
        lcd_draw_string(screen, str, start_row + i);
    }
    curr_row = lines.size() + start_row;
}


void lane_detect::lcd_draw_data(SSD1306_t& screen, std::string preamble, int data, int row)
{
    std::string arg = preamble + " " + std::to_string(data);
    lcd_draw_string(screen, arg, row);
}


void lane_detect::lcd_draw_data(SSD1306_t& screen, std::string preamble, bool data, int row)
{
    std::string arg = preamble + " " + (data ? "true" : "false");
    lcd_draw_string(screen, arg, row);
}


/// @brief Writes a binary matrix to a provided LCD screen.
/// @param screen The LCD screen to write to.
/// @param bin_mat The binary matrix to write.
void lane_detect::lcd_draw_matrix(SSD1306_t& screen, const cv::Mat& bin_mat)
{
    curr_row = 0;
    for (uint8_t row = 0; row < bin_mat.rows; row++)
    {
        for (uint8_t col = 0; col < bin_mat.cols; col++)
        {
            const bool invert = (0 == bin_mat.at<uint8_t>(row, col));
            _ssd1306_pixel(&screen, col, row, invert);
        }
    }
    ssd1306_show_buffer(&screen);
}
