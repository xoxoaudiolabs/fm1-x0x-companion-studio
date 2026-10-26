# X0X for the FM-1 Companion Studio

[X0X](https://github.com/charlesvestal/fm1-x0x), Charles Vestal's groovebox firmware for the M-VAVE FM-1 (909, 808,
two 303s with TB-3PO, a breakbeat player, song mode), running natively on the
[FM-1 Companion](https://github.com/xoxoaudiolabs/FM-1-Companion) Studio (a Raspberry Pi 3 B+ with a 3.5" 480×320
touchscreen), played from a real FM-1.

It's X0X's own code: its UI, sequencer and every engine, built for the Pi the way X0X's browser version is built
(`web/emu/x0x_web.c`): X0X's host simulator (`host/x0x_host.c`) with its script engine left out, and
`x0x_studio.c` giving it the Studio's sound, screen, saving and the FM-1's panel. Nothing is emulated.

**Status:** works with FM-1 Companion Studio v1.5 or later. What's been tried on the real Studio and what
hasn't yet: *Tested* below.

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
- **Clock, with SLOOP on the Studio** ([SLOOP Remote Out](https://github.com/xoxoaudiolabs/sloop-remote-out)): the
  Companion (its main branch after Studio v1.5) runs both at once and connects each one's MIDI clock to the other,
  as two FM-1s cabled both ways. X0X follows an incoming clock by itself (its tempo turns amber) and sends its own
  with GLOBAL › CLOCK OUT on, while it isn't following one. SLOOP leads by default there; to have X0X lead, set
  SLOOP's SYNC to USB. X0X follows the other app's clock only while that app plays, so X0X playing on its own isn't
  pulled off its beat when SLOOP opens. One change inside X0X's sequencer: clock ticks that arrive together share
  the time since the last ones, so the tempo shown while following is right (it read 115 for 120).
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

On the real Studio (Raspberry Pi 3 B+, Raspberry Pi OS Trixie), 2026-10-25.

**By hand, from a real FM-1 A** running SLOOP Remote Out (REMOTE on OUT 16), opened from the Companion's
**PLAY X0X**:
- X0X's screens on the Studio, moved around from FM-1 A's panel.
- ENV held: X0X's help, and accents placed. (ENV does nothing alone in X0X. On an FM-1 running X0X it also lights
  the accents on the key LEDs; here those LEDs are FM-1 A's own, showing SLOOP.)
- FM-1 A's REMOTE OUT itself: every knob turn and MASTER seen arriving on channel 16.

**Stress tests, driven by a script** sending the same MIDI messages FM-1 A's REMOTE OUT sends (not by hand),
recorded by the Companion's recorder, X0X's screen captured 8 times a second (videos made of both):
- **X0X alone, 41 s:** a song written on every part (the 909 with an accent, the 808, a TB-3PO line on each 303,
  the break on every step), then, playing: the 909 muted and unmuted (HOME, black key 1), 303B mutated in TB-3PO
  (ARP, OCT−), the break played backwards (black key 9 held) and the tempo 125 → 135 → 125 (HOME + SELECT). Each
  step seen on the captured screen. No gaps in the sound, peak 42%, the recorder's clip check found nothing,
  0 PipeWire errors. X0X took 8–31% of one core (about 20% on average), load average 0.5.
- **X0X and SLOOP Remote Out's Studio program at once, 33 s,** both at 120 BPM: X0X as above, SLOOP playing four
  recorded tracks. No gaps, the clip check found nothing (peak 94%: two apps at full level add up), 0 PipeWire
  errors; X0X about 19% of a core, SLOOP about 9%. They were started together with no shared clock (this test
  came before it), so they drifted apart. This ran both as test programs of their own: the Companion itself runs one app at a time (starting
  one stops the other).
- Both exited cleanly when stopped (whether they saved wasn't checked in these runs).
- Earlier checks: X0X's host simulator renders its showcase, perf and jam scripts in 5–8% of real time on one
  core; a sound change (KNOB 1: TUNE 34 → 44) saved at the stop on a PC; on the Studio, a first short check
  (three drum tracks, 8ths at 125 BPM) saved its file at the stop.

**Clock with SLOOP** (2026-10-25):
- By hand, from FM-1 A through the Companion's PLAY tiles: both running at once, SLOOP leading, playing together
  in time by ear. A stutter when SLOOP opened was found this way and fixed; by hand afterwards, it was gone.
- By a script on the Studio, each app's sound recorded: X0X following SLOOP at 120 and 130 BPM, through STOP and
  PLAY, with no drift (each bar the same length in both), its tempo shown as 120–121 and 132 for 120 and 130;
  SLOOP following X0X the same way. Two X0Xs, one following the other, in one recording: the follower 9–13 ms
  after the leader on average, about 8–9 ms of jitter. X0X's beat steady while SLOOP was opened and closed next to
  it.
- X0X's own host tests: the sequencer and external-clock tests pass with the sequencer change. (Some of its other
  tests can't run here: they need reference sources that aren't checked out, and fail the same without the change.)

**Not tested yet:**
- X0X leading SLOOP by hand (only by the script).
- Turning off the leading app (HOLD OFF) while both play: the other should carry on at its own tempo, after a stop
  of about half a second.
- Every control one by one by hand (the stress test's controls were sent by a script).
- The Companion's **< HOME**, **HOLD OFF** and + / − for X0X, and its MIXER channel.
- Song mode on the Studio.
- A save loaded back by X0X on the Studio after a restart.

## License

GPL-3.0-only ([LICENSE](../LICENSE)), as X0X: built from X0X's sources by Charles Vestal, a fork of Felucca by Leo
Kuroshita / Hügelton Instruments. What's ours in this fork: see the note at the top of [the repo's README](../README.md).

**Built and run with** (on the Studio, from Raspberry Pi OS's packages; not copied into this repository, each
under its own licence):

| Software | What for | Licence |
|---|---|---|
| GCC | building it | GPL-3.0-or-later (what it builds isn't bound by it: the GCC Runtime Library Exception) |
| the GNU C library (glibc), libm | linked into it | LGPL-2.1-or-later |
| Python 3, with Pillow | X0X's build scripts that make its generated headers (Pillow for the font) | PSF License; the PIL Software License (MIT-CMU) |
| PipeWire (`pw-cat`) | its sound | MIT |
| the Linux kernel (the framebuffer) | its screen | GPL-2.0 |

The licences are each project's own, as Debian lists them in `/usr/share/doc/<package>/copyright`: that file is
the one to go by.
