# J6 user guide

A beginner's guide to J6, the JUNO-60 style synth firmware for the M-VAVE FM-1. No synth experience needed.

## 1. What you have
J6 turns the FM-1 into a six-voice synthesizer: you can play six notes at once on the touch keyboard. Every sound is
built from a few simple parts (see section 4), and you shape it with four knobs and a few buttons.

## 2. First sound
1. Power on. The **HOME** page shows the sound name and four main settings.
2. Touch the keys. Left is lower, right is higher. Pressing harder makes a note louder and adds a little vibrato.
3. Turn the **PRESET** encoder to try other sounds (16 factory sounds; the remaining 40 memories are empty "INIT").
4. Turn **K1..K4** to change what you hear: on HOME they are CUTOFF (brightness), RES (resonance), ATTACK and RELEASE.
5. Press **SAVE** to keep the sound. The SAVE light means something has changed and is not saved yet.

Stuck notes or a runaway sequence? Press **OCT- and OCT+ together**. That is the **panic** chord: it silences everything.

## 3. Finding your way around
The screen shows one *page* at a time. The four knobs **K1..K4** always edit the four values shown on the current page.

| Button | Page | What is on it |
|---|---|---|
| HOME | SOUND | the sound name, play mode, arp / hold / sequencer state and the 16-step pattern strip |
| EDIT | DCO | the oscillator: SAW, PULSE, SUB, NOISE (levels) |
| SEL | VCF | the filter: CUTOFF, RES, ENV, KEYS |
| ENV | ENVELOPE | ATTACK, DECAY, SUSTAIN, RELEASE |
| LFO | LFO | RATE, DELAY, TO DCO, TO VCF |
| FX | CHORUS | CHORUS, MODE, PORTA, SPREAD |
| GLO | GLOBAL | tuning, velocity, arpeggiator settings, tempo, theme, link |

**Second halves.** Several pages have more than four values. Press the page button again (or turn the **SELECT**
encoder) to flip to the second half. Pages with a second half: DCO, VCF and GLOBAL.

**Memories.** The **PRESET** encoder steps one memory at a time, the **ALGO** encoder jumps eight. There are 56.

**Octave.** **OCT-** / **OCT+** shift the keyboard down or up.

## 4. Every feature, page by page

### Oscillator (EDIT page)
Each voice has one oscillator (a *DCO*) with four sources you mix:
- **SAW:** a bright, buzzy wave. The classic JUNO sound.
- **PULSE:** a hollow square-like wave. **PW** sets how wide it is.
- **SUB:** a square one octave lower, for weight.
- **NOISE:** hiss, for breath and percussion.

