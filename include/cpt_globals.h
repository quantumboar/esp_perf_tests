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

// Global defines for running the performance test. They're placed here rather than using
// -D options in platformio.ini 'cause doing the latter always results into a tedious
// rebuild-the-universe-just-in-case event which, with my 11 years old Linux laptop, translates
// into a 10 minutes break just to test a different stack size. 

// Number of concurrent tasks to run. In the cooperative single-task mode this means the count of
// Parallel sub-fsms
#define CPT_TASK_COUNT (2)

// Report system status more often if set to 0
#define CPT_FREQUENT_SYSTEM_STATUS_REPORT (0)

// Test to use (preemptive or cooperative)
#define cpt_type cpt_preempt

// No need to edit these (they're used to generate the symbols to use for the test)
#define __METHOD(type, method) type ## _ ## method
#define METHOD(...) __METHOD(__VA_ARGS__)

// Compile-time solution for class abstraction
#define cpt_init METHOD(cpt_type, init)
#define cpt_uninit METHOD(cpt_type, uninit)
#define cpt_run_job METHOD(cpt_type, run_job)
#define cpt_wait_for_state_change METHOD(cpt_type, wait_for_state_change)

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