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

#ifndef __CPT_JOB_H__
#define __CPT_JOB_H__

#include "esp_err.h"

typedef enum
{
    CPT_JOB_NOT_DONE,
    CPT_JOB_DONE
} cpt_job_status;

// A job here is simply a counter to be incremented at each interaction
// it's oblivious of the number of tasks running it, or the synchronization mechanism.
typedef struct
{
    unsigned long long counter;
} cpt_job;

esp_err_t cpt_job_init(cpt_job * job);
void cpt_job_uninit(cpt_job * job);
cpt_job_status cpt_job_run(cpt_job * job);

#endif //__CPT_JOB_H__
