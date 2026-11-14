/* SPDX-License-Identifier: GPL-3.0-only
 * The second core (CPU1): its start, its mailbox, its hold. Written for this project from the facts in
 * pi32v2-isa docs/CPU1.md (the start and stop sequences, tested on an FM-1) and fm1.h.
 *
 * The mailbox: CPU0 writes the job (function, argument, a check word), then the job's number; CPU1 runs the job
 * whose number is one more than its last, and only if the check word matches what it read (some FM-1s' CPU1
 * now and then reads shared RAM wrong); then it writes the number back. Each side writes its own cache line.
 * Everything here runs from RAM (the whole RAM app does). */
#include "fm1.h"
#include "board.h"
#include "cpu1.h"

extern void cpu1_entry(void);
extern uint32_t cpu1_ustack[], cpu1_sstack_top[];

static struct {
    volatile uint32_t request;                              /* CPU0: the job's number */
    cpu1_fn volatile fn;
    volatile uint32_t arg, check;
    volatile uint32_t ready __attribute__((aligned(32)));   /* CPU1: its nonce once started */
    volatile uint32_t done, refused, jobs, cnum;
} mb __attribute__((aligned(32)));

#define CHECK(fn, arg, req) ((uint32_t)(uintptr_t)(fn) ^ (arg) ^ (req) * 0x9E3779B1u)
#define NONCE 0x43315259u                                    /* "C1RY" */
static int running;
static uint32_t saved_entry, entry_saved;

static uint32_t cnum(void) { uint32_t v; __asm__ volatile("%0 = cnum" : "=r"(v)); return v; }

/* ---------------------------------------------------------------- CPU1 */
void cpu1_main(void) __attribute__((noreturn, used));
void cpu1_main(void)
{
    /* its stack limits over both its stacks, less the lowest 256 bytes (a stack that runs down traps first) */
    uint32_t lo = (uint32_t)(uintptr_t)cpu1_ustack + 256u, hi = (uint32_t)(uintptr_t)cpu1_sstack_top - 1u;
    C1_EMU_SSP_H = hi; C1_EMU_SSP_L = lo; C1_EMU_USP_H = hi; C1_EMU_USP_L = lo;
    C1_EMU_CON = (C1_EMU_CON & ~((1u << 2) | (0x1Fu << 16))) | (1u << 3);
    uint32_t last = 0;
    mb.cnum = cnum();
    CSYNC();
    mb.ready = NONCE;
    CSYNC();
    for (;;) {
        uint32_t req = mb.request;
        CSYNC();
        if (req == last) continue;
        if (req == last + 1u) {
            cpu1_fn fn = mb.fn;
            uint32_t arg = mb.arg;
            if (mb.check == CHECK(fn, arg, req)) {
                fn(arg);
                mb.jobs++;
                last = req;
                CSYNC();
                mb.done = req;
                CSYNC();
                continue;
            }
        }
        mb.refused++;                                        /* a misread: read it all again */
        CSYNC();
    }
}

/* ---------------------------------------------------------------- CPU0 */
static int dbg_open(void) { int was = (int)(DBG_WR_EN & 1u); if (!was) DBG_WR_EN = 0xE7u; return was; }
static void dbg_close(int was) { if (!was && (DBG_WR_EN & 1u)) DBG_WR_EN = 0xE7u; }

/* hold it: the late bus-error causes a hold can raise are masked, then cleared (pi32v2-isa's stop procedure) */
void cpu1_stop(void)
{
    int was = dbg_open();
    uint32_t en = DBG_EN;
    DBG_EN = en & ~DBG_BUS_BITS;
    CSYNC();
    C1_CON |= 2u;
    C1_CON &= ~8u;
    CSYNC();
    for (volatile uint32_t n = 0; n < 256u; n++) ;
    DBG_MSG_CLR = DBG_BUS_BITS;
    CSYNC();
    DBG_EN = en;
    dbg_close(was);
    running = 0;
    if (entry_saved) { C1_ENTRY = saved_entry; entry_saved = 0; }   /* the retained area as we found it */
}

