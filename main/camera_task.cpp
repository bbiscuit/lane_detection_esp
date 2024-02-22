#include "camera_task.h"

#include <stdint.h>


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#undef EPS
#include "opencv2/core.hpp"
#define EPS 192

#include "esp_camera.h"
#include "esp_log.h"


#include "thread_safe_queue.h"
#include "common.h"


const char TAG[] = "camera_task";

// Special GPIO pins for the AI-THINKER board.
// Presumably, these work for the ESP32-CAM-MB
// daughter board as well.
// I think these ought to be taken out at some point; souldn't it be the menuconfig which sets
// these ports?
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


namespace lane_detect
{
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
        config.frame_size = FRAMESIZE_240X240;
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
    cv::Mat get_frame()
    {
        // Take a picture from the camera.
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            return cv::Mat();
        }
        
        // Build the OpenCV matrix.
        // CV_8UC2 is two-channel color, with 8-bit 
        cv::Mat result(fb->height, fb->width, CV_8UC2, fb->buf);

        // Cleanup and return.
        esp_camera_fb_return(fb);
        return result;
    }

    cv::Mat get_frame(camera_fb_t* fb)
    {
        // If a previous picture has been taken, give the frame-buffer back.
        if (fb != nullptr)
        {
            esp_camera_fb_return(fb);
        }

        // Take the picture.
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            return cv::Mat();
        }

        // Build the OpenCV matrix.
        // CV_8UC2 is two-channel color, with 8-bit channels.
        return cv::Mat(fb->height, fb->width, CV_8UC2, fb->buf);
    }

    void camera_task(void* arg)
    {
        const TickType_t TASK_PERIOD = 30;
        const TickType_t WAIT_PERIOD = 10;

        // Extract params
        TaskParameters* args = static_cast<TaskParameters*>(arg);
        ThreadSafeQueue<cv::Mat>* out_q = args->out;
        const uint8_t max_frames = args->max_out_size;

        while (true)
        {
            if (max_frames <= out_q->size())
            {
                vTaskDelay(WAIT_PERIOD);
                out_q->pop();
                continue;
            }

            auto frame = get_frame();
            out_q->push(frame);

            vTaskDelay(TASK_PERIOD);
        }
    }
}