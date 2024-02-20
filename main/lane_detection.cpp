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


static char TAG[]="lane_detection";


// This is necessary because it allows ESP-IDF to find the main function,
// even though C++ mangles the function name.
extern "C" {
void app_main(void);
}


/// @brief Sends a frame over the serial port for the purposes of testing.
/// @param frame The OpenCV matrix to send.
void send_frame(const cv::Mat& frame)
{
    // Begin transmission
    printf("START"); // Start of transmission

    // Transmit the number of rows and columns
    const int channels = frame.channels();

    printf("%04x", frame.rows);
    printf("%04x", frame.cols);
    printf("%04x", channels);

    // Transmit the data of the frame.
    for (int row = 0; row < frame.rows; row++) 
    {
        for (int col = 0; col < frame.cols; col++)
        {
            for (int channel = 0; channel < channels; channel++)
            {
                printf("%02x", frame.at<uint8_t>(row, col, channel));
            }
        }
    }

    // End transmission and flush
    printf("E\n");
    fflush(stdout);
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
            if (0 != bin_mat.at<uint8_t>(row, col))
            {
                _ssd1306_pixel(&screen, col, row, false);
            }
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

        // Get the canny of the current frame.
        in_q->top(working_frame);
        cv::cvtColor(working_frame, working_frame, cv::COLOR_BGR5652GRAY);

        cv::Mat screen_frame(SCREEN_HEIGHT, SCREEN_WIDTH, CV_8U);
        cv::resize(working_frame, screen_frame, screen_frame.size(), 0, 0, cv::INTER_CUBIC);
        cv::blur(screen_frame, screen_frame, cv::Size(3, 3));
        int lowThresh = 80;
        int kernSize = 3;
        cv::Canny(screen_frame, screen_frame, lowThresh, 4 * lowThresh, kernSize);

        // Write it to the display.
        ssd1306_clear_screen(&screen, false);
        write_bin_mat(screen, screen_frame);

        ESP_LOGI(TAG, "Wrote to the screen");
	    vTaskDelay(3000 / portTICK_PERIOD_MS);

        if (max_out_size > out_q->size())
        {
            out_q->push(working_frame);
        }

        vTaskDelay(TASK_PERIOD);
    }
}


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
