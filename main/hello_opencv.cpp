#undef EPS
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#define EPS 192

#include <esp_log.h>
#include <string>
#include "sdkconfig.h"
#include <iostream>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_err.h>
#include <esp_spiffs.h>

#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



using namespace cv;
using namespace std;

// Special GPIO pins for the AI-THINKER board.
// Presumably, these work for the ESP32-CAM-MB
// daughter board as well.
#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

typedef unsigned char byte_t;

static char TAG[]="hello_opencv";

static cv::Mat curr_frame;
static bool new_frame = false;

extern "C" {
void app_main(void);
}

/// @brief Configures the ESP-32-CAM.
void config_cam()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565; 
    config.frame_size = FRAMESIZE_96X96;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed");
        // Handle error
    }

}

/// @brief Gets a frame from the camera
/// @return The camera frame, as an OpenCV matrix.
void get_frame(Mat& frame)
{
    // Take a picture from the camera.
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        return;
    }
    
    // Build the OpenCV matrix.
    uint16_t* pixels_raw = static_cast<uint16_t*>(static_cast<void*>(fb->buf));
    for (uint8_t row = 0; row < frame.rows; row++)
    {
        for (uint8_t col = 0; col < frame.cols; col++)
        {
            uint16_t pixel = *pixels_raw;
            pixels_raw++;

            frame.at<uint8_t>(row, col, 0) = (uint8_t)(pixel & 0x1f); // B
            frame.at<uint8_t>(row, col, 1) = (uint8_t)((pixel >> 5) & 0x3f); // G
            frame.at<uint8_t>(row, col, 2) = (uint8_t)((pixel >> 11) & 0x1f); // R
        }
    }

    // Cleanup and return.
    esp_camera_fb_return(fb);
    return;
}

void send_frame(const Mat& frame)
{
    // Begin transmission
    printf("S"); // Start of transmission

    // Transmit the number of rows and columns
    printf("%04x", curr_frame.rows);
    printf("%04x", curr_frame.cols);

    // Transmit the data of the frame.
    for (int row = 0; row < curr_frame.rows; row++) 
    {
        for (int col = 0; col < curr_frame.cols; col++)
        {
            for (int channel = 0; channel < curr_frame.channels(); channel++)
            {
                printf("%02x", curr_frame.at<uint8_t>(row, col, channel));
            }
        }
    }

    // End transmission and flush
    printf("E\n");
    fflush(stdout);
}

/// @brief  Sends the last frame captured over the serial port.
void task_send_frame(void* arg)
{
    // How long between sends. Currently experimental; I'm not sure how many ticks
    // a "send" will take.
    const TickType_t SEND_PERIOD = 30;

    // The amount of time this thread should wait if no new frame was found.
    const TickType_t WAIT_PERIOD = 10;

    // This task should loop forever, constantly updating the frame.
    while (true)
    {
        // Wait until there's a new frame to send
        if (!new_frame)
        {
            vTaskDelay(WAIT_PERIOD);
            continue;
        }

        auto start_time = xTaskGetTickCount();

        // Begin transmission
        printf("S"); // Start of transmission

        // Transmit the number of rows and columns
        printf("%04x", curr_frame.rows);
        printf("%04x", curr_frame.cols);

        // Transmit the data of the frame.
        for (int row = 0; row < curr_frame.rows; row++) 
        {
            for (int col = 0; col < curr_frame.cols; col++)
            {
                for (int channel = 0; channel < curr_frame.channels(); channel++)
                {
                    printf("%02x", curr_frame.at<uint8_t>(row, col, channel));
                }
            }
        }

        // End transmission and flush
        printf("E\n");
        fflush(stdout);
        new_frame = false;

        // Enforce the constant period.
        auto elapsed_ticks = xTaskGetTickCount() - start_time; // Around 310 on avg

        vTaskDelay(SEND_PERIOD /* - elapsed_ticks */);
    }
}

/// @brief An unending task to get frames from the camera as
/// OpenCV matrices.
void task_get_frames(void* arg)
{
    const TickType_t TASK_PERIOD = 30;

    // The amount of time this thread should wait if no new frame was found.
    const TickType_t WAIT_PERIOD = 10;

    config_cam();

    Mat frame(96, 96, CV_8UC3, cv::Scalar(0, 0, 0));

    while (true)
    {
        /*
        // Don't get a new frame until 
        if (new_frame)
        {
            vTaskDelay(WAIT_PERIOD);
            continue;
        }
        */

        auto start_tick = xTaskGetTickCount();
        get_frame(frame);
        new_frame = true;
        send_frame(curr_frame);

        auto elapsed_ticks = xTaskGetTickCount() - start_tick;
        vTaskDelay(TASK_PERIOD /*- elapsed_ticks */);
    }
}

void app_main(void)
{
    xTaskCreate(task_get_frames, "get_frames", 4096, nullptr, 0, nullptr);
    //xTaskCreate(task_send_frame, "send_frame", 4096, nullptr, 1, nullptr);

    while (true)
    {
        vTaskDelay(100);
    }

    esp_camera_deinit();

}
