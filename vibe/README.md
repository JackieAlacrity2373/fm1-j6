# J6 for VIBE FM (prepared, not yet built)

The VIBE FM SDK ("C → one `.wasm`") is not public yet, and thefirmwarehub.com's converter rejects the FM-1 layout of
this repository. This folder holds what can be prepared without the SDK.

- `j6_vibe.h` / `j6_vibe.c`: a platform-neutral wrapper around the engine ([../src/juno.c](../src/juno.c)): MIDI in,
  program change, float stereo out. No FM-1 or SDK calls. The engine renders at a fixed 44100 Hz.
- `wrapper_test.c`: host check (note on, sound, note off, silence).

```
gcc -O2 -ffp-contract=off -I fm1-base/include -o build/vibe_test vibe/wrapper_test.c vibe/j6_vibe.c src/juno.c -lm
```

## When the SDK is released

1. Write the SDK glue (`vibe/j6_sdk.c`): create the instrument, call `j6v_midi` for each incoming MIDI event and
   `j6v_process` in the audio callback.
2. Expose the sound parameters (`j_p`, `j_g`) as the SDK's parameters; MIDI CCs already reach most of them via `j_event`.
3. Handle hosts that don't run at 44100 Hz (resample, or make `SR` in `src/juno.c` variable).
4. Draw the screen and panel the SDK's way; the FM-1 UI in `src/app.c` is not reused.
5. Build with the SDK, then publish the `.wasm`, VST3 and AU in a release.
