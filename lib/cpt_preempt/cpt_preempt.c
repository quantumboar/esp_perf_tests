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

#include "cpt_globals.h"
#include "cpt_preempt.h"
#include "esp_check.h"

#define TAG "preempt"

// The stack size to be used for tasks.
// Anything below 1000 causes assertions in simple operations like logging.
// For practical purposes where logging/debugging is included this value should be larger than 2000
#define CPT_TASKS_STACK_SIZE (2048)

static void cpt_preempt_task_function(void * parameters);

/// @brief change the state for this object and notify waiting task (if set)
/// @details If set, the task pending for state changes will be unblocked
/// @param new_state the state to set this cpt_preempt object to
/// @return esp_ok in case of success
static esp_err_t cpt_preempt_set_state(cpt_preempt *preempt, cpt_state new_state)
{
    ESP_LOGD(TAG, "Changing state from %d to %d", atomic_load(&preempt->state), new_state);
    // Set the state first
    atomic_store(&preempt->state, new_state);

    // Then read the handle
    volatile TaskHandle_t waiting_task_handle = atomic_load(&preempt->waiting_task_handle);

    // Notify task if necessary
    if (waiting_task_handle != NULL)
    {
        xTaskNotifyGive(waiting_task_handle);
    }

    return ESP_OK;
}

esp_err_t cpt_preempt_wait_for_state_change(cpt_preempt * preempt, uint32_t max_wait_ms, cpt_state expected_state)
{
    TaskHandle_t this_task_handle = xTaskGetCurrentTaskHandle();
    TaskHandle_t null_task_handle = NULL;
    volatile cpt_state current_state = CPT_STATE_NONE;
    uint32_t notification_value = 0;

    // Set the wait handle first
    bool valid = atomic_compare_exchange_strong(&preempt->waiting_task_handle, &null_task_handle, this_task_handle);
    ESP_RETURN_ON_FALSE(valid, ESP_ERR_INVALID_STATE, TAG, "Handle already set");

    while (current_state != expected_state)
    {
        // Read the state after setting the handle: this fixes races
        current_state = atomic_load(&preempt->state);

        if (current_state != expected_state)
        {
            // Block here
            notification_value = ulTaskNotifyTake(pdTRUE, max_wait_ms == CPT_WAIT_FOREVER ? portMAX_DELAY : pdMS_TO_TICKS(max_wait_ms));
            if (notification_value == 0)
            {
                break;
            }
        }
    }

    atomic_store(&preempt->waiting_task_handle, NULL);

    return notification_value == 0 ? ESP_ERR_TIMEOUT : ESP_OK;
}

esp_err_t cpt_preempt_init(cpt_preempt * preempt, cpt_job * job)
{
    * preempt = (cpt_preempt) {0};
    esp_err_t ret = ESP_OK;

    preempt->job = job;
    preempt->job_lock = xSemaphoreCreateCounting(1, 1);
    ESP_GOTO_ON_FALSE(preempt->job_lock != NULL, ESP_ERR_INVALID_STATE, exit, TAG, "Unable to create semaphore");

    BaseType_t task_create_ret = 0;
    ret = cpt_preempt_set_state(preempt, CPT_STATE_INITIALIZING);
    ESP_GOTO_ON_ERROR(ret, exit, TAG, "Error setting state: %s", esp_err_to_name(ret));

    for (uint8_t task_index = 0; task_index < CPT_CONCURRENCY_COUNT; task_index ++)
    {
        char task_name[configMAX_TASK_NAME_LEN];
        if (snprintf(task_name, configMAX_TASK_NAME_LEN, "task_%"PRIu8, task_index) < 0)
        {
            ESP_LOGE(TAG, "Task name is truncated");
        }

        task_create_ret = xTaskCreatePinnedToCore(
            cpt_preempt_task_function,                    // task function
            task_name,                                    // task name
            CPT_TASKS_STACK_SIZE,                         // stack size
            (void *)preempt,                              // context passed to task function
            CPT_PREEMPT_TASK_PRIO,                        // task priority
            &preempt->cpt_tasks[task_index].handle,       // task handle (output parameter)
            CPT_PREEMPT_ENABLE_MULTI_CORE ? task_index % 2 : 0); // core
    
        ESP_GOTO_ON_FALSE(task_create_ret == pdPASS, ESP_ERR_INVALID_STATE, exit, TAG, "Unable to create task index %d", task_index);
    }

    ESP_LOGI(TAG, "Tasks initialized");

    exit:
    if (ret != ESP_OK)
    {
        cpt_preempt_uninit(preempt);
    }

    return ret;
}

