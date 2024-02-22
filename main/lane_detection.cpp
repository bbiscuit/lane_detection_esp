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
void canny_and_disp(void* arg)
{
    const TickType_t TASK_PERIOD = 30;
    const TickType_t WAIT_PERIOD = 10;

    // Extract params
    lane_detect::TaskParameters* args = static_cast<lane_detect::TaskParameters*>(arg);
    ThreadSafeQueue<cv::Mat>* in_q = args->in;
    ThreadSafeQueue<cv::Mat>* out_q = args->out;
    const uint8_t max_out_size = args->max_out_size;

    cv::Mat working_frame;

    // Init screen
    constexpr uint8_t SCREEN_WIDTH = 128;
    constexpr uint8_t SCREEN_HEIGHT = 64;

    SSD1306_t screen; // The screen device struct.
    i2c_master_init(&screen, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&screen, SCREEN_WIDTH, SCREEN_HEIGHT);

    while (true)
    {
        if (0 == in_q->size())
        {
            vTaskDelay(WAIT_PERIOD);
            continue;
        }

        // Crop the current frame so that it will fit on the screen.
        in_q->top(working_frame);
        working_frame = working_frame(cv::Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT));

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

        ESP_LOGI(TAG, "Wrote to the screen");
        vTaskDelay(1);

        if (max_out_size > out_q->size())
        {
            out_q->push(working_frame);
        }
    }
}


/// @brief Runs thresholding on the frame using predetermined values generated from the debugging
/// tool.
/// @param in_queue The Queue from which the frames come. 
void threshold_and_disp(ThreadSafeQueue<cv::Mat>& in_queue)
{
    constexpr uint8_t SCREEN_WIDTH = 128; // The width, in pixels, of the LCD screen.
    constexpr uint8_t SCREEN_HEIGHT = 64; // The height, in pixels, of the LCD screen.
    cv::Mat working_frame; // The mat into which the frame will be loaded from the camera.

    // Initialize the LCD.
    SSD1306_t screen; // The screen device struct.
    i2c_master_init(&screen, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
    ssd1306_init(&screen, SCREEN_WIDTH, SCREEN_HEIGHT);


    while (true)
    {
        // If the queue is empty, wait a little bit and try again.
        if (0 == in_queue.size())
        {
            constexpr TickType_t WAIT_PERIOD = 1;
            vTaskDelay(WAIT_PERIOD); // So that we're not stuck here.
            continue;
        }

        in_queue.top(working_frame);

        // Do the thresholding

        // Write to the screen.
        write_bin_mat(screen, working_frame);
        vTaskDelay(1); // So that we don't hang.
    }
}


/// @brief The entry-point.
void app_main(void)
{
    lane_detect::config_cam();

    // Setup the IT-Queues.
    ThreadSafeQueue<cv::Mat> raw_frame_queue;
    ThreadSafeQueue<cv::Mat> canny_queue;

    // Setup the task which gets frames from the camera.
    lane_detect::TaskParameters get_frames_params = {nullptr, &raw_frame_queue, 1};
    xTaskCreate(lane_detect::camera_task, "get_img_matrix", 4096, &get_frames_params, 1, nullptr);

    // Start the canny thread on the main processor.
    lane_detect::TaskParameters task_canny_and_disp_params = {&raw_frame_queue, &canny_queue, 1};
    canny_and_disp(&task_canny_and_disp_params);

}
