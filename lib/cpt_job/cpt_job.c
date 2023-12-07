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

#include <cpt_job.h>
#include "esp_err.h"
#include "esp_log.h"

#define TAG "cpt_job"

#define CPT_JOB_MAX_COUNT (10000)

esp_err_t cpt_job_init(cpt_job * job)
{
    * job = (cpt_job) {0};
    return ESP_OK;
}

void cpt_job_uninit(cpt_job * job)
{
    (void)job;
}

// Function to be used to access the shared counter. It's *not* thread safe
cpt_job_status cpt_job_run(cpt_job * job)
{
    if (cpt_job_get_status(job) == CPT_JOB_NOT_DONE)
    {
        job->counter ++;
        return CPT_JOB_NOT_DONE;
    }

    return CPT_JOB_DONE;
}

cpt_job_status cpt_job_get_status(cpt_job * job)
{
    if (job->counter < CPT_JOB_MAX_COUNT)
    {
        return CPT_JOB_NOT_DONE;
    }

    return CPT_JOB_DONE;
}