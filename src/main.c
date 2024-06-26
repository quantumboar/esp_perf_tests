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

#include "esp_check.h"
#include "cpt_preempt.h"
#include "cpt_coop.h"

#include "cpt_utils.h"

#define TAG "cpt"

void app_main() {
    cpt_job job;
    cpt_type test;
    esp_err_t ret = ESP_ERR_TIMEOUT;

#if CPT_FREQUENT_SYSTEM_STATUS_REPORT
    cpt_log_system_status("Initial status");
#endif //CPT_FREQUENT_SYSTEM_STATUS_REPORT

    cpt_job_init(&job);
    cpt_init(&test, &job);
    cpt_run_job(&test);

#if CPT_FREQUENT_SYSTEM_STATUS_REPORT
    cpt_log_system_status("Status prior starting test");
#endif //CPT_FREQUENT_SYSTEM_STATUS_REPORT

    ESP_LOGI(TAG, "Starting test");

    uint64_t start_time = cpt_get_current_time_ms();
    ret = cpt_wait_for_state_change(&test, CPT_WAIT_FOREVER, CPT_STATE_DONE);
    uint64_t duration_ms = cpt_get_current_time_ms() - start_time;

    cpt_log_system_status("Test completed");
    ESP_LOGI(TAG, "return value: %s duration: %"PRIu64" ms", esp_err_to_name(ret), duration_ms);
    cpt_uninit(&test);
    ESP_LOGI(TAG, "return status: %s", esp_err_to_name(ret));

    cpt_job_uninit(&job);
}