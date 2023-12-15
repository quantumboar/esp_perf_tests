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

    ESP_LOGI(TAG, "==== Memory stats ====");
    ESP_LOGI(TAG, "urrent free: %d minimum free: %d current allocated: %d",
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
    ESP_LOGI(TAG, "==== Tasks stats ====");
    ESP_LOGI(TAG, "%d tasks in system", task_count);
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

    ESP_LOGI(TAG, "-------------- --------- ---- ----------------");
    ESP_LOGI(TAG, "Name           CPU usage Prio Stack high water");
    ESP_LOGI(TAG, "-------------- --------- ---- ----------------");

    for (int i = 0; i < task_count; i ++)
    {
        percentage_run_time = (float)task_statuses[i].ulRunTimeCounter / total_run_time_float;
        ESP_LOGI(TAG, "%-16s %6.2f%% %4d %16lu",
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
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "====== %s ======", text);
    ret = log_tasks();
    ESP_LOGI(TAG,"");
    log_memory();
    ESP_LOGI(TAG,"");

    return ret;
}

void app_main() {
    cpt_job job;
    cpt_type test;
    cpt_coop coop;
    esp_err_t ret = ESP_ERR_TIMEOUT;

#if CPT_FREQUENT_SYSTEM_STATUS_REPORT
    log_system_status("Initial status");
#endif //CPT_FREQUENT_SYSTEM_STATUS_REPORT

    cpt_job_init(&job);
    cpt_init(&test, &job);
    cpt_run_job(&test);

#if CPT_FREQUENT_SYSTEM_STATUS_REPORT
    log_system_status("Status prior starting test");
#endif //CPT_FREQUENT_SYSTEM_STATUS_REPORT

    ESP_LOGI(TAG, "Starting test");

    uint64_t start_time = get_current_time_ms();
    ret = cpt_wait_for_state_change(&test, CPT_WAIT_FOREVER, CPT_STATE_DONE);
    uint64_t duration_ms = get_current_time_ms() - start_time;

    log_system_status("Test completed");
    ESP_LOGI(TAG, "return value: %s duration: %"PRIu64" ms", esp_err_to_name(ret), duration_ms);
    cpt_uninit(&test);
    ESP_LOGI(TAG, "return status: %s", esp_err_to_name(ret));

    cpt_job_uninit(&job);
}