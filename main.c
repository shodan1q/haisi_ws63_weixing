/*
 * haisi_ws63_weixing - personal app entry for WS63 SDK
 *
 * Layout:
 *   src/application/samples/custom/   <- clone this repo here (rename during clone)
 *     - main.c                        <- this file
 *     - CMakeLists.txt
 *
 * The SDK auto-discovers `custom/` via add_subdirectory_if_exist(custom)
 * in src/application/samples/CMakeLists.txt, so no SDK file needs editing.
 * The app_run() macro at the bottom registers weixing_entry() to be called
 * automatically at boot via the SDK's initcall mechanism.
 */

#include "common_def.h"
#include "soc_osal.h"
#include "app_init.h"

#define WEIXING_TASK_STACK_SIZE  0x1000
#define WEIXING_TASK_PRIORITY    26
#define WEIXING_DELAY_MS         1000

static void *weixing_task(const char *arg)
{
    unused(arg);
    osal_printk("[weixing] task started\r\n");
    for (;;) {
        osal_printk("[weixing] alive\r\n");
        osal_msleep(WEIXING_DELAY_MS);
    }
    return NULL;
}

static void weixing_entry(void)
{
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)weixing_task, 0,
                                      "weixing", WEIXING_TASK_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, WEIXING_TASK_PRIORITY);
    }
    osal_kthread_unlock();
}

app_run(weixing_entry);
