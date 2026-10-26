/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MIDI IN on the TRS jack: PH8 -> input channel 1 -> UART1 RX, 31250 baud,
 * RX DMA into a 2048-byte ring, polled from the TIMER5 ISR (no UART IRQ).
 * Channel messages go directly into midi_in_q next to USB-MIDI packets
 * (cable 0, CIN = status >> 4). */
#pragma once
#include <stdint.h>
#include "fm1_uart.h"

#define UM_RING 2048u                     /* 655 ms at 31250 baud: a flash erase (interrupts masked, up to 400 ms) cannot overflow it; the DMA fills it meanwhile */
static volatile uint8_t um_ring[UM_RING] __attribute__((aligned(16)));
static struct {
    uint32_t rd, pend;
    volatile uint32_t bytes, drops, msgs;
    uint8_t st, need, got, d0, sysex;
} um;

static void uart_midi_init(void)                   /* before timer5_start(): PORTH RMW */
{
    fm1_uart1_midi_init(um_ring, UM_RING);
}

static inline uint32_t um_len(uint32_t s)         /* data bytes of a channel status */
{
    return (s & 0xE0u) == 0xC0u ? 1u : 2u;         /* Cx Dx: 1, else 2 */
}

static void um_byte(uint32_t b)
{
    if (b >= 0xF8u) {
        if (b == 0xF8u || b == 0xFAu || b == 0xFBu || b == 0xFCu) {
            if (mi_w - mi_r < MQ) {
                midi_in_q[mi_w % MQ] = 0xFu | b << 8;
                RING_PUBLISH();
                mi_w++;
                um.msgs++;
            } else {
                um.drops++;
            }
        }
        return;                                    /* realtime keeps running status */
    }
    if (b & 0x80u) {
        um.sysex = b == 0xF0u;
        um.st = b < 0xF0u || b == 0xF2u ? (uint8_t)b : 0;   /* system common/SysEx cancel running status; the
                                                       song position (F2) takes its two bytes (MONO2K) */
        um.need = (uint8_t)um_len(b);
        um.got = 0;
        return;
    }
    if (um.sysex || !um.st)
        return;
    if (um.need == 2u && !um.got) {
        um.d0 = (uint8_t)b;
        um.got = 1;
        return;
    }
    {
        uint32_t d1 = um.need == 2u ? um.d0 : b, d2 = um.need == 2u ? b : 0u;
        uint32_t pkt = (um.st >> 4) | (uint32_t)um.st << 8 | d1 << 16 | d2 << 24;
        um.got = 0;
        if (um.st == 0xF2u)
            um.st = 0;                             /* (system common has no running status) */
        if (mi_w - mi_r < MQ) {
            midi_in_q[mi_w % MQ] = pkt;
            RING_PUBLISH();
            mi_w++;
            um.msgs++;
        } else {
            um.drops++;
        }
    }
}

static void uart_midi_take(uint32_t n)
{
    um.pend += n;
    if (um.pend > UM_RING) {
        um.drops += um.pend - UM_RING;
        um.rd = (um.rd + um.pend - UM_RING) & (UM_RING - 1u);
        um.pend = UM_RING;
        um.st = um.got = um.sysex = 0;              /* lost bytes: wait for a complete new status */
    }
    while (um.pend) {
        um_byte(um_ring[um.rd]);
        um.rd = (um.rd + 1u) & (UM_RING - 1u);
        um.pend--;
        um.bytes++;
    }
}

static void uart_midi_poll(void)                   /* TIMER5 ISR, same context as usb_poll */
{
    uart_midi_take(fm1_uart1_rx_take());
}
