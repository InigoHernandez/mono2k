# MONO2K on the FM-1 Firmware Hub

Everything the hub asks for (<https://thefirmwarehub.com/submit/>, <https://thefirmwarehub.com/kb/>), ready to send.

## The mail

To **zven@thefirmwarehub.com**, links only, no attachments. It is the hub's own form (the "Mail me your firmware"
button on <https://thefirmwarehub.com/submit/> opens it with the same subject and questions), answered. Send it only
after the tests below have passed and the v0.1 release is on GitHub: its Repository and Tested lines say so.

> **Subject:** The Firmware Hub: my firmware
>
> Name of the firmware: MONO2K
> Your name (as it should appear): Sequencist
> Repository: https://github.com/InigoHernandez/mono2k (release v0.1, mono2k-0.1.fwsc attached)
> Identity (FM-1_...): FM-1_980
> Licence: GPL-3.0-only, the complete source in the repository
> What it does (two or three sentences): Six mono machines in the spirit of Elektron's Monomachine, one per track:
> FM, superwaves, wavetables, a SID, a voice, four drum synths and three effects. A 64-step sequencer with parameter
> locks, trig conditions and ratchets, and a lofi bus. It comes with four demo parts, a sound library and a web patch
> editor that backs up and restores the FM-1 over USB.
> Picture (link or attached): https://raw.githubusercontent.com/InigoHernandez/mono2k/main/docs/hub/screen-4x.png
> (the screen while it plays: https://raw.githubusercontent.com/InigoHernandez/mono2k/main/docs/hub/mono2k-2x.gif)
> Play in the browser / editor / video (optional): https://inigohernandez.github.io/mono2k/ ·
> https://inigohernandez.github.io/mono2k/editor.html · the manual: https://inigohernandez.github.io/mono2k/manual.html
> Tested on a real FM-1, leaving it again works: yes
>
> It keeps its own store and does not read other firmwares' projects or samples (the README's "On the FM-1"). USB
> port name: MONO2K FM-1 (from 1.0; 0.1 showed "Felucca", the platform's)

## The card

- **Name:** MONO2K
- **By:** Sequencist
- **Kind:** Grooveboxes (`groove` in `fmhub.json`)
- **Text** (`fmhub.json`, 262 characters): Six mono machines in the spirit of Elektron's Monomachine: FM,
  superwaves, wavetables, a SID, a voice and four drum synths, one per track. A 64-step sequencer with parameter
  locks, trig conditions and ratchets, a lofi bus, four demo parts and a web patch editor.
- **Picture:** `docs/hub/screen.png` (240 × 240) and `docs/hub/screen-4x.png` (960 × 960): the engine's own screen in
  dark mode while A01 CIRCUIT plays. `docs/hub/screens-wide.png`: HOME and the chord machine's page.
- **Animation:** `docs/hub/mono2k.gif` (240 × 240) and `docs/hub/mono2k-2x.gif` (480 × 480): four bars on HOME, then the
  chord machine's page, 12.5 s, looping, every frame the engine's own.
- **Play in the browser:** <https://inigohernandez.github.io/mono2k/>
- **Editor:** <https://inigohernandez.github.io/mono2k/editor.html>
- **Manual:** <https://inigohernandez.github.io/mono2k/manual.html>

## The package

- **File:** MONO2K 1.0.3, the installer's `mono2k.fwsc` (596 KB, SHA-256
  `18090b08a338d0aab91d8232d8b002c5ccafd3a0fdd7565ce9b1818aa0dc1f23`); the release v1.0.3 attaches the same file as
  `mono2k-1.0.3.fwsc` (v1.0 had `mono2k-1.0.fwsc`, SHA-256 48a923b7...; v0.1.1 was never released). The hub listed v0.1 on 2026-10-25 (`mono2k-0.1.fwsc`, SHA-256 27790d06...).
- **Release:** GitHub releases `v0.1` (beta on the card), then `v1.0` (out of beta), `v1.0.1` (PADS at each track's root). The hub takes every later release by itself,
  within three hours.
- **Identity:** FM-1_980. No other firmware on the hub uses it (its table, 2026-10-24), and it is of the form
  FM-1_9NN, so the hub resumes an interrupted update. Every release keeps it: `tools/fm1/build.py --release X.Y` sets
  only the version the FM-1 reports and the file's name (FM-1_981 and FM-1_983 are other firmwares').
- **Update loader:** Felucca's, from the CS-1 platform the FM-1 build is made on.
- **USB:** a MIDI device named "MONO2K FM-1", by Sequencist (from 1.0: "FM-1" in the name, so the hub's installer
  probes it). The update loader keeps Felucca's names ("Felucca Update"), by which the hub finds a waiting loader.
- **What it keeps:** its own store in the last 128 KiB of the data area; it does not read other firmwares' projects
  or samples (the README's "On the FM-1").

## Before sending: the hub's tests, on a real FM-1, with that file

1. **From the official firmware:** on <https://thefirmwarehub.com>, "Back to official V15"; then install MONO2K from
   <https://inigohernandez.github.io/mono2k/installer.html>. It starts, and after "Connect the FM-1" the hub shows
   FM-1_980.
2. **From another firmware:** install another one from the hub (Felucca, for one), then MONO2K again.
3. **Leave it:** with MONO2K running, install any firmware from the hub. The update must start: the most important
   test.
4. **USB:** on a Mac, Audio MIDI Setup; note the name the FM-1 shows (for the mail).
5. **An interrupted update:** unplug once during "Writing", plug in, press Install again: it must continue.
6. **Play it:** the four demos, saving, a backup and restore with the Patch Editor.

## Then

1. The v0.1 release on GitHub with the same file (the hub lists only releases).
2. The mail above, from your own address.
3. The hub puts it on its test site, then live; later releases appear by themselves.
4. Optional: "Built from source on GitHub" (the hub's shared workflow `zvenson/fm1-build`), once the build runs on
   GitHub's Ubuntu runner.
