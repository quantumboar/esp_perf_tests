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

#include "cpt_preempt.h"
#include "esp_check.h"

#define TAG "preempt"

static void cpt_preempt_task_function(void * parameters);

// This implementation runs the specified job (of type cpt_job_t) concurrently, using threads and locks

esp_err_t cpt_preempt_init(cpt_preempt * preempt, cpt_job * job)
{
    * preempt = (cpt_preempt) {0};
    esp_err_t ret = ESP_OK;

    preempt->job_lock = xSemaphoreCreateMutex();
    preempt->join_lock = xSemaphoreCreateCounting(CPT_TASK_COUNT, 0);
    preempt->job = job;

    ESP_GOTO_ON_FALSE(preempt->job_lock != NULL && preempt->join_lock != NULL, ESP_ERR_INVALID_STATE, exit, TAG, "Unable to create semaphore");

    BaseType_t task_create_ret = 0;

    for (uint8_t i = 0; i < CPT_TASK_COUNT; i ++)
    {
        char task_name[configMAX_TASK_NAME_LEN];
        if (snprintf(task_name, configMAX_TASK_NAME_LEN, "task_%2u", i) < 0)
        {
            ESP_LOGE(TAG, "Task name is truncated");
        }

        task_create_ret = xTaskCreatePinnedToCore(cpt_preempt_task_function, task_name, 100, (void *)preempt, tskIDLE_PRIORITY, &preempt->cpt_tasks[i].handle, 0);
        ESP_GOTO_ON_FALSE(task_create_ret != pdPASS, ESP_ERR_INVALID_STATE, exit, TAG, "Unable to create task");
    }

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

    if (preempt->join_lock)
    {
        vSemaphoreDelete(preempt->join_lock);
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

    // Take the join semaphore until done
    xSemaphoreTake(preempt->join_lock, portMAX_DELAY);

    // Suspend the current task. It will be resumed by a call to start()
    vTaskSuspend(NULL);

    while (! done)
    {
        xSemaphoreTake(preempt->job_lock, portMAX_DELAY);
        done = cpt_job_run(preempt->job) == CPT_JOB_DONE;
        xSemaphoreGive(preempt->job_lock);

        // Doesn't need to be in the critical section as it's accessed by this task only
        preempt->cpt_tasks[task_index].counter ++;
    }

    xSemaphoreGive(preempt->join_lock);
}

esp_err_t cpt_preempt_run_job(cpt_preempt * preempt)
{
    for (uint8_t i = 0; i < CPT_TASK_COUNT; i ++)
    {
        ESP_RETURN_ON_FALSE(preempt->cpt_tasks[i].handle != NULL, ESP_ERR_INVALID_STATE, TAG, "Invalid thread handler");
        vTaskResume(preempt->cpt_tasks[i].handle);
    }

    return ESP_OK;
}

esp_err_t cpt_preempt_wait_for_time(cpt_preempt * preempt, uint16_t wait_ms)
{

    if (xSemaphoreTake(preempt->join_lock, portTICK_PERIOD_MS * wait_ms) == pdFALSE)
    {
        return ESP_ERR_TIMEOUT;
    }

    xSemaphoreGive(preempt->join_lock);

    return ESP_OK;
}