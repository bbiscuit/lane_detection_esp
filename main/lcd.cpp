#include "lcd.h"


/// @brief Writes a parameter to the LCD screen.
/// @param screen The screen to write to.
/// @param lines The lines to write to the screen.
void lane_detect::lcd_draw_string(SSD1306_t& screen, std::vector<std::string>& lines)
{
    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string& str = lines[i];
        ssd1306_display_text(&screen, i, str.data(), str.size(), false);
    }
}


/// @brief Writes a binary matrix to a provided LCD screen.
/// @param screen The LCD screen to write to.
/// @param bin_mat The binary matrix to write.
void lane_detect::lcd_draw_matrix(SSD1306_t& screen, const cv::Mat& bin_mat)
{
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
