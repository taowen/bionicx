#include <X11/Xlib.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    SAMPLE_RATE = 48000,
    CHANNELS = 2,
    TONE_SECONDS = 5,
    CHUNK_FRAMES = 960,
};

static void fail(const char *stage, int error) {
    fprintf(stderr, "BXTEST FAIL host-audio stage=%s error=%d message=%s\n",
            stage, error, pa_strerror(error));
    exit(1);
}

static void paint(Display *display, Window window, GC gc,
                  unsigned long background, const char *message) {
    XSetWindowBackground(display, window, background);
    XClearWindow(display, window);
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
    XDrawString(display, window, gc, 36, 116, message, (int)strlen(message));
    XFlush(display);
}

int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        fputs("BXTEST FAIL host-audio stage=x-open\n", stderr);
        return 1;
    }
    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen),
                                        120, 120, 720, 240, 0,
                                        BlackPixel(display, screen), 0x16324f);
    XStoreName(display, window, "BionicX host audio probe");
    XSelectInput(display, window, ExposureMask | StructureNotifyMask);
    XMapWindow(display, window);
    GC gc = XCreateGC(display, window, 0, NULL);
    paint(display, window, gc, 0x16324f,
          "Connecting glibc PulseAudio client to Android AAudio...");

    const pa_sample_spec sample_spec = {
        .format = PA_SAMPLE_S16LE,
        .rate = SAMPLE_RATE,
        .channels = CHANNELS,
    };
    int error = 0;
    pa_simple *pulse = pa_simple_new(NULL, "bionicx-audio-probe",
                                     PA_STREAM_PLAYBACK, NULL,
                                     "deterministic-square-wave",
                                     &sample_spec, NULL, NULL, &error);
    if (pulse == NULL) fail("pulse-connect", error);

    paint(display, window, gc, 0x146b3a,
          "PLAYING: 440 Hz / stereo / 48 kHz / signed 16-bit PCM");
    int16_t samples[CHUNK_FRAMES * CHANNELS];
    uint32_t phase = 0;
    uint64_t frames_written = 0;
    pa_usec_t latency = 0;
    const uint32_t phase_step = (uint32_t)(((uint64_t)440 << 32) / SAMPLE_RATE);
    for (int chunk = 0;
         chunk < TONE_SECONDS * SAMPLE_RATE / CHUNK_FRAMES; ++chunk) {
        for (int frame = 0; frame < CHUNK_FRAMES; ++frame) {
            int16_t sample = (phase & 0x80000000u) ? 6000 : -6000;
            phase += phase_step;
            samples[frame * 2] = sample;
            samples[frame * 2 + 1] = sample;
        }
        if (pa_simple_write(pulse, samples, sizeof(samples), &error) < 0)
            fail("pulse-write", error);
        frames_written += CHUNK_FRAMES;
        if (chunk == 0) {
            latency = pa_simple_get_latency(pulse, &error);
            if (latency == (pa_usec_t)-1) fail("pulse-latency", error);
        }
        while (XPending(display) > 0) {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == Expose)
                paint(display, window, gc, 0x146b3a,
                      "PLAYING: 440 Hz / stereo / 48 kHz / signed 16-bit PCM");
        }
    }
    if (pa_simple_drain(pulse, &error) < 0) fail("pulse-drain", error);
    pa_simple_free(pulse);
    paint(display, window, gc, 0x1a704c,
          "PASS: Android audio stream accepted and drained 240000 frames");
    printf("BXTEST PASS host-audio connect=ok rate=%d channels=%d "
           "frames=%llu bytes=%llu initialLatencyUsec=%llu\n",
           SAMPLE_RATE, CHANNELS,
           (unsigned long long)frames_written,
           (unsigned long long)(frames_written * CHANNELS * sizeof(int16_t)),
           (unsigned long long)latency);
    puts("BXSUMMARY host-audio passed=1 failed=0");
    fflush(stdout);
    sleep(2);
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
