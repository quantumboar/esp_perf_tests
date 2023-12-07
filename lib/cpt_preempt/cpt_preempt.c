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

#include "cpt_config.h"
#include "cpt_preempt.h"
#include "esp_check.h"

#define TAG "preempt"

// The stack size to be used for tasks.
// Anything below 1000 causes assertions in simple operations like logging.
// For practical purposes where logging/debugging is included this value should be larger than 2000
#define CPT_TASKS_STACK_SIZE (2048)

static void cpt_preempt_task_function(void * parameters);

esp_err_t cpt_preempt_wait_for_join(cpt_preempt * preempt, uint32_t max_wait_ms)
{
    TaskHandle_t current_task_handle = xTaskGetCurrentTaskHandle();
    TaskHandle_t null_task_handle = NULL;
    bool valid = atomic_compare_exchange_strong(&preempt->join_handle, &null_task_handle, current_task_handle);
    ESP_RETURN_ON_FALSE(valid, ESP_ERR_INVALID_STATE, TAG, "Handle already set");

    uint32_t notification_value = ulTaskNotifyTake(pdTRUE, max_wait_ms == CPT_PREEMPT_WAIT_FOREVER ? portMAX_DELAY : pdMS_TO_TICKS(max_wait_ms));
    atomic_compare_exchange_strong(&preempt->join_handle, &current_task_handle, NULL);

    return notification_value == 0 ? ESP_ERR_TIMEOUT : ESP_OK;
}

/// @brief signals that the job is completed. If a task was blocked via cpt_preempt_wait_for_join, it will be unblocked
/// @return ESP_OK in case of success, a different value in case of error
static inline esp_err_t cpt_preempt_signal_join(cpt_preempt * preempt)
{
    BaseType_t max_woken_thread_priority = 1;
    // Use the fromISR variant to avoid blocking the calling thread
    vTaskNotifyGiveFromISR(atomic_load(&preempt->join_handle), &max_woken_thread_priority);
    return ESP_OK;
}

esp_err_t cpt_preempt_init(cpt_preempt * preempt, cpt_job * job)
{
    * preempt = (cpt_preempt) {0};
    esp_err_t ret = ESP_OK;

    preempt->job = job;
    preempt->job_lock = xSemaphoreCreateCounting(1, 1);
    ESP_GOTO_ON_FALSE(preempt->job_lock != NULL, ESP_ERR_INVALID_STATE, exit, TAG, "Unable to create semaphore");

    BaseType_t task_create_ret = 0;

    for (uint8_t i = 0; i < CPT_TASK_COUNT; i ++)
    {
        char task_name[configMAX_TASK_NAME_LEN];
        if (snprintf(task_name, configMAX_TASK_NAME_LEN, "task_%u", i) < 0)
        {
            ESP_LOGE(TAG, "Task name is truncated");
        }

        task_create_ret = xTaskCreatePinnedToCore(cpt_preempt_task_function, task_name, CPT_TASKS_STACK_SIZE, (void *)preempt, tskIDLE_PRIORITY, &preempt->cpt_tasks[i].handle, 0);
        ESP_GOTO_ON_FALSE(task_create_ret == pdPASS, ESP_ERR_INVALID_STATE, exit, TAG, "Unable to create task");
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

    for (int i = 0; i < CPT_TASK_COUNT; i ++)
    {
        if (preempt->cpt_tasks[i].handle != NULL)
        {
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

    for (int8_t i = 0; i < CPT_TASK_COUNT; i ++)
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

    if (atomic_fetch_add(&preempt->join_counter, 1) == CPT_TASK_COUNT - 1)
    {
        cpt_preempt_signal_join(preempt);
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
        ESP_LOGD(TAG, "task: %u counter: %llu", task_index, preempt->job->counter);
        xSemaphoreGive(preempt->job_lock);

        // Doesn't need to be in the critical section as it's accessed by this task only
        preempt->cpt_tasks[task_index].counter ++;
    }

    // signal that we're done
    cpt_preempt_signal_join(preempt);
}

esp_err_t cpt_preempt_run_job(cpt_preempt * preempt)
{
    ESP_LOGI(TAG, "Starting job");

    // Here we wait until all tasks are initialized (the last task that's initialized will signal then suspend itself)
    cpt_preempt_wait_for_join(preempt, CPT_PREEMPT_WAIT_FOREVER);

    bool do_spin = true;

    // This should spin for a short time only, to avoid a race with the last task suspending itself
    while (do_spin)
    {
        do_spin = false;

        for (int i = 0; i < CPT_TASK_COUNT; i ++)
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

    ESP_LOGI(TAG, "all tasks suspended, proceeding");

    // Now resume all tasks. Time measurement should begin here
    for (uint8_t i = 0; i < CPT_TASK_COUNT; i ++)
    {
        vTaskResume(preempt->cpt_tasks[i].handle);
    }

    return ESP_OK;
}