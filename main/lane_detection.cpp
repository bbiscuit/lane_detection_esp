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
#include "lcd.h"


static char TAG[]="lane_detection";

// If this is a "1," then send the raw image from the ESP-32 over the serial port. If 0, don't.
#define CALIBRATION_MODE 0


// This is necessary because it allows ESP-IDF to find the main function,
// even though C++ mangles the function name.
extern "C" {
void app_main(void);
}

using contour_t = std::vector<cv::Point2i>;


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
inline contour_t get_solid_line(const cv::Mat1b& mask)
{
    // Get the contours.
    std::vector<contour_t> contours;
    cv::findContours(mask, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    if (0 == contours.size())
    {
        return contour_t();
    }

    // Find the largest contour, assume that's the solid line.
    std::sort(contours.begin(), contours.end(), [](const contour_t& a, const contour_t& b)
    {
        const cv::Rect2i a_rect = cv::boundingRect(a);
        const cv::Rect2i b_rect = cv::boundingRect(b);

        return a_rect.area() > b_rect.area();
    });
    const auto& solid_line = contours[0];
    return solid_line;
}


inline contour_t get_stop_line(const cv::Mat1b& mask)
{
    // Get the contours.
    std::vector<contour_t> contours;
    cv::findContours(mask, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    if (0 == contours.size())
    {
        return contour_t();
    }

    // Find the largest contour, assume that's the solid line.
    std::sort(contours.begin(), contours.end(), [](const contour_t& a, const contour_t& b)
    {
        const cv::Rect2i a_rect = cv::boundingRect(a);
        const cv::Rect2i b_rect = cv::boundingRect(b);

        return a_rect.area() > b_rect.area();
    });
    const auto& solid_line = contours[0];
    return solid_line;
}


/// @brief Gets the slope through the solid line.
/// @param contour The contour of the solid line.
/// @return The slope.
inline float get_slope(const contour_t& contour)
{
    // Find the furthest left and furthest right point.
    cv::Point2i leftmost(INT_MAX, 0);
    cv::Point2i rightmost(INT_MIN, 0);

    for (const auto& point : contour)
    {
        // Check for leftmost
        if (point.x < leftmost.x)
        {
            leftmost = point;
        }

        // Check for rightmost
        if (point.x > rightmost.x)
        {
            rightmost = point;
        }
    }


    // Since we know that the solid line is approximately "line-shaped," an approximation
    // of the slope should be just rise over run with these.
    const float rise = leftmost.y - rightmost.y;
    const float run = leftmost.x - rightmost.x;

    float slope;
    if (run != 0)
    {
        slope = rise/run;
    }
    else
    {
        slope = INFINITY;
    }

    return slope;
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


/// @brief Finds the outside line and extracts parameters.
/// @param hsv The frame, in HSV, to extract data from.
/// @param thresh The thresholded frame. Output param.
/// @param center_point The centerpoint of the detected line. Output param.
/// @param slope The slope of the detected line. Output param.
void outside_line_detection(cv::Mat& hsv, cv::Mat1b& thresh, cv::Point2i& center_point, float& slope)
{
    apply_cropping(hsv, outside_cropping_top, outside_cropping_bottom, outside_cropping_left, outside_cropping_right);

    const auto low = cv::Scalar(outside_thresh_min_hue, outside_thresh_min_sat, outside_thresh_min_val);
    const auto high = cv::Scalar(outside_thresh_max_hue, outside_thresh_max_sat, outside_thresh_max_val);
    cv::inRange(hsv, low, high, thresh);

    const auto solid_line = get_solid_line(thresh);
    if (0 == solid_line.size())
    {
        center_point.x = -1;
        center_point.y = -1;
        slope = NAN;
    }
    else
    {
        const auto solid_line_rect = cv::boundingRect(solid_line);

        // If there is less area than the min. expected, don't record as a detection
        if (solid_line_rect.area() < outside_min_detect_area)
        {
            center_point.x = -1;
            center_point.y = -1;
            slope = NAN;
        }


        center_point.x = solid_line_rect.x + (solid_line_rect.width >> 1);
        center_point.y = solid_line_rect.y + (solid_line_rect.height >> 1);

        cv::line(thresh, cv::Point2i(center_point.x, 0), cv::Point2i(center_point.x, thresh.rows), 0xff);

        slope = get_slope(solid_line);
    }


}


/// @brief Finds the red line and extracts parameters.
/// @param hsv The frame, in HSV, to extract data from.
/// @param thresh The threshold frame, Output param.
/// @param detected Whether or not the red line is "detected." Output param.
void stop_line_detection(cv::Mat& hsv, cv::Mat1b& thresh, bool& detected)
{
    apply_cropping(hsv, stop_cropping_top, stop_cropping_bottom, stop_cropping_left, stop_cropping_right);

    const auto low = cv::Scalar(stop_thresh_min_hue, stop_thresh_min_sat, stop_thresh_min_val);
    const auto high = cv::Scalar(stop_thresh_max_hue, stop_thresh_max_sat, stop_thresh_max_val);
    cv::inRange(hsv, low, high, thresh);

    const auto stop_line = get_stop_line(thresh);

    const auto stop_line_rect = cv::boundingRect(stop_line);
    detected = stop_line_rect.area() >= stop_min_detect_area;
}


/// @brief Parameters which are available to the LCD screen printing.
class PrintParams
{
    public:
    PrintParams(): frame(cv::Mat1b()), outside_line_slope(0), start_tick(0), outside_dist_from_ideal(0) {}

    /// @brief The image to print to the screen.
    cv::Mat1b frame;

    /// @brief The slope of the detected outside line.
    float outside_line_slope;

    /// @brief The number of ticks which have thus far passed.
    TickType_t start_tick;

    /// @brief The number of pixels the line on the screen is from its ideal, calibrated position.
    int outside_dist_from_ideal;
};

/// @brief Prints data about the detection process to the LCD screen.
/// @param screen The screen to print to.
/// @param params A struct containing values to print to the screen.
void output_to_screen(SSD1306_t& screen, PrintParams& params)
{
    cv::resize(params.frame, params.frame, cv::Size(lane_detect::SCREEN_WIDTH, lane_detect::SCREEN_HEIGHT));
    lane_detect::lcd_draw_matrix(screen, params.frame);

    // Calculate the FPS.
    const auto delta_ticks = xTaskGetTickCount() - params.start_tick;
    const auto framerate = static_cast<double>(configTICK_RATE_HZ) / delta_ticks; // How many seconds it took to process a frame.

    lane_detect::lcd_draw_data(screen, "FPS:", (int)framerate);
    lane_detect::lcd_draw_data(screen, "Dist:", params.outside_dist_from_ideal);
}


/// @brief The main driver loop.
inline void main_loop()
{
    camera_fb_t* fb = nullptr;

    // Init screen
    SSD1306_t screen; // The screen device struct.
    i2c_master_init(&screen, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&screen, lane_detect::SCREEN_WIDTH, lane_detect::SCREEN_HEIGHT);

    while (true)
    {
        // Crop the current frame so that it will fit on the screen.
        cv::Mat working_frame = lane_detect::get_frame(&fb);
        if (working_frame.size[0] == 0)
        {
            vTaskDelay(1);
            continue;
        }

        #if(CALIBRATION_MODE == 1)
        lane_detect::debug::send_matrix(working_frame);
        #endif

        const auto start_tick = xTaskGetTickCount();

        // Get into the right color space for thresholding.
        cv::Mat bgr;
        cv::cvtColor(working_frame, bgr, cv::COLOR_BGR5652BGR);
        cv::Mat hsv;
        cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV, 3);

        // Perform detection on the outsid line.
        cv::Mat hsv_outside = hsv.clone();
        cv::Mat1b outside_thresh;
        cv::Point2i outside_line_center;
        float outside_line_slope;
        outside_line_detection(hsv_outside, outside_thresh, outside_line_center, outside_line_slope);
        int outside_dist_from_ideal = outside_line_center.x - expected_line_pos;

        // Perform detection on the stop line.
        cv::Mat1b stop_thresh;
        bool detected;
        stop_line_detection(hsv, stop_thresh, detected);

        // Write to the screen.
        PrintParams params;
        params.start_tick = start_tick;
        params.frame = outside_thresh | stop_thresh;
        params.outside_dist_from_ideal = outside_dist_from_ideal;
        params.outside_line_slope = outside_line_slope;
        output_to_screen(screen, params);

        vTaskDelay(1);
    }
}


/// @brief The entry-point.
void app_main(void)
{
    lane_detect::config_cam();

    main_loop();
}
