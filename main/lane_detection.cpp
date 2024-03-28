// Opencv Imports
#undef EPS
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#define EPS 192

// SSD1306 Imports
#include "ssd1306.h"


// Stdlib imports imports
#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <algorithm>


// Esp imports
#include <esp_err.h>
#include <esp_spiffs.h>
#include <esp_log.h>
#include "sdkconfig.h"
#include "esp_camera.h"
#include "esp_log.h"


// FreeRTOS imports
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"


// In-project imports
#include "thread_safe_queue.h"
#include "common.h"
#include "camera_task.h"
#include "debugging.h"
#include "params.h"


static char TAG[]="lane_detection";


// This is necessary because it allows ESP-IDF to find the main function,
// even though C++ mangles the function name.
extern "C" {
void app_main(void);
}


/// @brief Writes a binary matrix to a provided LCD screen.
/// @param screen The LCD screen to write to.
/// @param bin_mat The binary matrix to write.
void write_bin_mat(SSD1306_t& screen, const cv::Mat& bin_mat, const int vert_bar = -1)
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


/// @brief Writes a parameter to the LCD screen.
/// @param screen The screen to write to.
/// @param lines The lines to write to the screen.
void write_val_lcd(SSD1306_t& screen, std::vector<std::string>& lines)
{
    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string& str = lines[i];
        ssd1306_display_text(&screen, i, str.data(), str.size(), false);
    }
}


/// @brief Finds the center of the lane in the image.
/// @param mask The binary image.
/// @param start_row Based upon our cropping, we know that allot of the image won't
/// contain any data. Pass this in, so that we don't waste time considering that
/// sector of the image.
/// @return The project center column.
inline uint8_t get_lane_center(const cv::Mat1b& mask, const uint8_t start_row = 0)
{
    //const auto start_tick = xTaskGetTickCount();

    uint16_t result = 0; // The center column.
    uint16_t sums[mask.cols] = {0};
    
    // Sum up the columns into the "sums" array,
    for (uint16_t row = start_row; row < mask.rows; row++)
    {
        for (uint16_t col = 0; col < mask.cols; col++)
        {
            sums[col] += mask.at<uint8_t>(row, col);
        }
    }

    // Split the image into two halves -- the left half should contain the left dotted line,
    // the right half should contain the right solid line.
    const uint8_t half = (mask.cols >> 1);

    // Find the max of that which is on the left side of the image. Call that the dotted line.
    uint16_t dotted_col = 0;
    uint16_t max = 0;

    for (uint16_t col = 0; col < half; col++)
    {
        const uint16_t val = sums[col];
        if (val > max)
        {
            max = val;
            dotted_col = col;
        }
    }

    // Find the max of that which is on the right side of the image. Call that the solid line.
    uint16_t solid_col = 0;
    max = 0;

    for (uint16_t col = half; col < mask.cols; col++)
    {
        const uint16_t val = sums[col];
        if (val > max)
        {
            max = val;
            solid_col = col;
        }
    }

    // The center of the lane is the average of the right and left lines.
    result = ((dotted_col + solid_col) >> 1);

    //const auto end_tick = xTaskGetTickCount();

    //printf("Ticks for get_lane_center: %ld\n", (end_tick - start_tick));

    return result;
}


