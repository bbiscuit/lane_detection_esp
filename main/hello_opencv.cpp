// Opencv Imports
#undef EPS
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#define EPS 192


// Stdlib imports imports
#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>


// Esp imports
#include <esp_err.h>
#include <esp_spiffs.h>
#include <esp_log.h>
#include "sdkconfig.h"
#include "esp_camera.h"


// FreeRTOS imports
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"


// In-project imports
#include "thread_safe_queue.h"


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


// The parameters which are sent to a task.
struct TaskParameters
{
    // The queue from which the task should get frames.
    ThreadSafeQueue<cv::Mat>* in;

    // The queue to which the task should output frames.
    ThreadSafeQueue<cv::Mat>* out;

    // The maximum number of frames which are allowed to
    // be written into the output.
    uint8_t max_out_size;
};


// This is necessary because it allows ESP-IDF to find the main function,
// even though C++ mangles the function name.
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


/// @brief An unending task to get frames from the camera as
/// OpenCV matrices.
void task_img_usb(void* arg)
{
    const TickType_t TASK_PERIOD = 30;
    const TickType_t WAIT_PERIOD = 10;

    // Break-out params.
    TaskParameters* args = static_cast<TaskParameters*>(arg);
    ThreadSafeQueue<cv::Mat>* in_q = args->in;

    cv::Mat frame;

    while (true)
    {
        // If there are no frames to send, don't send.
        if (0 == in_q->size())
        {
            vTaskDelay(WAIT_PERIOD);
            continue;
        }

        auto start_tick = xTaskGetTickCount();
        in_q->top(frame);
        //Mat frame = get_frame();
        send_frame(frame);

        //auto elapsed_ticks = xTaskGetTickCount() - start_tick;
        vTaskDelay(TASK_PERIOD /*- elapsed_ticks */);
    }
}


void task_get_img_matrix(void* arg)
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

        auto start_tick = xTaskGetTickCount();
        auto frame = get_frame();
        out_q->push(frame);
        auto end_tick = xTaskGetTickCount();

        

        vTaskDelay(TASK_PERIOD);
    }
}


/// @brief Runs Canny edge detection on a frame from the input Queue, displays it on
/// the connected screen, and also pushes it onto an output Queue for debugging purposes.
/// @param arg The input/output queues.
void task_canny_and_disp(void* arg)
{
    const TickType_t TASK_PERIOD = 30;
    const TickType_t WAIT_PERIOD = 10;

    // Extract params
    TaskParameters* args = static_cast<TaskParameters*>(arg);
    ThreadSafeQueue<cv::Mat>* in_q = args->in;
    ThreadSafeQueue<cv::Mat>* out_q = args->out;
    const uint8_t max_out_size = args->max_out_size;

    cv::Mat working_frame;

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
        cv::Canny(working_frame, working_frame, 50, 150);


        if (max_out_size > out_q->size())
        {
            out_q->push(working_frame);
        }

        vTaskDelay(TASK_PERIOD);
    }
}


void app_main(void)
{
    config_cam();

    // Setup the task which gets frames from the camera.
    ThreadSafeQueue<cv::Mat> raw_frame_queue;
    TaskParameters get_frames_params = {nullptr, &raw_frame_queue, 1};
    xTaskCreate(task_get_img_matrix, "get_img_matrix", 4096, &get_frames_params, 0, nullptr);

    // Setup the canny thread.
    ThreadSafeQueue<cv::Mat> canny_queue;
    TaskParameters task_canny_and_disp_params = {&raw_frame_queue, &canny_queue, 1};
    xTaskCreate(task_canny_and_disp, "canny_and_disp", 4096, &task_canny_and_disp_params, 0, nullptr);

    // Setup the task which sends frames through the serial port.
    TaskParameters send_serial = {&canny_queue, nullptr, 0};
    xTaskCreate(task_img_usb, "img_usb", 4096, &send_serial, 0, nullptr);

    while (true)
    {
        vTaskDelay(100);
    }

    esp_camera_deinit();

}
