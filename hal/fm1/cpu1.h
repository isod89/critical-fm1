/* SPDX-License-Identifier: GPL-3.0-only
 * The second core (cpu1.c, cpu1_entry.S): a worker that runs one job at a time for CPU0, polled.
 *   cpu1_start()          once per power-on, before the audio: 0 it answered, -1 it did not (then held)
 *   cpu1_run(fn, arg)     1 it has the job; 0 do it yourself (not running, or a job outstanding)
 *   cpu1_wait(us)         0 done; -1 late; -2 it misread the mailbox (some FM-1s' CPU1 now and then reads shared
 *                         RAM wrong: a job is run only when its check word matches, never a wrong one)
 *   cpu1_stop()           hold it (before returning to the bootloader) */
#pragma once
#include <stdint.h>

typedef void (*cpu1_fn)(uint32_t);
int cpu1_start(void);
int cpu1_running(void);
int cpu1_run(cpu1_fn fn, uint32_t arg);
int cpu1_wait(uint32_t timeout_us);
void cpu1_stop(void);
uint32_t cpu1_jobs(void);                /* the jobs it has done */
uint32_t cpu1_misreads(void);            /* the jobs it refused (check word wrong) */
uint32_t cpu1_core_id(void);             /* the core number CPU1 read (cnum): 1 */
void sys_download_mode(void);            /* the soft key: the chip's USB download mode (M-Flasher); no return */
