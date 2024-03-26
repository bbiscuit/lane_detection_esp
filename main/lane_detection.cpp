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


/// @brief Finds the center of the lane in the image.
/// @param mask The binary image.
/// @param start_row Based upon our cropping, we know that allot of the image won't
/// contain any data. Pass this in, so that we don't waste time considering that
/// sector of the image.
/// @return The estimated center column. -1 if it was not found.
inline int get_lane_center(const cv::Mat1b& mask, const uint8_t start_row = 0)
{
    const auto start_tick = xTaskGetTickCount();

    std::vector<std::vector<cv::Point2i>> contours;
    cv::findContours(mask, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    // Find the three largest contours within the mask.
    const size_t num_contours = contours.size();
    if (num_contours < 3)
    {
        return -1;
    }

    // Sort the contours by their area.
    std::sort(contours.begin(), contours.end(), [](const std::vector<cv::Point2i>& a, const std::vector<cv::Point2i>& b){
        cv::Rect2i a_rect = cv::boundingRect(a);
        cv::Rect2i b_rect = cv::boundingRect(b);

        return a_rect.area() > b_rect.area();
    });

    // Find the average of the distance between the centerline
    const cv::Rect2i bold_line = cv::boundingRect(contours[0]); // The largest contour in the image; presumed to be the white line.
    const cv::Rect2i a = cv::boundingRect(contours[1]);
    const cv::Rect2i b = cv::boundingRect(contours[2]);

    const int bold_x = bold_line.x + (bold_line.width >> 1);
    const int a_x = a.x + (a.width >> 1);
    const int b_x = b.x + (b.width >> 1);

    const int dist_a = bold_x - a_x;
    const int dist_b = bold_x - b_x;

    if (dist_a < 0 || dist_b < 0)
    {
        return -1;
    }

    const int center_a = a_x + (dist_a >> 1);
    const int center_b = b_x + (dist_b >> 1);

    const int center = ((center_a + center_b) >> 1);

    const auto end_tick = xTaskGetTickCount();

    //printf("Ticks for lane cntr detection: %ld\n", (end_tick - start_tick));

    return center;
}


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
        cv::Mat thresh;
        cv::inRange(hsv, low, high, thresh);

        const uint8_t center_col = get_lane_center(thresh, top_cropping);
        printf("center %d\n", center_col);

        // Write it to the display.
        cv::resize(thresh, thresh, cv::Size(SCREEN_WIDTH, SCREEN_HEIGHT));
        //lane_detect::debug::send_matrix(thresh);
        write_bin_mat(screen, thresh);

        //const auto end_tick = xTaskGetTickCount();
        //printf("Ticks for thresh_and_disp: %ld\n", (end_tick - start_tick));

        vTaskDelay(1);
    }
}


/// @brief The entry-point.
void app_main(void)
{
    lane_detect::config_cam();

    thresh_and_disp();
}
