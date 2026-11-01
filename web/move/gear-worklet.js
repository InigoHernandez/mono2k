// SPDX-License-Identifier: GPL-3.0-only
// MONO2K on the Move, in the browser: its AudioWorklet. One WebAssembly instance of m2k_move.wasm (the Move's build:
// eight tracks, its controller, its 128 x 64 screen) runs here as dsp.so runs on the Move's audio thread: the controls
// come in as messages (gear.js), each 128 frames rendered at 44.1 kHz drain them first, and every two of the Move's
// panel frames (34.83 ms of sound, ~28.7 a second) the page state is built, the screen drawn and both sent to the page.
const FRAME_SAMPLES = 2 * 6 * 128;     /* the Move's pace: two panel frames of six 128-sample slices */

class MoveGearProc extends AudioWorkletProcessor {
  constructor() {
    super();
    this.x = null;
    this.due = 0;
    this.port.onmessage = (ev) => this.msg(ev.data);
  }

  msg(m) {
    if (m.t === "wasm") {
      const x = new WebAssembly.Instance(new WebAssembly.Module(m.bytes), {}).exports;
      if (x._initialize) x._initialize();
      x.w_init(m.seed >>> 0);
      this.x = x;
      this.audio = new Int16Array(x.memory.buffer, x.w_audio(), 256);
      this.nleds = x.w_nleds();
      this.port.postMessage({ t: "ready", rate: sampleRate });
      this.frame();
      return;
    }
    const x = this.x;
    if (!x) return;
    for (const c of m.list || [m]) {
      switch (c.t) {
        case "pad": x.w_pad(c.i, c.v); break;
        case "step": x.w_step(c.i, c.v); break;
        case "knob": x.w_knob(c.i, c.v); break;
        case "touch": x.w_touch(c.i, c.v); break;
        case "jog": x.w_jog(c.v); break;
        case "btn": x.w_button(c.i, c.v); break;
        default: break;
      }
    }
  }

  /* the page state and the screen, as the Move's drawer makes them */
  frame() {
    const x = this.x, mem = x.memory.buffer;
    const fb = new Uint8Array(mem, x.w_frame(), 1024).slice();
    const leds = new Uint8Array(mem, x.w_leds(), this.nleds).slice();
    const st = new Uint8Array(mem, x.w_state(), 48).slice();
    this.port.postMessage({ t: "frame", fb, leds, st }, [fb.buffer, leds.buffer, st.buffer]);
  }

  process(inputs, outputs) {
    const x = this.x;
    if (!x) return true;
    const out = outputs[0], L = out[0], R = out[1] || out[0], n = L.length;
    if (sampleRate === 44100) {
      for (let o = 0; o < n; o += 128) {
        x.w_render(128);
        this.due += 128;
        const a = this.audio;
        for (let i = 0; i < 128 && o + i < n; i++) {
          L[o + i] = a[2 * i] / 32768;
          R[o + i] = a[2 * i + 1] / 32768;
        }
      }
    } else {
      /* the core runs at 44.1 kHz as on the Move; read it at this context's rate, linearly (worklet.js's way) */
      if (!this.rs) this.rs = { pos: 0, step: 44100 / sampleRate, i: 128, prevL: 0, prevR: 0, curL: 0, curR: 0 };
      const s = this.rs, a = this.audio;
      for (let o = 0; o < n; o++) {
        s.pos += s.step;
        while (s.pos >= 1) {
          s.pos -= 1;
          if (s.i >= 128) { x.w_render(128); this.due += 128; s.i = 0; }
          s.prevL = s.curL; s.prevR = s.curR;
          s.curL = a[2 * s.i] / 32768; s.curR = a[2 * s.i + 1] / 32768;
          s.i++;
        }
        L[o] = s.prevL + (s.curL - s.prevL) * s.pos;
        R[o] = s.prevR + (s.curR - s.prevR) * s.pos;
      }
    }
    if (this.due >= FRAME_SAMPLES) {
      this.due -= FRAME_SAMPLES;
      this.frame();
    }
    return true;
  }
}
registerProcessor("mono2k-move", MoveGearProc);
