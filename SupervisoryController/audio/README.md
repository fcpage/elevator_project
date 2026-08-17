# Floor announcement assets

The currently bundled prerecorded WAV files are:

```text
floor1.wav
floor2.wav
floor3.wav
```

Replace these files as needed; keep the
same filenames so the supervisory application and Raspberry Pi build scripts
continue to find them automatically.

The real Raspberry Pi build copies these files beside the executable when they
exist. The service uses miniaudio's PulseAudio backend and plays the file
matching the confirmed arrival floor.

Phase 2 may run without these files when `SUPERVISORY_ENABLE_MINIAUDIO` is
disabled; the announcement service then logs `AUDIO_DEMO_ANNOUNCEMENT` records
for video/test evidence.
