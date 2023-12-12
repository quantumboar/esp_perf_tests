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

// 1 is the same priority as main. It allows for full CPU utilization
#define CPT_PREEMPT_TASK_PRIO (1)

// Use this in cpt_preempt_wait_for_state_change to deisable timeout
#define CPT_PREEMPT_WAIT_FOREVER (0)

// Distribute tasks across cores (evenly split)
#define CPT_PREEMPT_ENABLE_MULTI_CORE (1)

/// @brief Structure handling a task in the preemptive test
typedef struct
{
    TaskHandle_t handle; // Handle for the task
    unsigned long counter;  // Counts how many times this task had a chance to run a job
} cpt_preempt_task;

/// @brief  Preeventive test state, useful for state signal handling
typedef enum
{
    CPT_PREEMPT_STATE_NONE = 0,
    CPT_PREEMPT_STATE_INITIALIZING,
    CPT_PREEMPT_STATE_INITIALIZED,
    CPT_PREEMPT_STATE_RUNNING,
    CPT_PREEMPT_STATE_DONE,
    CPT_PREEMPT_STATE_COUNT
} cpt_preempt_state;

/// @brief Structure holding state for a preemoption test
typedef struct
{
    cpt_preempt_task cpt_tasks[CPT_TASK_COUNT];
    atomic_uint_fast8_t initialized_tasks_count; // Used to determine when all tasks are initialized

    cpt_job * job;

    SemaphoreHandle_t job_lock;  // Protects access to the shared resource (the job)

    volatile _Atomic cpt_preempt_state state; // The state of this preempt object
    // An event is generated at each significant state change. Currently when the preempt object threads all are initialized, and when the job is completed.
    volatile _Atomic TaskHandle_t waiting_task_handle; // Handle for a task waiting for the next event
} cpt_preempt;

// Initializes all structures and tasks necessary to run the test. Tasks are suspended at creation and
// will be resumed when calling the start function.
esp_err_t cpt_preempt_init(cpt_preempt * preempt, cpt_job * job);
void cpt_preempt_uninit(cpt_preempt * test);

// Starts the execution of the job scheduled for this preempt object
// This call is not blocking
esp_err_t cpt_preempt_run_job(cpt_preempt * preempt);

/// @brief Block caller thread until the next state change.
/// @details cpt_preempt objects signal an event at each relevant state change. A task (and just one) can use this method to block until the next state change
///        by using this function. It handles the race between setting the state and calling this method by virtue of a short spin on
///        atomic compare and swap
/// @param max_wait_us the maximum wait time in ms, CPT_PREEMPT_WAIT_FOREVER to never timeout
/// @param state the state to wait for
/// @return ESP_OK in case of success, ESP_ERROR_TIMEOUT if the maximum time was reached.
esp_err_t cpt_preempt_wait_for_state_change(cpt_preempt * preempt, uint32_t max_wait_ms, cpt_preempt_state state);

/// @brief Provides an indication of the job progress
/// @return the job progress as an absolute number
uint64_t cpt_preempt_get_job_counter(cpt_preempt * preempt);

#endif //__CPT_PREEMPT_H__