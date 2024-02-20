///////////////////////////////////////////////////////////////////////////////////////////////////
/// Stores common code used by the whole project.
///
/// Author: Andrew Huffman
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once


#include <stdint.h>
#include "opencv2/core.hpp"
#include "thread_safe_queue.h"


namespace lane_detect
{
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
}