Second half: **RANGE** picks the pitch register (16', 8' or 4': low, normal, high). **PW** is the pulse width.
**PWM** chooses whether the width is set by hand (MANUAL) or swept automatically by the LFO (LFO) for a moving sound.

### Filter (SEL page)
The filter removes high frequencies, so it controls brightness.
- **CUTOFF:** low is dark and muffled, high is bright.
- **RES** (resonance): emphasises the frequencies at the cutoff. High values whistle and the filter can self-oscillate.
- **ENV:** how much the envelope opens the filter on each note.
- **KEYS:** how much higher notes open the filter (key follow).

Second half: **HPF** is a high-pass filter with four positions (BOOST, FLAT, THIN, THINNER) for thinning out the bass.
**ENV** can be NORMAL or INVERT (the envelope closes the filter instead of opening it). **VCA** is ENV (volume follows
the envelope) or GATE (on while a key is held, off when released). **LEVEL** is the overall volume of the sound.

### Envelope (ENV page)
Shapes how a note evolves, shared by the filter and the volume:
- **ATTACK:** how long the note takes to fade in. Low is instant, high is a slow swell.
- **DECAY:** how long it takes to fall to the sustain level.
- **SUSTAIN:** the level held while the key stays down.
- **RELEASE:** how long the note takes to fade after you let go.

### LFO (LFO page)
A slow wobble you can apply to the sound:
- **RATE:** wobble speed. **DELAY:** how long after a note starts before it begins.
- **TO DCO:** pitch wobble (vibrato). **TO VCF:** brightness wobble (wah).
- Pressing a key harder also adds vibrato, like a mod wheel.

### Chorus, play mode and glide (FX page)
- **CHORUS:** OFF, I, II or I+II. Makes the sound wider and shimmery, as on the original JUNO.
- **MODE:** POLY (six notes at once), UNISON (all voices stack on one note), SOLO (one note at a time).
- **PORTA:** portamento, a slide between notes.
- **SPREAD:** how much the voices are detuned from each other in UNISON mode.

### Global (GLO page)
First half:
- **TUNE:** fine tuning, -50 to +50 cents.
- **VELOCITY:** how strongly the force of your touch affects loudness (higher is more expressive).
- **ARP:** arpeggiator direction: UP, DOWN or UP/DN.
- **OCTAVES:** arpeggiator range, 1 to 3 octaves.

Second half:
- **TEMPO:** speed of the arpeggiator and the sequencer.
- **THEME:** screen colours: GREEN, AMBER, CYAN, RED, MONO or JUNO (blue, the default). Saved with SAVE.
- **LINK:** see section 7.

## 5. Playing helpers
- **ARP:** hold some keys and press ARP. The notes you hold are played one after another. Press ARP again to stop.
  Direction, range and speed are set on the GLO page.
- **PLAY:** hold. Notes keep sounding after you release the keys. Press again to let go.
- **Octave buttons:** press OCT- or OCT+ to shift; both together is panic.

## 6. Recording a pattern (sequencer)
1. Press **REC**. The old pattern is cleared.
2. Play up to 16 notes with the keys. Press **ARP** to enter a rest (a silent step).
3. Press **REC** again to finish.
4. Press **SEQ** to play the pattern in a loop, and **SEQ** again to stop. If nothing is recorded the screen says
   "EMPTY: REC". Starting the sequencer turns the arpeggiator off.
5. **TEMPO** (GLO page, second half) sets the speed. **SAVE** keeps the pattern.

The HOME page shows the pattern as a row of bars: tall for high notes, short for rests, white for the current step.

## 7. LINK (beta): driving a second FM-1
LINK lets one FM-1 play another over a USB cable. Set **GLO, second half, K3 (LINK)** to **HOST**.
Whatever the arpeggiator and sequencer play (and the keys when the arp is off) is also sent to the other unit.

What you need:
- A **powered USB hub or powered adapter** between the two units. The FM-1 does not supply power on its port in host mode.
- The second FM-1 must be in normal mode (LINK off if it is also running J6).

Good to know:
- LINK always starts OFF after power-up, so installing firmware from a computer keeps working.
- While LINK is HOST there is no computer connection (no USB audio, no installer). Set LINK to OFF to return.
- A small "LINK" label on the HOME page is bright when connected, dim while searching.
- Messages: LOOKING, CONNECTED, NO MIDI, NEEDS POWER, LOST.

## 8. MIDI
Over USB from a computer or keyboard, J6 responds to notes, velocity, pitch bend, the mod wheel (CC1), sustain pedal
(CC64), channel pressure and program change (selects a memory).

## 9. Saving, and what is kept
**SAVE** stores the current sound, your settings (octave, tuning, theme and so on) and the sequencer pattern. J6 keeps only
its own data and never reads other firmwares' projects or samples.

## 10. Quick reference
| Button / control | Action |
|---|---|
| HOME / EDIT / SEL / ENV / LFO / FX / GLO | go to a page; press again for its second half |
| K1..K4 | change the four values on screen |
| SELECT encoder | flip a page's halves |
| PRESET / ALGO encoders | memory +/-1 / +/-8 |
| SAVE | save sound, settings and pattern |
| ARP | arpeggiator on/off (rest while recording) |
| PLAY | hold on/off |
| REC / SEQ | record / play the pattern |
| OCT- / OCT+ | octave; both together = panic |

## 11. Troubleshooting
- **No sound:** check the volume and that LEVEL (VCF page, second half) and CUTOFF are not at zero.
- **Notes will not stop:** press OCT- and OCT+ together.
- **A sound sounds different after a restart:** you did not press SAVE.
- **LINK says NEEDS POWER or NO MIDI:** use a powered hub; make sure the other unit is a normal USB device.
- **Cannot install from the computer:** set LINK to OFF (or power-cycle).
- **Going back to the original firmware:** install any other FM-1 firmware the same way; see the README.
