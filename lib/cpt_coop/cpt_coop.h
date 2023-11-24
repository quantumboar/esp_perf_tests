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

#ifndef __CPT_COOP_H__
#define __CPT_COOP_H__

#include "cpt_job.h"

typedef enum
{
    CPT_FSM_STATE_NONE,
    CPT_FSM_STATE_A,
    CPT_FSM_STATE_B,
    CPT_FSM_STATE_COUNT
} cpt_coop_state;

typedef struct
{
    unsigned long counters[CPT_TASK_COUNT];
    cpt_coop_state state;
    cpt_job * job;
} cpt_coop;

esp_err_t cpt_coop_init(cpt_coop * coop, cpt_job * job);
void cpt_coop_uninit(cpt_coop * coop);

esp_err_t cpt_coop_run_job(cpt_coop * coop);
esp_err_t cpt_coop_wait_for_time(cpt_coop * coop, uint16_t time_ms);

#endif //__CPT_COOP_H__

