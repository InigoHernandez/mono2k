/* SPDX-License-Identifier: GPL-3.0-only */
/* MONO2K on the FM-1: the glue's pacing of the sound and the screen (m2k_fm1.c), apart from the platform so that the
 * host's guard bench (host/m2k_guard_bench.c) runs this very code against load traces. The includer defines RING (the
 * ring's blocks), BLK_TICKS (a render's real time in ticks) and PACE_TICKS_MS (ticks a millisecond), and has the core's
 * m2k_guard and m2k_guard_gap (m2k.h) */
#ifndef M2K_PACE_H
#define M2K_PACE_H

static uint32_t ravg;                   /* the render's time, followed over ~8 renders */
static uint32_t busy_ticks, busy_t0;    /* the first core's busy time (renders, frames, strips) over each quarter second */
static uint32_t frame_gap = 33u;        /* ms between frames: 33 (30 a second) to 100, as the first core has room */

/* before a render: the CPU guard (m2k_guard, 1.11), on the render's load, the blocks ahead and the shorts so far; it
 * may fade a track first, or have the screen give way */
static void pace_guard(uint32_t ahead, uint32_t shorts)
{
    m2k_guard((int)(ravg / (BLK_TICKS / 100u)), (int)ahead, (int)RING, (int)shorts);
}

/* after it: its time */
static void pace_rendered(uint32_t dt)
{
    ravg = ravg - (ravg >> 3) + (dt >> 3);
    busy_ticks += dt;
}

/* the first core's share (the sound, the frames, the strips) every quarter second: over 85 % the frames come further
 * apart, under 70 % closer */
static void pace_busy(uint32_t now)
{
    uint32_t span = now - busy_t0;
    if (span < 250u * PACE_TICKS_MS) return;
    uint32_t pct = busy_ticks / (span / 100u);
    if (pct > 85u) frame_gap = frame_gap + 8u > 100u ? 100u : frame_gap + 8u;
    else if (pct < 70u) frame_gap = frame_gap < 37u ? 33u : frame_gap - 4u;
    busy_ticks = 0;
    busy_t0 += span;
}

/* may a frame be drawn, SINCE ms after the last with AHEAD blocks in the ring. The sound first: while the ring is under
 * half full a frame waits (it is drawn whole, nothing rendered meanwhile), 150 ms at most; while the guard has the
 * screen give way (m2k_guard_gap), its gap at least, and twice that at most */
static int pace_frame_due(uint32_t since, uint32_t ahead)
{
    const uint32_t g = (uint32_t)m2k_guard_gap(), gap = frame_gap > g ? frame_gap : g;
    return since >= gap && (ahead >= RING / 2u || since >= (g ? 2u * g : 150u));
}
#endif
