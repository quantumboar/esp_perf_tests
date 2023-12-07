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

/*Contention Perf Test (cpt for short)*/
#include "cpt_preempt.h"
#include "cpt_coop.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"
#include "esp_check.h"
#include <sys/time.h>

#define TAG "cpt"

void log_memory()
{
    multi_heap_info_t heap_info = (multi_heap_info_t) {0};
    heap_caps_get_info(&heap_info, 0); // 0 matches all heaps, stats will be totalled across them

    ESP_LOGD(TAG, "Memory stats, current free: %d minimum free: %d current allocated: %d",
        heap_info.total_free_bytes,
        heap_info.minimum_free_bytes,
        heap_info.total_allocated_bytes);
}

// Method to log tasks in the system and their relative CPU usage
esp_err_t log_tasks()
{
    TaskStatus_t * task_statuses = NULL;

    volatile UBaseType_t task_count;
    esp_err_t ret = ESP_OK;

    task_count = uxTaskGetNumberOfTasks();
    ESP_LOGD(TAG, "Tasks stats, %d tasks in system", task_count);
    ESP_GOTO_ON_FALSE(task_count > 0, ESP_ERR_INVALID_STATE, exit, TAG, "No tasks in system");

    task_statuses = pvPortMalloc(task_count * sizeof(TaskStatus_t));
    ESP_GOTO_ON_FALSE(task_statuses != NULL, ESP_ERR_NO_MEM, exit, TAG, "Unable to allocate task info structures");

    uint32_t total_run_time = 0;
    float percentage_run_time;
    float total_run_time_float;

    task_count = uxTaskGetSystemState(task_statuses, task_count, &total_run_time);
    ESP_GOTO_ON_FALSE(task_count > 0, ESP_ERR_INVALID_STATE, exit, TAG, "Unable to get tasks");
    ESP_GOTO_ON_FALSE(total_run_time > 0, ESP_ERR_INVALID_STATE, exit, TAG, "Unable to get runtime");

    total_run_time_float = (float)total_run_time / 100;

    ESP_LOGD(TAG, "Name           CPU usage Prio Stack high water");
    ESP_LOGD(TAG, "----           --------- ---- ----------------");

    for (int i = 0; i < task_count; i ++)
    {
        percentage_run_time = (float)task_statuses[i].ulRunTimeCounter / total_run_time_float;
        ESP_LOGD(TAG, "%-16s %6.2f%% %4d %16lu",
            task_statuses[i].pcTaskName, 
            percentage_run_time,
            task_statuses[i].uxCurrentPriority,
            task_statuses[i].usStackHighWaterMark);
    }

    exit:
    if (task_statuses)
    {
        vPortFree(task_statuses);
    }

    return ret;
}

static inline uint64_t get_current_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Logs the system status together with an informative tag
esp_err_t log_system_status(const char * text)
{
    static uint64_t last_time_ms = 0;
    uint64_t current_time_ms = get_current_time_ms();

    if (last_time_ms == 0)
    {
        last_time_ms = get_current_time_ms();
    }

    esp_err_t ret = ESP_OK;

    ESP_LOGD(TAG, "====== %s ======", text);
    ESP_LOGD(TAG, "Time lapsed since last status: %"PRIu64" ms\n", current_time_ms - last_time_ms);
    ret = log_tasks();
    ESP_LOGD(TAG,"");
    log_memory();
    ESP_LOGD(TAG,"");

    return ret;
}

void app_main() {
    cpt_job job;
    cpt_preempt preempt;
    cpt_coop coop;
    esp_err_t ret = ESP_ERR_TIMEOUT;

    log_system_status("Initial status");

    // Preemptive test
    cpt_job_init(&job);
    cpt_preempt_init(&preempt, &job);
    cpt_preempt_run_job(&preempt);

    log_system_status("Status after preemptive initialization (not running yet)");

    ESP_LOGI(TAG, "Starting preemptive test");

    while(cpt_preempt_wait_for_join(&preempt, CPT_JOB_PROGRESS_REPORT_INTERVAL_MS) == ESP_ERR_TIMEOUT)
    {
        log_system_status("Status while running");
        ESP_LOGI(TAG, "job counter: %"PRIu64, cpt_preempt_get_job_counter(&preempt));
    }

    log_system_status("Preemptive test done, return value:");
    cpt_preempt_uninit(&preempt);
    ESP_LOGI(TAG, "return status: %s", esp_err_to_name(ret));

    // Reset job
    cpt_job_uninit(&job);
    cpt_job_init(&job);
    ret = ESP_ERR_TIMEOUT;

    log_system_status("Status prior initializing cooperative test");

    cpt_coop_init(&coop, &job);
    cpt_coop_run_job(&coop);

    while(ret == ESP_ERR_TIMEOUT)
    {
        ret = cpt_coop_wait_for_time(&coop, 1000);
        log_memory();
    }

    log_system_status("Cooperative test done");
    cpt_coop_uninit(&coop);
    cpt_job_uninit(&job);

    log_system_status("Test completed");
}