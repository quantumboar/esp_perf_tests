/*
Copyright (c) 2023 Quantumboar <quantum@quantumboar.net>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __CPT_PREEMPT_H__
#define __CPT_PREEMPT_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "stdatomic.h"

#include "cpt_config.h"
#include "cpt_job.h"

#define CPT_PREEMPT_WAIT_FOREVER (0)

typedef struct
{
    TaskHandle_t handle;
    unsigned long counter;  // Counts how many times this task had a chance to run
} cpt_preempt_task;

typedef struct
{
    cpt_preempt_task cpt_tasks[CPT_TASK_COUNT];
    cpt_job * job;

    atomic_uint_fast8_t join_counter; // Used to determine join operations
    // FreeRTOS notifications are more efficient than semaphores, but we use the latter for convenience
    SemaphoreHandle_t job_lock;  // Protects access to the shared resource (the job)
    SemaphoreHandle_t join_lock; // Synchronization to block the main thread until the job is completed

    _Atomic TaskHandle_t join_handle; // Handle waiting to join
} cpt_preempt;

// Initializes all structures and tasks necessary to run the test. Tasks are suspended at creation and
// will be resumed when calling the start function.
esp_err_t cpt_preempt_init(cpt_preempt * preempt, cpt_job * job);
void cpt_preempt_uninit(cpt_preempt * test);

// Starts the execution of the job scheduled for this preempt object
// This call is not blocking
esp_err_t cpt_preempt_run_job(cpt_preempt * preempt);

/// @brief Blocks the caller task until the current job is terminated or timeout
/// @param max_wait_us the maximum wait time in ms, CPT_PREEMPT_WAIT_FOREVER to never timeout
/// @return ESP_OK in case of success, ESP_ERROR_TIMEOUT if the maximum time was reached or a different value in case of error
esp_err_t cpt_preempt_wait_for_join(cpt_preempt * preempt, uint32_t max_wait_ms);

#endif //__CPT_PREEMPT_H__