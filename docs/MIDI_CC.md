# J6 — MIDI CC Implementation Chart

This document lists every MIDI message **J6** responds to over USB-MIDI on the M-VAVE FM-1. The control layout follows the
J6 panel (DCO, VCF/HPF, envelope, LFO, voice, global). Where a standard MIDI controller already fits, it is used:
CC 1 mod wheel, 5 portamento time, 7 volume, 64 sustain, 70–75 sound controllers (cutoff, resonance, ADSR), 93 chorus.

The browser editor in [`editor/`](../editor/) is a working front end; its table, [`editor/params.js`](../editor/params.js),
holds this same list in a form you can reuse.

- **Direction:** the FM-1 only **receives**. It does not send its state back, so a front end keeps its own copy of the values.
- **Channel:** any (omni). Use channel 1 for simplicity.
- **Values:** data bytes are 0–127. A value above a control's maximum is **clamped**, never wrapped.
- **No SysEx.** There is no patch dump; a sound is set by sending its CCs (section 8).
- **Engine:** 6 voices, one DCO per voice, 24 dB low-pass, HPF, one ADSR, LFO, stereo chorus, arpeggiator, 16-step sequencer.

---

## 1. Notes & Performance

| Message | Range | Description |
|:---|:---:|:---|
| Note On / Off (`9n` / `8n`) | 0–127 | Play. Velocity 0 on `9n` is a note off. Velocity sensitivity is CC 103. |
| Pitch Bend (`En`) | 14 bit | Range set by CC 86 (0–12 semitones). |
| Channel Pressure (`Dn`) | 0–127 | Aftertouch. |
| Program Change (`Cn`) | 0–55 | Selects memory 1–56 and loads its saved sound over the live one. Higher values wrap (mod 56). |

| CC # | Parameter | Range | Values / Description |
|:---:|:---|:---:|:---|
| **1** | Modulation Wheel | `0–127` | Moves the LFO depth |
| **64** | Sustain Pedal | `0–127` | ≥ 64 holds notes |
| **120** | All Sound Off | `0` | Silences every voice at once |
| **121** | Reset Controllers | `0` | Resets mod wheel, pressure, pitch bend and sustain |
| **123** | All Notes Off | `0` | Releases all held notes |

---

## 2. DCO (Oscillator)

| CC # | Parameter | Range | Values / Description |
|:---:|:---|:---:|:---|
| **20** | Range | `0–2` | `0 = 16'`, `1 = 8'`, `2 = 4'` |
| **21** | Saw Level | `0–127` | Level of the sawtooth in the mix |
| **22** | Pulse Level | `0–127` | Level of the pulse / square in the mix |
| **23** | Sub Level | `0–127` | Level of the sub oscillator |
| **24** | Noise Level | `0–127` | Level of the noise generator |
| **25** | Pulse Width | `0–127` | Manual pulse width, or the depth of its LFO modulation when PWM is LFO |
| **26** | PWM Source | `0–1` | `0 = MANUAL`, `1 = LFO` |

---

## 3. VCF, HPF & VCA

| CC # | Parameter | Range | Values / Description |
|:---:|:---|:---:|:---|
| **74** | Cutoff | `0–127` | 24 dB/oct low-pass cutoff |
| **71** | Resonance | `0–127` | Resonance, up to self-oscillation |
| **27** | Envelope Amount | `0–127` | How far the envelope moves the cutoff |
| **28** | Keyboard Tracking | `0–127` | Cutoff follows the note pitch (0–100 %) |
| **29** | Envelope Polarity | `0–1` | `0 = NORMAL`, `1 = INVERT` (filter envelope) |
| **30** | VCA Mode | `0–1` | `0 = ENV` (shaped by the ADSR), `1 = GATE` (organ style) |
| **31** | HPF | `0–3` | `0 = BOOST` (bass boost), `1 = FLAT`, `2 = THIN`, `3 = THINNER` |
| **7** | Level | `0–127` | Output level of the sound |

---

## 4. Envelope (ADSR)

One ADSR shared by the VCF and the VCA.

| CC # | Parameter | Range | Values / Description |
|:---:|:---|:---:|:---|
| **73** | Attack | `0–127` | Attack time |
| **75** | Decay | `0–127` | Decay time |
| **70** | Sustain | `0–127` | Sustain level |
| **72** | Release | `0–127` | Release time |

