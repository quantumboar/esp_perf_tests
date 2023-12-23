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

#ifndef __CPT_GLOBALS_H__
#define __CPT_GLOBALS_H__

#include <inttypes.h>

// Number of "concurrent" workers to run. In the cooperative single-task mode this means the count of
// Parallel sub-fsms. In the preemptive one it will be the number of tasks. For the preemptive test,
// it's also possible to specify multi-core in cpt_preempt.h
// TODO: figure out a way to have the cooperative test running in multi-core
#define CPT_CONCURRENCY_COUNT (2)

// Report system status more often if set to 0
#define CPT_FREQUENT_SYSTEM_STATUS_REPORT (0)


/*** the api to be used for the test is resolved at compile-time after defining cpt_type ***/
// cpt_type will define what type is going to be used in the test (preemptive or cooperative)
#define cpt_type cpt_preempt

// There's no need to change the lines below (they're used to generate the symbols to use for the cpt_type api)
#define __CPT_METHOD(type, CPT_METHOD) type ## _ ## CPT_METHOD
#define CPT_METHOD(...) __CPT_METHOD(__VA_ARGS__)

// Compile-time solution for class abstraction
#define cpt_init CPT_METHOD(cpt_type, init)
#define cpt_uninit CPT_METHOD(cpt_type, uninit)
#define cpt_run_job CPT_METHOD(cpt_type, run_job)
#define cpt_wait_for_state_change CPT_METHOD(cpt_type, wait_for_state_change)

/*** Generic definitions to be used by any implementation of the cpt_type api ***/

// Use this in *_wait_for_state_change to disable timeout
#define CPT_WAIT_FOREVER (0)

/// @brief test state, useful for state signal handling
typedef enum
{
    CPT_STATE_NONE = 0,
    CPT_STATE_INITIALIZING,
    CPT_STATE_INITIALIZED,
    CPT_STATE_RUNNING,
    CPT_STATE_DONE,
    CPT_STATE_COUNT
} cpt_state;

#endif //__CPT_GLOBALS_H__