#ifndef THREAD_SAFE_QUEUE_H
#define THREAD_SAFE_QUEUE_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <queue>

template <typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue(): mutex_(xSemaphoreCreateMutex()) {

    }

    void push(const T& value) {
        //std::lock_guard<std::mutex> lock(mutex_);
        xSemaphoreTake(mutex_, portMAX_DELAY);
        queue_.push(value);
        xSemaphoreGive(mutex_);
    }

    bool pop() {
        xSemaphoreTake(mutex_, portMAX_DELAY);

        if (!queue_.empty()) {
            queue_.pop();

            xSemaphoreGive(mutex_);
            return true;
        }

        xSemaphoreGive(mutex_);
        return false;
    }

    bool top(T& value) {
        xSemaphoreTake(mutex_, portMAX_DELAY);

        if (!queue_.empty()) {
            value = queue_.front();
            
            xSemaphoreGive(mutex_);
            return true;
        }

        xSemaphoreGive(mutex_);
        return false;
    }

    size_t size() {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        size_t s = queue_.size();
        xSemaphoreGive(mutex_);
        return s;
    }

private:
    std::queue<T> queue_;
    SemaphoreHandle_t mutex_;
};

#endif
