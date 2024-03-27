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

#ifndef __CPT_UTILS_H__
#define __CPT_UTILS_H__

#include <inttypes.h>
#include "esp_err.h"

/// @brief get the current time in ms
uint64_t cpt_get_current_time_ms();

/// @brief log the system mamory status
void cpt_log_memory();

/// @brief  Logs the systems status: memory, tasks stats, time
/// @discussion to properyl log the task status (e.g. uxTaskGetSystemState) you'll need to enable the RTOS tracing features
/// via setting the build option -DCONFIG_FREERTOS_USE_TRACE_FACILITY, f.e. for platformio.ini:
/// build_flags = -DCONFIG_FREERTOS_USE_TRACE_FACILITY
/// To generate thred stats, you'll also need GENERATE_RUN_TIME_STATS
/// @param label A label to be show in the header of the system status log
/// @return ESP_OK or an error code
esp_err_t cpt_log_system_status(const char * label);

#endif // __CPT_UTILS_H__