void cpt_preempt_uninit(cpt_preempt * preempt)
{
    ESP_LOGI(TAG, "uninitializing");

    for (int i = 0; i < CPT_CONCURRENCY_COUNT; i ++)
    {
        if (preempt->cpt_tasks[i].handle != NULL)
        {
            ESP_LOGD(TAG, "Deleting task %d", i);
            vTaskDelete(preempt->cpt_tasks[i].handle);
        }
    }

    if (preempt->job_lock)
    {
        vSemaphoreDelete(preempt->job_lock);
    }

    * preempt = (cpt_preempt) {0};
}

// Returns -1 if the task wasn't found
static int8_t cpt_preempt_get_current_task_index(cpt_preempt * preempt)
{
    TaskStatus_t task_status = {0};
    vTaskGetInfo(
        NULL,
        &task_status,
        pdFALSE,    //don't query for stack info (faster)
        eNoAction); //don't query for task state (faster)

    for (int8_t i = 0; i < CPT_CONCURRENCY_COUNT; i ++)
    {
        if (preempt->cpt_tasks[i].handle == task_status.xHandle)
        {
            return i;
        }
    }

    return -1;
}

// Initialization times are removed from the perf measurement, so we'll have all tasks
// enter a suspended state right after terminating their initialization. The job_run method
// will wait for the tasks to be suspended before starting the preemptive run test.
static void cpt_preempt_task_function(void * parameters)
{
    bool done = false;
    cpt_preempt * preempt = (cpt_preempt *) parameters;
    int8_t task_index = cpt_preempt_get_current_task_index(preempt);

    if (task_index == -1)
    {
        ESP_LOGE(TAG, "Task not found in array, terminating task");
        return;
    }

    if (atomic_fetch_add(&preempt->initialized_tasks_count, 1) == CPT_CONCURRENCY_COUNT - 1)
    {
        cpt_preempt_set_state(preempt, CPT_STATE_INITIALIZED);
    }

    // Suspend the current task. It will be resumed by a call to start()
    // There is a non-zero time interval between the fetch-add above and when the task is suspended, so 
    // the obj_run method will have to poll for the tread state and spin for a while
    ESP_LOGD(TAG, "suspending task %d, waiting for start", task_index);
    vTaskSuspend(NULL);
    ESP_LOGD(TAG, "task %d resumed", task_index);

    while (! done)
    {
        xSemaphoreTake(preempt->job_lock, portMAX_DELAY);
        done = cpt_job_run(preempt->job) == CPT_JOB_DONE;
        xSemaphoreGive(preempt->job_lock);

        // Doesn't need to be in the critical section as it's accessed by this task only
        preempt->cpt_tasks[task_index].counter ++;

        // Job done, relinquish any remaining CPU to allow other threads to run
        taskYIELD();
    }

    // signal that we're done
    cpt_preempt_set_state(preempt, CPT_STATE_DONE);

    // FreeRTOS tasks can't return, wait for deletion here
    while (true)
    {
        taskYIELD();
    }
}

esp_err_t cpt_preempt_run_job(cpt_preempt * preempt)
{
    ESP_LOGI(TAG, "Starting job");

    // Here we wait until all tasks are initialized (the last task that's initialized will signal then suspend itself)
    cpt_preempt_wait_for_state_change(preempt, CPT_WAIT_FOREVER, CPT_STATE_INITIALIZED);

    bool do_spin = true;

    // This should spin for a short time only, to avoid a race with the last task suspending itself
    while (do_spin)
    {
        do_spin = false;

        for (int i = 0; i < CPT_CONCURRENCY_COUNT; i ++)
        {
            TaskStatus_t task_status;
            vTaskGetInfo(preempt->cpt_tasks[i].handle, &task_status, pdFALSE, eInvalid);
            if (task_status.eCurrentState != eSuspended) {
                do_spin = true;
                break;
            }
        }

        if (do_spin)
        {
            taskYIELD();
        }
    }

    cpt_preempt_set_state(preempt, CPT_STATE_RUNNING);

    // Now resume all tasks. Time measurement should begin here
    for (uint8_t i = 0; i < CPT_CONCURRENCY_COUNT; i ++)
    {
        vTaskResume(preempt->cpt_tasks[i].handle);
    }

    return ESP_OK;
}