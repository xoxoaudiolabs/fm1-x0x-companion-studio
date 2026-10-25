# X0X for the FM-1 Companion Studio

[X0X](https://github.com/charlesvestal/fm1-x0x), Charles Vestal's groovebox firmware for the M-VAVE FM-1 (909, 808,
two 303s with TB-3PO, a breakbeat player, song mode), running natively on the
[FM-1 Companion](https://github.com/xoxoaudiolabs/FM-1-Companion) Studio (a Raspberry Pi 3 B+ with a 3.5" 480×320
touchscreen), played from a real FM-1.

It's X0X's own code: its UI, sequencer and every engine, built for the Pi the way X0X's browser version is built
(`web/emu/x0x_web.c`): X0X's host simulator (`host/x0x_host.c`) with its script engine left out, and
`x0x_studio.c` giving it the Studio's sound, screen, saving and the FM-1's panel. Nothing is emulated.

**Status: in development, not released.** See *Tested* below.

## How it works

- **Control:** a real FM-1 running [SLOOP Remote Out](https://github.com/xoxoaudiolabs/sloop-remote-out) with
  GLO › MIDI 3 › REMOTE on **OUT 16**. It sends the FM-1's whole panel (14 buttons, 7 knobs, MASTER, 27 keys);
  X0X's panel is the same, so every control does what it does on an FM-1 running X0X. The FM-1 itself keeps
  showing and playing SLOOP: watch the Studio's screen, and turn FM-1 A down in the Companion's MIXER.
- **Screen:** X0X's 240×240 picture at 4/3 size (320×320) in the middle of the Studio's screen. The Companion draws
  the margins: **< HOME** (top left) goes back to the Companion with X0X still running (and playing, if it was),
  **PLAY X0X** in MORE shows it again, **HOLD OFF** (bottom left, held a second) saves and stops it, and + / − on
  the right are its level (also an X0X channel in the MIXER).
- **Sound:** 44.1 kHz stereo into PipeWire, wherever the Studio plays.
- **Saved:** what X0X saves (settings, sounds, patterns, song, motion: its flash objects) in
  `~/.local/share/x0x-companion-studio/store.bin`, read before it starts. X0X autosaves while stopped; at the stop,
  anything not yet saved is saved first.
- **Not there:** break loops of your own (the user sample slots: the built-in break works); the screen reader.

## Build (on the Studio)

```sh
git clone https://github.com/xoxoaudiolabs/fm1-x0x-companion-studio
cd fm1-x0x-companion-studio
sh studio/build.sh                  # needs gcc and python3; about 1.5 minutes on a Pi 3 B+
mkdir -p ~/.local/share/x0x-companion-studio && cp studio/x0x-companion-studio ~/.local/share/x0x-companion-studio/
```

The Companion (Studio v1.5 or later) shows **PLAY X0X** in MORE once that file exists.

## Tested

On the real Studio, 2026-10-25, driven by a script sending the FM-1's REMOTE OUT messages (not yet by hand on an
FM-1):

- X0X's host simulator on the Pi: its showcase, perf and jam scripts render in 5–8% of real time on one core.
- X0X's 909 screen drawn on the Studio's screen; steps toggled from the keys; PLAY starts it (the playhead runs).
- Its sound reaches PipeWire ("X0X"), 0 errors; the Studio's recorder records it (8ths at 125 BPM, peak 41%, its
  clip check clean).
- Saving: on a PC, a sound changed (KNOB 1: TUNE 34 → 44) and saved at the stop; on the Studio, saved at the stop.

**Not tested yet:** playing it from a real FM-1 by hand; PLAY X0X, < HOME, HOLD OFF and its + / − in the Companion;
the 808, 303s, breaks and song mode on the Studio; loading a save back on the Studio.

## License

GPL-3.0-only ([LICENSE](../LICENSE)), as X0X: built from X0X's sources by Charles Vestal, a fork of Felucca by Leo
Kuroshita / Hügelton Instruments.
