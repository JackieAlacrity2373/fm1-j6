// The J6 MIDI parameter table: one entry per control. docs/MIDI_CC.md documents the same table; tests/editor_test.py
// checks that the two (and src/juno.c) agree. A front end of your own only needs this list.
//   cc       MIDI control change number
//   key      the j_patch_t field, or the global it sets
//   type     knob (0..max), select (named values), toggle (0/1 as a switch)
//   min, max the range the firmware accepts (it clamps anything outside)
//   def      the INIT value
//   show     optional: how to display the raw value (signed tune)
const J6_SECTIONS = ["DCO", "VCF", "ENVELOPE", "LFO", "VOICE", "GLOBAL"];

const J6_PARAMS = [
  // DCO
  {cc: 21, key: "saw", label: "SAW", section: "DCO", type: "knob", min: 0, max: 127, def: 100},
  {cc: 22, key: "pulse", label: "PULSE", section: "DCO", type: "knob", min: 0, max: 127, def: 0},
  {cc: 23, key: "sub", label: "SUB", section: "DCO", type: "knob", min: 0, max: 127, def: 0},
  {cc: 24, key: "noise", label: "NOISE", section: "DCO", type: "knob", min: 0, max: 127, def: 0},
  {cc: 20, key: "range", label: "RANGE", section: "DCO", type: "select", min: 0, max: 2, def: 1, options: ["16'", "8'", "4'"]},
  {cc: 25, key: "pw", label: "PW", section: "DCO", type: "knob", min: 0, max: 127, def: 0},
  {cc: 26, key: "pwm_src", label: "PWM", section: "DCO", type: "select", min: 0, max: 1, def: 0, options: ["MANUAL", "LFO"]},
  // VCF
  {cc: 74, key: "cutoff", label: "CUTOFF", section: "VCF", type: "knob", min: 0, max: 127, def: 90},
  {cc: 71, key: "res", label: "RES", section: "VCF", type: "knob", min: 0, max: 127, def: 0},
  {cc: 27, key: "env_amt", label: "ENV", section: "VCF", type: "knob", min: 0, max: 127, def: 0},
  {cc: 28, key: "kbd", label: "KEYS", section: "VCF", type: "knob", min: 0, max: 127, def: 64},
  {cc: 31, key: "hpf", label: "HPF", section: "VCF", type: "select", min: 0, max: 3, def: 1, options: ["BOOST", "FLAT", "THIN", "THINNER"]},
  {cc: 29, key: "env_pol", label: "ENV POL", section: "VCF", type: "select", min: 0, max: 1, def: 0, options: ["NORMAL", "INVERT"]},
  {cc: 30, key: "vca_mode", label: "VCA", section: "VCF", type: "select", min: 0, max: 1, def: 0, options: ["ENV", "GATE"]},
  {cc: 7, key: "level", label: "LEVEL", section: "VCF", type: "knob", min: 0, max: 127, def: 100},
  // ENVELOPE
  {cc: 73, key: "a", label: "ATTACK", section: "ENVELOPE", type: "knob", min: 0, max: 127, def: 0},
  {cc: 75, key: "d", label: "DECAY", section: "ENVELOPE", type: "knob", min: 0, max: 127, def: 40},
  {cc: 70, key: "s", label: "SUSTAIN", section: "ENVELOPE", type: "knob", min: 0, max: 127, def: 127},
  {cc: 72, key: "r", label: "RELEASE", section: "ENVELOPE", type: "knob", min: 0, max: 127, def: 20},
  // LFO
  {cc: 76, key: "lfo_rate", label: "RATE", section: "LFO", type: "knob", min: 0, max: 127, def: 40},
  {cc: 77, key: "lfo_delay", label: "DELAY", section: "LFO", type: "knob", min: 0, max: 127, def: 0},
  {cc: 78, key: "lfo_dco", label: "TO DCO", section: "LFO", type: "knob", min: 0, max: 127, def: 0},
  {cc: 79, key: "lfo_vcf", label: "TO VCF", section: "LFO", type: "knob", min: 0, max: 127, def: 0},
  // VOICE
  {cc: 93, key: "chorus", label: "CHORUS", section: "VOICE", type: "select", min: 0, max: 3, def: 0, options: ["OFF", "I", "II", "I+II"]},
  {cc: 80, key: "mode", label: "MODE", section: "VOICE", type: "select", min: 0, max: 2, def: 0, options: ["POLY", "UNISON", "SOLO"]},
  {cc: 5, key: "porta", label: "PORTA", section: "VOICE", type: "knob", min: 0, max: 127, def: 0},
  {cc: 85, key: "detune", label: "SPREAD", section: "VOICE", type: "knob", min: 0, max: 127, def: 0},
  {cc: 86, key: "bend", label: "BEND RANGE", section: "VOICE", type: "knob", min: 0, max: 12, def: 2},
  // GLOBAL (not part of a sound: the device keeps these with its settings)
  {cc: 102, key: "tune", label: "TUNE", section: "GLOBAL", type: "knob", min: 0, max: 100, def: 50, global: true,
   show: v => (v - 50) + " c"},
  {cc: 103, key: "vel", label: "VELOCITY", section: "GLOBAL", type: "knob", min: 0, max: 127, def: 80, global: true},
  {cc: 104, key: "arp_mode", label: "ARP", section: "GLOBAL", type: "select", min: 0, max: 2, def: 0, global: true, options: ["UP", "DOWN", "UP/DN"]},
  {cc: 105, key: "arp_range", label: "OCTAVES", section: "GLOBAL", type: "knob", min: 1, max: 3, def: 1, global: true},
  {cc: 106, key: "arp_rate", label: "TEMPO", section: "GLOBAL", type: "knob", min: 0, max: 127, def: 80, global: true},
];

