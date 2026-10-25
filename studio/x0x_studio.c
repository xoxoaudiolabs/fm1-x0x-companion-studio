/* SPDX-License-Identifier: GPL-3.0-only */
/* X0X for the FM-1 Companion Studio (a Raspberry Pi 3 B+ with a 3.5" 480x320 screen): X0X itself, its UI,
 * sequencer and every engine, running natively on the Pi, played from a real FM-1 whose SLOOP sends its panel
 * over MIDI (SLOOP Remote Out, github.com/xoxoaudiolabs/sloop-remote-out: GLO > MIDI 3 > REMOTE = OUT n), which
 * the FM-1 Companion passes on through a FIFO.
 *
 * Built as X0X's browser version is (web/emu/x0x_web.c): the host simulator, host/x0x_host.c (the whole app
 * against a simulated FM-1), with its script engine left out (X0X_WEB); this file gives it the Studio's sound,
 * screen, the FM-1's panel and a file for what it saves.
 *
 *   x0x-companion-studio --fb /dev/fb0 --midi FIFO [--ch 16] [--state FILE]
 *
 *   screen  X0X's 240x240 picture at 4/3 (320x320) in the middle of the framebuffer, x 80..399; the margins are
 *           the Companion's. SIGUSR1 hides it (it keeps running and playing, drawing nothing), SIGUSR2 shows it
 *   sound   44.1 kHz stereo into PipeWire (pw-cat), the Studio's speaker or jack
 *   panel   on MIDI channel --ch: CC 102 + label (127 down, 0 up) the 14 buttons, CC 116 + role (64 + steps) the
 *           7 knobs, CC 123 MASTER (0..127), notes 36..62 the 27 keys: X0X's panel is the FM-1's, in the same
 *           order, so the panel X0X sees is the one the FM-1's player touches. Anything else is left alone
 *   state   X0X's saved objects (its settings, sounds, patterns, song, motion, and the copy before each) in
 *           --state (default ~/.local/share/x0x-companion-studio/store.bin): read before it starts, written
 *           when X0X saves (it autosaves while stopped) and at the stop, after a last save of what's unsaved
 *   stop    SIGTERM or SIGINT */
#define _GNU_SOURCE                     /* (F_SETPIPE_SZ) */
#include <errno.h>                      /* (before the simulator: it defines __attribute__ away) */
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

static void web_audio(const int32_t *blk, uint32_t n);
#include "../host/x0x_host.c"

#undef __attribute__

#define FB_W 480u
#define VIEW_X 80u                      /* X0X's picture: 320x320 from here, the whole height */
#define VIEW 320u
#define REMOTE_CC0 102u
#define REMOTE_OUT_CC 116u
#define REMOTE_OUT_MASTER 123u
#define REMOTE_OUT_NOTE 36u

static volatile sig_atomic_t stop, hidden, show_whole;
static void on_signal(int s) { (void)s; stop = 1; }
static void on_hide(int s) { (void)s; hidden = 1; }
static void on_show(int s) { (void)s; hidden = 0; show_whole = 1; }

/* ---- the screen reader (announce.c) has no listener here ---- */

static void web_said(const uint8_t *p, uint32_t n) { (void)p, (void)n; }

/* ---- the sound: each 256-frame block to PipeWire; the write paces the whole program ---- */

static FILE *pcm;

static void open_sound(void)
{
    pcm = popen("pw-cat --playback --raw --rate 44100 --channels 2 --format s16 --latency 512 "
                "--media-role Music --properties "
                "'{\"node.name\":\"x0x-companion-studio\",\"node.description\":\"X0X\"}' -", "w");
    if (pcm) {
        fcntl(fileno(pcm), F_SETPIPE_SZ, 4096); /* a small pipe: the write blocks after ~23 ms, so sound keeps time */
        setvbuf(pcm, NULL, _IOFBF, 4096);
    }
}

