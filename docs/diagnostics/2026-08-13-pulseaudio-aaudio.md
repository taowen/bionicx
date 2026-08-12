# glibc PulseAudio clients on Android AAudio

## Controlled reproduction

`examples/audio-probe` is a genuine ARM64 glibc X11 and libpulse-simple client.
It opens an X window, connects to the profile-selected PulseAudio Unix socket,
writes five seconds of deterministic 440 Hz signed 16-bit stereo PCM at 48 kHz,
queries latency and drains the stream. It is not an Android audio API client.

Before this change BionicX profiles could only select the Vulkan host service;
VLC consequently used `--no-audio`. The inherited Winlator PulseAudio launcher
also had no modules in the BionicX APK and was unreachable from this activity.

## Generic bridge

Profiles can now request `hostServices: ["pulseaudio"]`. BionicX extracts the
pinned `module-native-protocol-unix` and `module-aaudio-sink` modules, launches
the Bionic PulseAudio server, and exposes its app-private Unix socket. A glibc
PulseAudio 17 client then speaks protocol 35 to the Bionic server's protocol 33
and the server submits audio through AAudio. No glibc code is loaded into the
Bionic process and no Android library is loaded into the glibc process.

The executable, modules and six Android shared libraries come from the same
Winlator source snapshot `c2f4ad4534f4637b543a9a3b085e28f50cf6d01c`
recorded in `NOTICE`. Binary inputs are individually SHA-256 pinned by
`tools/prepare-pulseaudio-runtime.sh`; the module archive SHA-256 is
`a31d331e1f1ec514d8287b830c1668c1d65a9b34c21010bd918eca43d2f4947f`.

## x300 evidence

The final run was untraced as `u0_a194` on serial `01408BH601027129`:

```text
BXTEST PASS host-audio connect=ok rate=48000 channels=2 frames=240000 bytes=960000 initialLatencyUsec=20000
BXSUMMARY host-audio passed=1 failed=0
```

At the same time Android AudioFlinger reported a 48 kHz active track,
`numTracks=1 writeErrors=0 underruns=0 overruns=0`. This proves that success was
not merely a PulseAudio client-side acknowledgement.

The real Debian VLC 3.0.23 integration was then upgraded from video-only raw
I420 to a single AVI containing I420 and PCM. VLC logs identify the `avi`,
`rawvideo`, `araw` and `pulse` modules, show a local connection to `AAudioSink`,
and report `s16l 48000 Hz Stereo`. Its video remained visible while AudioFlinger
again reported an active, zero-error track.

Evidence:

- `evidence/host-audio-playing.png`
- `evidence/host-audio.log`
- `evidence/bionicx-trixie-vlc-audio.png`
- `evidence/vlc-pulseaudio.log`