---

## 5. LFO

| CC # | Parameter | Range | Values / Description |
|:---:|:---|:---:|:---|
| **76** | Rate | `0–127` | LFO speed |
| **77** | Delay | `0–127` | Fade-in time after a note starts |
| **78** | To DCO | `0–127` | Vibrato depth (pitch) |
| **79** | To VCF | `0–127` | Filter modulation depth (cutoff) |

---

## 6. Voice, Chorus & Glide

| CC # | Parameter | Range | Values / Description |
|:---:|:---|:---:|:---|
| **80** | Voice Mode | `0–2` | `0 = POLY`, `1 = UNISON`, `2 = SOLO` |
| **5** | Portamento Time | `0–127` | Glide time |
| **85** | Unison Spread | `0–127` | Detune between the stacked voices in UNISON |
| **86** | Bend Range | `0–12` | Pitch bend range in semitones |
| **93** | Chorus | `0–3` | `0 = OFF`, `1 = I`, `2 = II`, `3 = I+II` |

---

## 7. Global Settings & Switches

Global values are not part of a sound; the FM-1 keeps them with its settings (SAVE), not in a memory.
A switch is **on at 64 or more** and off below 64. The FM-1 does not report a switch's state.

| CC # | Parameter | Range | Values / Description |
|:---:|:---|:---:|:---|
| **102** | Tune | `0–100` | `50` = centre; (value − 50) cents |
| **103** | Velocity Sensitivity | `0–127` | How much velocity changes the level |
| **104** | Arpeggiator Direction | `0–2` | `0 = UP`, `1 = DOWN`, `2 = UP/DN` |
| **105** | Arpeggiator Octaves | `1–3` | A value of 0 is read as 1 |
| **106** | Tempo | `0–127` | Arpeggiator and sequencer rate |
| **107** | Arpeggiator Switch | on/off | Ignored while the sequencer plays |
| **108** | Sequencer Play | on/off | Plays the recorded pattern in a loop |
| **109** | Sequencer Record | on/off | Then play keys to record up to 16 steps |
| **110** | Hold Switch | on/off | Latches the held notes |
| **111** | Screen Theme | `0–5` | `0 = GREEN`, `1 = AMBER`, `2 = CYAN`, `3 = RED`, `4 = MONO`, `5 = JUNO` (blue). Recolours the FM-1 screen and is kept with SAVE; the web editor uses the same colours |

---

## 8. Setting a Whole Sound

There is no dump message. To load a sound, send one CC for every entry in sections 2–6 (and 7 if you want the globals),
about 2 ms apart; a long burst with no gaps can overflow the FM-1's input. The editor's **Send all** does exactly this.
Changing a CC changes the **live sound** only; press **SAVE** on the FM-1 to keep it in a memory. A Program Change
replaces the live sound with the saved one, so send it *before* editing.

The 56 factory sounds are in [`editor/patches.js`](../editor/patches.js), generated from [`src/juno.c`](../src/juno.c)
by `python tools/gen_patches.py`, as arrays in the field order of `J6_FIELDS` in `editor/params.js`.

## 9. CC Index

1 mod wheel · 5 portamento · 7 level · 20 range · 21 saw · 22 pulse · 23 sub · 24 noise · 25 pulse width · 26 PWM source ·
27 envelope amount · 28 key tracking · 29 envelope polarity · 30 VCA mode · 31 HPF · 64 sustain · 70 sustain level ·
71 resonance · 72 release · 73 attack · 74 cutoff · 75 decay · 76 LFO rate · 77 LFO delay · 78 LFO to DCO ·
79 LFO to VCF · 80 voice mode · 85 unison spread · 86 bend range · 93 chorus · 102 tune · 103 velocity ·
104 arp direction · 105 arp octaves · 106 tempo · 107 arp · 108 seq play · 109 seq record · 110 hold ·
111 theme · 120 sound off · 121 reset · 123 notes off. Anything else is ignored.

## 10. Using the Editor

The editor is a static page; the FM-1 does not host it. Serve the folder and open it in Chrome, Edge or Opera
(Web MIDI needs a secure context; `localhost` counts):

```
python -m http.server 8080 -d editor
```

Open <http://localhost:8080/>, press **Connect MIDI** and choose the FM-1 output.