/// @brief Finds the contour of the solid line, assumed to be the largest contour in the given mask.
/// @param mask The binary image.
/// @return The contour of the solid line.
inline std::vector<cv::Point2i> get_solid_line(const cv::Mat1b& mask)
{
    // Get the contours.
    std::vector<std::vector<cv::Point2i>> contours;
    cv::findContours(mask, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    if (0 == contours.size())
    {
        return std::vector<cv::Point2i>();
    }

    // Find the largest contour, assume that's the solid line.
    std::sort(contours.begin(), contours.end(), [](const std::vector<cv::Point2i>& a, const std::vector<cv::Point2i>& b)
    {
        const cv::Rect2i a_rect = cv::boundingRect(a);
        const cv::Rect2i b_rect = cv::boundingRect(b);

        return a_rect.area() > b_rect.area();
    });
    const auto& solid_line = contours[0];
    return solid_line;
}


/// @brief Crops a captured frame based upon cropping parameters.
/// @param frame The frame to crop. Modified.
/// @param top The number of pixels on the top to crop off.
/// @param bottom The number of pixels on the bottom to crop off.
/// @param left The number of pixels on the left to crop off.
/// @param right The number of pixels on the right to crop off.
inline void apply_cropping(
    cv::Mat& frame, 
    const uint16_t top, 
    const uint16_t bottom, 
    const uint16_t left, 
    const uint16_t right
)
{
    // Do the top cropping.
    if (top > 0)
    {
        cv::rectangle(frame, cv::Rect2i(cv::Point2i(0, 0), cv::Point2i(frame.cols, top)), cv::Scalar(0, 0, 0), -1);
    }

    // Do the bottom cropping.
    if (bottom > 0)
    {
        cv::rectangle(frame, cv::Rect2i(cv::Point2i(0, frame.rows), cv::Point2i(frame.cols, frame.rows - bottom)), cv::Scalar(0, 0, 0), -1);
    }

    // Do the left cropping.
    if (left > 0)
    {
        cv::rectangle(frame, cv::Rect2i(cv::Point2i(0, 0), cv::Point2i(left, frame.rows)), cv::Scalar(0, 0, 0), -1);
    }

    // Do the right cropping.
    if (right > 0)
    {
        cv::rectangle(frame, cv::Rect2i(cv::Point2i(frame.cols, 0), cv::Point2i(frame.cols - right, frame.rows)), cv::Scalar(0, 0, 0), -1);
    }
}


/// @brief The main driver loop.
inline void main_loop()
{
    camera_fb_t* fb = nullptr;

    // Init screen
    constexpr uint8_t SCREEN_WIDTH = 128;
    constexpr uint8_t SCREEN_HEIGHT = 64;

    SSD1306_t screen; // The screen device struct.
    i2c_master_init(&screen, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&screen, SCREEN_WIDTH, SCREEN_HEIGHT);

    while (true)
    {
        // Crop the current frame so that it will fit on the screen.
        cv::Mat working_frame = lane_detect::get_frame(&fb);
        if (working_frame.size[0] == 0)
        {
            vTaskDelay(1);
            continue;
        }

        //lane_detect::debug::send_matrix(working_frame);
        //const auto start_tick = xTaskGetTickCount();
        
        // Get into the right color space for thresholding.
        cv::Mat bgr;
        cv::cvtColor(working_frame, bgr, cv::COLOR_BGR5652BGR);
        cv::Mat hsv;
        cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV, 3);

        // Add a black rectangle over parts of the image which we don't want to be considered
        // in the threshold.
        apply_cropping(hsv, top_cropping, bottom_cropping, left_cropping, right_cropping);

        // Perform the threshold.
        const auto low = cv::Scalar(thresh_min_hue, thresh_min_sat, thresh_min_val);
        const auto high = cv::Scalar(thresh_max_hue, thresh_max_sat, thresh_max_val);
        cv::Mat1b thresh;
        cv::inRange(hsv, low, high, thresh);

        //const uint8_t center_col = get_lane_center(thresh, top_cropping);
        const auto solid_line = get_solid_line(thresh);

        const auto solid_line_rect = cv::boundingRect(solid_line);
        const auto solid_line_col = solid_line_rect.x + (solid_line_rect.width >> 1);

        printf("solid %d\n", solid_line_col);

        // Draw a solid line where the line has been detected.
        if (-1 != solid_line_col) 
        {
            cv::line(thresh, cv::Point2i(solid_line_col, 0), cv::Point2i(solid_line_col, thresh.rows), 0xff);
        }
        
        // Write it to the display.
        cv::resize(thresh, thresh, cv::Size(SCREEN_WIDTH, SCREEN_HEIGHT));
        //lane_detect::debug::send_matrix(thresh);

        write_bin_mat(screen, thresh);
        std::vector<std::string> disp = {std::string("solid x: ") + std::to_string(solid_line_col)};
        write_val_lcd(screen, disp);

        //const auto end_tick = xTaskGetTickCount();
        //printf("Ticks for thresh_and_disp: %ld\n", (end_tick - start_tick));

        vTaskDelay(1);
    }
}


/// @brief The entry-point.
void app_main(void)
{
    lane_detect::config_cam();

    main_loop();
}