static void web_audio(const int32_t *blk, uint32_t n)   /* engine_render's 24-bit stereo, as run_ms makes it */
{
    static int16_t out[512 * 2];
    uint32_t k;
    if (n > 512u)
        n = 512u;
    for (k = 0; k < 2u * n; k++) {
        int32_t v = blk[k] >> 8;
        out[k] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    if (pcm && fwrite(out, sizeof out[0], 2u * n, pcm) != 2u * n) {
        pclose(pcm);
        pcm = NULL;
    }
}

/* ---- the panel from MIDI (the FM-1's REMOTE OUT) ---- */

static uint32_t remote_ch = 15u;        /* 0..15 (--ch 1..16) */

static void panel_message(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t kind = st & 0xF0u;
    if ((st & 0x0Fu) != remote_ch)
        return;
    if (kind == 0xB0u && d1 >= REMOTE_CC0 && d1 < REMOTE_CC0 + NB) {
        if (d2 >= 64u)
            held_btn |= 1u << (d1 - REMOTE_CC0);
        else
            held_btn &= ~(1u << (d1 - REMOTE_CC0));
    } else if (kind == 0xB0u && d1 >= REMOTE_OUT_CC && d1 < REMOTE_OUT_CC + NE) {
        enc_acc[d1 - REMOTE_OUT_CC] += (int32_t)d2 - 64;
    } else if (kind == 0xB0u && d1 == REMOTE_OUT_MASTER) {
        uint32_t k10 = d2 * 1023u / 127u;   /* as the FM-1 turns its pot into a gain (main_fm1.c) */
        master = (k10 * k10) >> 8;
    } else if ((kind == 0x90u || kind == 0x80u) && d1 >= REMOTE_OUT_NOTE && d1 < REMOTE_OUT_NOTE + NKEYS) {
        uint32_t k = 1u << (d1 - REMOTE_OUT_NOTE);
        if (kind == 0x90u && d2)
            held_keys |= k;
        else
            held_keys &= ~k;
    }
}

static uint8_t mbuf[3];
static uint32_t mlen, mneed;

static void midi_bytes(const uint8_t *p, ssize_t n)
{
    ssize_t i;
    for (i = 0; i < n; i++) {
        uint8_t b = p[i];
        if (b >= 0xF8u)
            continue;                       /* real-time: not the panel's */
        if (b & 0x80u) {
            mbuf[0] = b, mlen = 1;
            mneed = (b & 0xF0u) == 0xC0u || (b & 0xF0u) == 0xD0u ? 2u : b < 0xF0u ? 3u : 0u;
            continue;
        }
        if (!mneed || mlen >= mneed)
            continue;                       /* (SysEx, or data with no status: not the panel's) */
        mbuf[mlen++] = b;
        if (mlen == mneed) {
            panel_message(mbuf[0], mbuf[1], mneed > 2u ? mbuf[2] : 0u);
            mlen = 1;                       /* running status, should a sender use it */
        }
    }
}

/* ---- the screen: X0X's 240x240 big-endian RGB565, nearest-neighbour to 320x320, only the rows that changed ---- */

static uint16_t shown[240 * 240];
static int fbfd = -1, first = 1;

static void blit(void)
{
    static uint16_t row[VIEW];
    static uint32_t sx[VIEW];
    uint32_t y, x, src_was = 0xFFFFFFFFu, changed = 0;
    if (first)
        for (x = 0; x < VIEW; x++)
            sx[x] = x * 240u / VIEW;
    for (y = 0; y < VIEW; y++) {
        uint32_t sy = y * 240u / VIEW;
        if (sy != src_was) {
            src_was = sy;
            changed = first || memcmp(&fb[sy * 240u], &shown[sy * 240u], 240u * 2u);
            if (changed)
                for (x = 0; x < VIEW; x++)
                    row[x] = swap16(fb[sy * 240u + sx[x]]);
        }
        if (changed && pwrite(fbfd, row, sizeof row, (off_t)((y * FB_W + VIEW_X) * 2u)) < 0)
            return;
    }
    memcpy(shown, fb, sizeof shown);
    first = 0;
}

/* ---- what X0X saves (the simulator's flash: store, store_prev and their lengths), kept in a file ---- */

#define STATE_MAGIC 0x53303058u         /* "X00S" */
static const char *state_path;
static uint32_t saved_writes;

static void state_load(void)
{
    FILE *f = fopen(state_path, "rb");
    uint32_t magic = 0, nobj = 0, max = 0;
    if (!f)
        return;
    if (fread(&magic, 4, 1, f) == 1 && fread(&nobj, 4, 1, f) == 1 && fread(&max, 4, 1, f) == 1 &&
        magic == STATE_MAGIC && nobj == OBJ_NOBJ && max == PLAT_STORE_MAX) {
        int len[OBJ_NOBJ], plen[OBJ_NOBJ];
        static uint8_t s[OBJ_NOBJ][PLAT_STORE_MAX], p[OBJ_NOBJ][PLAT_STORE_MAX];
        if (fread(len, sizeof len, 1, f) == 1 && fread(plen, sizeof plen, 1, f) == 1 &&
            fread(s, sizeof s, 1, f) == 1 && fread(p, sizeof p, 1, f) == 1) {
            memcpy(store_len, len, sizeof len);         /* whole, or not at all */
            memcpy(store_prev_len, plen, sizeof plen);
            memcpy(store, s, sizeof s);
            memcpy(store_prev, p, sizeof p);
        }
    }
    fclose(f);
}

static void state_save(void)            /* when X0X wrote something: a new file put in place (never half a file) */
{
    char tmp[512];
    uint32_t head[3] = {STATE_MAGIC, OBJ_NOBJ, PLAT_STORE_MAX};
    FILE *f;
    if (store_writes == saved_writes)
        return;
    snprintf(tmp, sizeof tmp, "%s.tmp", state_path);
    f = fopen(tmp, "wb");
    if (!f)
        return;
    if (fwrite(head, sizeof head, 1, f) == 1 && fwrite(store_len, sizeof store_len, 1, f) == 1 &&
        fwrite(store_prev_len, sizeof store_prev_len, 1, f) == 1 && fwrite(store, sizeof store, 1, f) == 1 &&
        fwrite(store_prev, sizeof store_prev, 1, f) == 1 && fflush(f) == 0 && fsync(fileno(f)) == 0 &&
        fclose(f) == 0) {
        if (rename(tmp, state_path) == 0)
            saved_writes = store_writes;
        return;
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *fb_path = "/dev/fb0", *midi_path = "/tmp/fm1-x0x-midi";
    static char state_default[512];
    uint32_t next_blit = 0, next_save = 2000;
    int midi, i;
    for (i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "--fb"))
            fb_path = argv[i + 1];
        else if (!strcmp(argv[i], "--midi"))
            midi_path = argv[i + 1];
        else if (!strcmp(argv[i], "--ch") && atoi(argv[i + 1]) >= 1 && atoi(argv[i + 1]) <= 16)
            remote_ch = (uint32_t)atoi(argv[i + 1]) - 1u;
        else if (!strcmp(argv[i], "--state"))
            state_path = argv[i + 1];
    }
    if (!state_path) {
        snprintf(state_default, sizeof state_default, "%s/.local/share/x0x-companion-studio/store.bin",
                 getenv("HOME") ? getenv("HOME") : ".");
        state_path = state_default;
    }
    signal(SIGUSR1, on_hide);
    signal(SIGUSR2, on_show);
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGPIPE, SIG_IGN);
    fbfd = open(fb_path, O_WRONLY);
    if (fbfd < 0) {
        fprintf(stderr, "x0x-companion-studio: %s: %s\n", fb_path, strerror(errno));
        return 1;
    }
    if (mkfifo(midi_path, 0600) < 0 && errno != EEXIST)
        fprintf(stderr, "x0x-companion-studio: %s: %s\n", midi_path, strerror(errno));
    midi = open(midi_path, O_RDONLY | O_NONBLOCK);  /* (it reads nothing until the Companion writes) */
    master = 4096;                          /* full until the FM-1's MASTER says otherwise (CC 123) */
    state_load();                           /* what it saved last time, before it boots from it */
    saved_writes = store_writes;
    boot();
    open_sound();
    while (!stop) {
        uint8_t buf[256];
        ssize_t n;
        while (midi >= 0 && (n = read(midi, buf, sizeof buf)) > 0)
            midi_bytes(buf, n);
        run_ms(1);                          /* the device's clock, a millisecond: the engine every 256 samples (its
                                             * write to PipeWire waits for room: real time), the UI every 16 ms */
        if (show_whole) {                   /* shown again: everything redrawn, all of it written */
            show_whole = 0;
            ui_redraw();
            first = 1;
        }
        if (now_ms >= next_blit) {
            next_blit = now_ms + 16u;
            if (!hidden)
                blit();
        }
        if (now_ms >= next_save) {
            next_save = now_ms + 2000u;
            state_save();
        }
        if (!pcm && now_ms % 6u == 0)       /* no PipeWire: keep time without it */
            usleep(6000);
    }
    if (ui_dirty())
        project_save();                     /* what's not saved yet (X0X saves only while stopped: here, now) */
    state_save();
    if (pcm)
        pclose(pcm);
    return 0;
}
