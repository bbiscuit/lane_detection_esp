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
void write_bin_mat(SSD1306_t& screen, const cv::Mat& bin_mat)
{
    for (uint8_t row = 0; row < bin_mat.rows; row++)
    {
        for (uint8_t col = 0; col < bin_mat.cols; col++)
        {
            bool invert = (0 == bin_mat.at<uint8_t>(row, col));
            _ssd1306_pixel(&screen, col, row, invert);
        }
    }
    ssd1306_show_buffer(&screen);
}


/// @brief Runs Canny edge detection on a frame from the input Queue, displays it on
/// the connected screen, and also pushes it onto an output Queue for debugging purposes.
/// @param arg The input/output queues.
inline void canny_and_disp()
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

        lane_detect::debug::send_matrix(working_frame);
        cv::resize(working_frame, working_frame, cv::Size(SCREEN_WIDTH, SCREEN_HEIGHT));

        // Prepare the image for Canny
        cv::cvtColor(working_frame, working_frame, cv::COLOR_BGR5652GRAY);
        cv::blur(working_frame, working_frame, cv::Size(3, 3));

        // Run Canny on the image.
        int lowThresh = 80;
        int kernSize = 3;
        cv::Canny(working_frame, working_frame, lowThresh, 4 * lowThresh, kernSize);

        lane_detect::debug::send_matrix(working_frame);

        // Write it to the display.
        write_bin_mat(screen, working_frame);
        vTaskDelay(1);
    }
}


/// @brief Runs Canny edge detection on a frame from the input Queue, displays it on
/// the connected screen, and also pushes it onto an output Queue for debugging purposes.
/// @param arg The input/output queues.
inline void thresh_and_disp()
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

        lane_detect::debug::send_matrix(working_frame);
        
        // Get into the right color space for thresholding.
        cv::Mat bgr;
        cv::cvtColor(working_frame, bgr, cv::COLOR_BGR5652BGR);
        cv::Mat hsv;
        cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV, 3);

        // Add a black rectangle over parts of the image which we don't want to be considered
        // in the threshold.
        cv::rectangle(hsv, cv::Rect2i(cv::Point2i(0, 0), cv::Point2i(hsv.cols, crop_row)), cv::Scalar(0, 0, 0), -1);

        // Perform the threshold.
        const auto low = cv::Scalar(thresh_min_hue, thresh_min_sat, thresh_min_val);
        const auto high = cv::Scalar(thresh_max_hue, thresh_max_sat, thresh_max_val);
        cv::Mat thresh;
        cv::inRange(hsv, low, high, thresh);


        // Write it to the display.
        cv::resize(thresh, thresh, cv::Size(SCREEN_WIDTH, SCREEN_HEIGHT));
        lane_detect::debug::send_matrix(thresh);
        write_bin_mat(screen, thresh);
        vTaskDelay(1);
    }
}


inline void vals_test()
{
    camera_fb_t* fb = nullptr;

    constexpr uint8_t cols = 90;
    constexpr uint8_t rows = 90;

    while (true)
    {
        fb = esp_camera_fb_get();
        auto buf = (uint16_t*)fb->buf;

        for (uint8_t col = 0; col < cols; col++)
        {
            uint32_t sum = 0;
            for (uint8_t row = 0; row < rows; row++)
            {
                //uint16_t pixel_val = buf[row + rows * ]
            }
        }

        esp_camera_fb_return(fb);
    }
}


/// @brief The entry-point.
void app_main(void)
{
    lane_detect::config_cam();

    thresh_and_disp();
}