int cpu1_start(void)
{
    if (running) return 0;
    cpu1_stop();
    mb.request = 0; mb.fn = 0; mb.arg = 0; mb.check = 0;
    mb.ready = 0; mb.done = 0; mb.refused = 0; mb.jobs = 0; mb.cnum = 0xFFu;
    CSYNC();
    for (int i = 0; i < 32; i++) C1_ICFG(i) = 0;            /* its interrupt sources off: it runs polled */
    /* the start path open: CPU1 goes through the ROM (PC limits wide open) and its entry word is in the
     * write-protected top of RAM */
    int was = dbg_open();
    uint32_t p0h = PC_LIMIT0_H, p0l = PC_LIMIT0_L, p1h = PC_LIMIT1_H, p1l = PC_LIMIT1_L, top = C0_WR_LIMIT_EN & 4u;
    PC_LIMIT0_L = 0; PC_LIMIT0_H = 0xFFFFFFFFu; PC_LIMIT1_L = 0; PC_LIMIT1_H = 0xFFFFFFFFu;
    C0_WR_LIMIT_EN &= ~4u;
    dbg_close(was);
    saved_entry = C1_ENTRY; entry_saved = 1;
    C1_ENTRY = (uint32_t)(uintptr_t)cpu1_entry;
    uint32_t div = SYS_DIV;
    SYS_DIV = div | 8u;
    CSYNC();
    C1_CON |= 8u;
    C1_CON &= ~2u;
    uint32_t t0 = time_ticks(), lim = 20000u * time_mhz();  /* 20 ms */
    while (mb.ready != NONCE && (uint32_t)(time_ticks() - t0) < lim) CSYNC();
    int ok = mb.ready == NONCE;
    SYS_DIV = div;
    was = dbg_open();
    DBG_MSG_CLR = 0xFFFFFFFFu;
    PC_LIMIT0_L = p0l; PC_LIMIT0_H = p0h; PC_LIMIT1_L = p1l; PC_LIMIT1_H = p1h;
    C0_WR_LIMIT_EN |= top;
    dbg_close(was);
    if (!ok) { cpu1_stop(); return -1; }
    running = 1;
    return 0;
}

int cpu1_running(void) { return running; }

int cpu1_run(cpu1_fn fn, uint32_t arg)
{
    CSYNC();
    if (!running || mb.done != mb.request) return 0;
    uint32_t req = mb.request + 1u;
    mb.fn = fn; mb.arg = arg; mb.check = CHECK(fn, arg, req);
    CSYNC();
    mb.request = req;
    CSYNC();
    return 1;
}

int cpu1_wait(uint32_t timeout_us)
{
    /* a misread mailbox is read again at once: the job runs when it reads true, and never twice */
    uint32_t t0 = time_ticks(), lim = timeout_us * time_mhz();
    for (;;) {
        CSYNC();
        if (mb.done == mb.request) return 0;
        if ((uint32_t)(time_ticks() - t0) >= lim) return mb.refused ? -2 : -1;
    }
}

uint32_t cpu1_jobs(void) { CSYNC(); return mb.jobs; }
uint32_t cpu1_misreads(void) { CSYNC(); return mb.refused; }
uint32_t cpu1_core_id(void) { CSYNC(); return mb.cnum; }

/* the chip's USB download mode (m-bootloader SPEC 4.2, 4.3): its marker at the bottom of the retained area, then a
 * reset; the mask ROM sees it. The top of RAM is write-protected: opened first, as for CPU1's entry */
void sys_download_mode(void)
{
    cpu1_stop();
    int was = dbg_open();
    C0_WR_LIMIT_EN &= ~4u;
    dbg_close(was);
    static const char MARK[16] = "usb_update_mode";
    volatile char *r = (volatile char *)(uintptr_t)0x01C7FD80u;
    for (int i = 0; i < 16; i++) r[i] = MARK[i];
    CSYNC();
    SYS_RESET();
}