// The fields of one sound, in the order of J6_INIT / J6_FACTORY (the j_patch_t order after the name)
const J6_FIELDS = ["saw", "pulse", "sub", "noise", "range", "pw", "pwm_src", "lfo_dco", "lfo_rate", "lfo_delay",
  "lfo_vcf", "hpf", "cutoff", "res", "env_amt", "kbd", "vca_mode", "env_pol", "a", "d", "s", "r", "chorus", "level",
  "mode", "porta", "detune", "bend"];

// Switches: a value of 64 or more turns it on. The device keeps the state; there is no read-back.
const J6_SWITCHES = [
  {cc: 107, key: "arp_on", label: "ARP ON"},
  {cc: 108, key: "seq_play", label: "SEQ PLAY"},
  {cc: 109, key: "seq_rec", label: "SEQ REC"},
  {cc: 110, key: "hold", label: "HOLD"},
  {cc: 64, key: "sustain", label: "SUSTAIN"},
];

const J6_PROGRAMS = 56;

// The screen themes, in the order of the firmware's palettes (fm1-base/device/gfx.c). CC 111 selects one on the FM-1;
// the editor uses the same colours: value = knob arcs, headings and values; bright = the brightest text.
const J6_THEME_CC = 111;
const J6_THEMES = [
  {name: "GREEN", value: "rgb(80,220,120)", bright: "rgb(150,255,175)"},
  {name: "AMBER", value: "rgb(232,136,30)", bright: "rgb(255,184,70)"},
  {name: "CYAN", value: "rgb(60,180,235)", bright: "rgb(150,225,255)"},
  {name: "RED", value: "rgb(232,76,60)", bright: "rgb(255,132,112)"},
  {name: "MONO", value: "rgb(196,196,202)", bright: "rgb(236,236,240)"},
  {name: "JUNO", value: "rgb(40,160,225)", bright: "rgb(200,236,255)"},
];
const J6_THEME_DEFAULT = 5;
