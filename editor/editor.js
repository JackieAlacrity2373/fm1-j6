// J6 web editor: talks to the FM-1 only through Web MIDI (USB). Nothing here runs on the device.
"use strict";

const $ = (s, root = document) => root.querySelector(s);
const el = (tag, cls, text) => {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
};

const state = {
  out: null,            // MIDIOutput
  access: null,
  channel: 0,           // 0..15
  values: {},           // key -> current value
  controls: {},         // key -> {set(v)} UI handle
  octave: 4,
  velocity: 100,
};
for (const p of J6_PARAMS) state.values[p.key] = p.def;
const param = key => J6_PARAMS.find(p => p.key === key);

// ------------------------------------------------------------------ MIDI out ---
const monitor = [];
function log(bytes, note) {
  const hex = Array.from(bytes, b => b.toString(16).padStart(2, "0")).join(" ");
  monitor.unshift(hex + (note ? "   " + note : ""));
  if (monitor.length > 14) monitor.pop();
  $("#monitor").textContent = monitor.join("\n");
}

function sendRaw(bytes, note, when) {
  log(bytes, note);
  if (!state.out) return;
  try {
    if (when) state.out.send(bytes, when); else state.out.send(bytes);
  } catch (e) { setStatus("Send failed: " + e.message, true); }
}

const sendCC = (cc, v, when) => sendRaw([0xB0 | state.channel, cc, v & 0x7F], "CC " + cc + " = " + v, when);
const sendNoteOn = n => sendRaw([0x90 | state.channel, n, state.velocity], "note on " + n);
const sendNoteOff = n => sendRaw([0x80 | state.channel, n, 0], "note off " + n);

// Coalesce knob drags to one CC per control per animation frame
const pending = new Map();
let flushing = false;
function queueCC(cc, v) {
  pending.set(cc, v);
  if (flushing) return;
  flushing = true;
  requestAnimationFrame(() => {
    flushing = false;
    for (const [c, val] of pending) sendCC(c, val);
    pending.clear();
  });
}

// A burst of CCs is spaced 2 ms apart so the device's input queue never fills
function sendAll() {
  const t0 = performance.now();
  J6_PARAMS.forEach((p, i) => sendCC(p.cc, state.values[p.key], t0 + i * 2));
}

// ------------------------------------------------------------------- values ---
function setValue(p, v, opts = {}) {
  v = Math.max(p.min, Math.min(p.max, Math.round(v)));
  const changed = state.values[p.key] !== v;
  state.values[p.key] = v;
  state.controls[p.key].set(v);
  if (opts.send !== false && (changed || opts.force)) queueCC(p.cc, v);
}

// Loading a sound sends it spaced out, like sendAll, rather than as one frame's burst
function loadSound(vals) {
  J6_FIELDS.forEach((k, i) => setValue(param(k), vals[i], {send: false}));
  const t0 = performance.now();
  J6_FIELDS.forEach((k, i) => sendCC(param(k).cc, state.values[k], t0 + i * 2));
}

const currentSound = () => J6_FIELDS.map(k => state.values[k]);

// -------------------------------------------------------------------- knobs ---
const ARC_START = 135, ARC_SWEEP = 270;
function polar(cx, cy, r, deg) {
  const a = (deg - 90) * Math.PI / 180;
  return [cx + r * Math.cos(a), cy + r * Math.sin(a)];
}
function arc(cx, cy, r, from, to) {
  const [x1, y1] = polar(cx, cy, r, from), [x2, y2] = polar(cx, cy, r, to);
  return `M${x1.toFixed(2)} ${y1.toFixed(2)} A${r} ${r} 0 ${to - from > 180 ? 1 : 0} 1 ${x2.toFixed(2)} ${y2.toFixed(2)}`;
}

function makeKnob(p) {
  const wrap = el("div", "ctl knob");
  wrap.tabIndex = 0;
  wrap.setAttribute("role", "slider");
  wrap.setAttribute("aria-label", p.label);
  wrap.setAttribute("aria-valuemin", p.min);
  wrap.setAttribute("aria-valuemax", p.max);
  wrap.title = "CC " + p.cc + " (drag, wheel or arrow keys; double-click resets; Shift = fine)";
  const NS = "http://www.w3.org/2000/svg";
  const svg = document.createElementNS(NS, "svg");
  svg.setAttribute("viewBox", "0 0 64 64");
  const mk = (tag, attrs) => {
    const n = document.createElementNS(NS, tag);
    for (const k in attrs) n.setAttribute(k, attrs[k]);
    return n;
  };
  const track = mk("path", {class: "track", d: arc(32, 32, 26, ARC_START, ARC_START + ARC_SWEEP)});
  const fill = mk("path", {class: "fill"});
  const cap = mk("circle", {cx: 32, cy: 32, r: 19, class: "cap"});
  const tick = mk("line", {class: "tick"});
  svg.append(track, fill, cap, tick);
  const val = el("div", "val");
  wrap.append(svg, el("div", "name", p.label), val);

  const span = p.max - p.min;
  function set(v) {
    const t = (v - p.min) / span, ang = ARC_START + ARC_SWEEP * t;
    fill.setAttribute("d", t > 0 ? arc(32, 32, 26, ARC_START, ang) : "");
    const [x1, y1] = polar(32, 32, 8, ang), [x2, y2] = polar(32, 32, 19, ang);
    tick.setAttribute("x1", x1); tick.setAttribute("y1", y1); tick.setAttribute("x2", x2); tick.setAttribute("y2", y2);
    val.textContent = p.show ? p.show(v) : v;
    wrap.setAttribute("aria-valuenow", v);
  }

  let drag = null;
  wrap.addEventListener("pointerdown", e => {
    wrap.setPointerCapture(e.pointerId);
    drag = {y: e.clientY, v: state.values[p.key]};
    wrap.focus();
    e.preventDefault();
  });
  wrap.addEventListener("pointermove", e => {
    if (!drag) return;
    const pixels = e.shiftKey ? 600 : 150;
    setValue(p, drag.v + (drag.y - e.clientY) * span / pixels);
  });
  const end = () => { drag = null; };
  wrap.addEventListener("pointerup", end);
  wrap.addEventListener("pointercancel", end);
  wrap.addEventListener("dblclick", () => setValue(p, p.def));
  wrap.addEventListener("wheel", e => {
    e.preventDefault();
    const step = Math.max(1, span / (e.shiftKey ? 127 : 32));
    setValue(p, state.values[p.key] + (e.deltaY < 0 ? step : -step));
  }, {passive: false});
  wrap.addEventListener("keydown", e => {
    const big = Math.max(1, Math.round(span / 10)), v = state.values[p.key];
    const d = {ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1, PageUp: big, PageDown: -big}[e.key];
    if (d) { setValue(p, v + d); e.preventDefault(); }
    else if (e.key === "Home") setValue(p, p.min);
    else if (e.key === "End") setValue(p, p.max);
  });
  state.controls[p.key] = {set};
  set(state.values[p.key]);
  return wrap;
}

function makeSelect(p) {
  const wrap = el("div", "ctl select");
  wrap.setAttribute("role", "radiogroup");
  wrap.setAttribute("aria-label", p.label);
  wrap.title = "CC " + p.cc;
  wrap.append(el("div", "name", p.label));
  const row = el("div", "seg");
  const buttons = p.options.map((o, i) => {
    const b = el("button", "", o);
    b.type = "button";
    b.setAttribute("role", "radio");
    b.addEventListener("click", () => setValue(p, i, {force: true}));
    row.append(b);
    return b;
  });
  wrap.append(row);
  state.controls[p.key] = {
    set: v => buttons.forEach((b, i) => { b.classList.toggle("on", i === v); b.setAttribute("aria-checked", i === v); }),
  };
  state.controls[p.key].set(state.values[p.key]);
  return wrap;
}

function buildPanel() {
  const host = $("#panel");
  for (const name of J6_SECTIONS) {
    const sec = el("section", "section");
    sec.append(el("h2", "", name));
    const body = el("div", "body");
    for (const p of J6_PARAMS.filter(x => x.section === name))
      body.append(p.type === "select" ? makeSelect(p) : makeKnob(p));
    sec.append(body);
    host.append(sec);
  }
}

// -------------------------------------------------------------- performance ---
function buildPerformance() {
  const host = $("#perf");
  const sw = el("div", "switches");
  for (const s of J6_SWITCHES) {
    const b = el("button", "toggle", s.label);
    b.type = "button";
    b.title = "CC " + s.cc;
    b.setAttribute("aria-pressed", "false");
    b.addEventListener("click", () => {
      const on = b.getAttribute("aria-pressed") !== "true";
      b.setAttribute("aria-pressed", on);
      b.classList.toggle("on", on);
      sendCC(s.cc, on ? 127 : 0);
    });
    sw.append(b);
  }
  host.append(sw);

  const slider = (label, min, max, val, onInput, spring) => {
    const w = el("label", "slider");
    w.append(el("span", "", label));
    const i = el("input");
    i.type = "range"; i.min = min; i.max = max; i.value = val;
    i.addEventListener("input", () => onInput(Number(i.value)));
    if (spring) {
      const home = () => { if (Number(i.value) !== val) { i.value = val; onInput(val); } };
      i.addEventListener("pointerup", home);
      i.addEventListener("pointercancel", home);
      i.addEventListener("blur", home);
    }
    w.append(i);
    return w;
  };
  host.append(
    slider("PITCH BEND", 0, 16383, 8192, v => sendRaw([0xE0 | state.channel, v & 0x7F, v >> 7], "bend " + (v - 8192)), true),
    slider("MOD WHEEL", 0, 127, 0, v => queueCC(1, v)),
    slider("PRESSURE", 0, 127, 0, v => sendRaw([0xD0 | state.channel, v], "pressure " + v)),
  );
}

// ----------------------------------------------------------------- keyboard ---
const KEYMAP = "awsedftgyhujkolp;".split("");
const held = new Map();   // key id -> note
function noteOn(id, n) {
  if (held.has(id)) return;
  held.set(id, n);
  sendNoteOn(n);
  const k = $(`[data-note="${n}"]`);
  if (k) k.classList.add("down");
}
function noteOff(id) {
  if (!held.has(id)) return;
  const n = held.get(id);
  held.delete(id);
  sendNoteOff(n);
  const k = $(`[data-note="${n}"]`);
  if (k) k.classList.remove("down");
}
function allNotesOff() {
  for (const id of Array.from(held.keys())) noteOff(id);
}

function buildKeyboard() {
  const host = $("#keys");
  host.innerHTML = "";
  const base = state.octave * 12 + 12;       // C of the chosen octave (C4 = 60)
  const black = [1, 3, 6, 8, 10];
  let whites = 0;
  for (let i = 0; i < 25; i++) {
    const n = base + i, isBlack = black.includes(i % 12);
    if (n > 127) break;
    const k = el("div", "key " + (isBlack ? "black" : "white"));
    k.dataset.note = n;
    if (isBlack) k.style.left = `calc(${whites} * var(--w) - var(--w) * 0.3)`;
    else whites++;
    k.addEventListener("pointerdown", e => { k.setPointerCapture(e.pointerId); noteOn("p" + e.pointerId, n); });
    k.addEventListener("pointerup", e => noteOff("p" + e.pointerId));
    k.addEventListener("pointercancel", e => noteOff("p" + e.pointerId));
    host.append(k);
  }
  host.style.setProperty("--whites", whites);
  $("#octave").textContent = "C" + state.octave;
}

function isTyping(e) {
  const t = e.target;
  return t && ((t.tagName === "INPUT" && t.type !== "range") || t.tagName === "TEXTAREA" || t.tagName === "SELECT");
}
window.addEventListener("keydown", e => {
  if (e.repeat || e.ctrlKey || e.metaKey || e.altKey || isTyping(e)) return;
  const i = KEYMAP.indexOf(e.key.toLowerCase());
  const n = state.octave * 12 + 12 + i;
  if (i >= 0 && n <= 127) noteOn("k" + e.key.toLowerCase(), n);
});
window.addEventListener("keyup", e => noteOff("k" + e.key.toLowerCase()));
window.addEventListener("blur", allNotesOff);

// ------------------------------------------------------------------ patches ---
const LIB_KEY = "j6-editor-library";
const library = () => { try { return JSON.parse(localStorage.getItem(LIB_KEY)) || []; } catch (e) { return []; } };
const saveLibrary = l => localStorage.setItem(LIB_KEY, JSON.stringify(l));
const soundName = () => ($("#patch-name").value.trim() || "UNTITLED").slice(0, 8).toUpperCase();

function buildFactory() {
  const host = $("#factory");
  J6_FACTORY.forEach((f, i) => {
    const b = el("button", "patch");
    b.type = "button";
    const n = el("small", "", String(i + 1));
    b.append(n, f.name.trim());
    b.title = "Load into the editor and send to the FM-1";
    b.addEventListener("click", () => {
      loadSound(f.v);
      $("#patch-name").value = f.name.trim();
      document.querySelectorAll("#factory .patch").forEach(x => x.classList.remove("on"));
      b.classList.add("on");
    });
    host.append(b);
  });
}

function renderLibrary() {
  const host = $("#library");
  host.innerHTML = "";
  const lib = library();
  if (!lib.length) host.append(el("p", "hint", "No saved sounds yet."));
  lib.forEach((s, i) => {
    const row = el("div", "lib-row");
    const load = el("button", "", s.name);
    load.type = "button";
    load.addEventListener("click", () => { loadSound(s.v); $("#patch-name").value = s.name; });
    const del = el("button", "del", "×");
    del.type = "button";
    del.title = "Delete";
    del.addEventListener("click", () => { const l = library(); l.splice(i, 1); saveLibrary(l); renderLibrary(); });
    row.append(load, del);
    host.append(row);
  });
}

function download(name, text) {
  const a = el("a");
  a.href = URL.createObjectURL(new Blob([text], {type: "application/json"}));
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

function validSound(v) {
  return Array.isArray(v) && v.length === J6_FIELDS.length && v.every((x, i) => {
    const p = param(J6_FIELDS[i]);
    return Number.isInteger(x) && x >= p.min && x <= p.max;
  });
}

function wirePatchTools() {
  $("#save-local").addEventListener("click", () => {
    const name = soundName();
    const l = library().filter(s => s.name !== name);
    l.push({name, v: currentSound()});
    saveLibrary(l);
    renderLibrary();
  });
  $("#export").addEventListener("click", () => {
    const name = soundName();
    download(name + ".j6.json", JSON.stringify({j6: 1, name, fields: J6_FIELDS, v: currentSound()}, null, 1));
  });
  $("#import").addEventListener("change", async e => {
    const f = e.target.files[0];
    e.target.value = "";
    if (!f) return;
    try {
      const d = JSON.parse(await f.text());
      if (!validSound(d.v)) throw new Error("not a J6 sound");
      loadSound(d.v);
      $("#patch-name").value = String(d.name || "").slice(0, 8);
    } catch (err) { setStatus("Import failed: " + err.message, true); }
  });
  $("#init").addEventListener("click", () => loadSound(J6_INIT));
  $("#random").addEventListener("click", () => {
    // Keep what makes a sound audible and playable: level, bend range, voice mode, portamento, VCA
    const keep = new Set(["level", "bend", "mode", "porta", "vca_mode"]);
    const v = J6_FIELDS.map(k => {
      const p = param(k);
      return keep.has(k) ? state.values[k] : p.min + Math.floor(Math.random() * (p.max - p.min + 1));
    });
    if ([0, 1, 2, 3].every(i => v[i] < 20)) v[0] = 100;
    if (v[J6_FIELDS.indexOf("cutoff")] < 30) v[J6_FIELDS.indexOf("cutoff")] = 60;
    loadSound(v);
  });
  $("#send-all").addEventListener("click", sendAll);
  $("#panic").addEventListener("click", () => {
    allNotesOff();
    sendCC(123, 0);
    sendCC(120, 0);
  });
  const pc = $("#program");
  const sendPC = () => sendRaw([0xC0 | state.channel, (Number(pc.value) - 1) % J6_PROGRAMS], "program " + pc.value);
  $("#pc-send").addEventListener("click", sendPC);
  $("#pc-prev").addEventListener("click", () => { pc.value = (Number(pc.value) + J6_PROGRAMS - 2) % J6_PROGRAMS + 1; sendPC(); });
  $("#pc-next").addEventListener("click", () => { pc.value = Number(pc.value) % J6_PROGRAMS + 1; sendPC(); });
}

// --------------------------------------------------------------------- MIDI ---
function setStatus(msg, bad) {
  const s = $("#status");
  s.textContent = msg;
  s.classList.toggle("bad", !!bad);
}

function fillOutputs() {
  const sel = $("#output");
  const prev = state.out && state.out.id;
  sel.innerHTML = "";
  sel.append(new Option("(no output: monitor only)", ""));
  const outs = state.access ? Array.from(state.access.outputs.values()) : [];
  for (const o of outs) sel.append(new Option(o.name, o.id));
  const guess = outs.find(o => o.id === prev) || outs.find(o => /fm-?1|m-?vave|j6/i.test(o.name));
  if (guess) sel.value = guess.id;
  selectOutput();
}

function selectOutput() {
  const id = $("#output").value;
  state.out = id && state.access ? state.access.outputs.get(id) : null;
  if (state.out) {
    setStatus("Connected: " + state.out.name + " (the FM-1 keeps its sound until you move a control or press Send all)");
  } else {
    setStatus(state.access ? "No output selected: messages only show in the monitor." : "MIDI not connected.");
  }
}

async function connect() {
  if (!navigator.requestMIDIAccess) {
    setStatus("This browser has no Web MIDI. Use Chrome, Edge or Opera.", true);
    return;
  }
  try {
    state.access = await navigator.requestMIDIAccess({sysex: false});
    state.access.onstatechange = fillOutputs;
    fillOutputs();
    $("#connect").textContent = "Reconnect";
  } catch (e) {
    setStatus("MIDI access was refused: " + e.message, true);
  }
}

// ------------------------------------------------------------------- theme ---
const THEME_KEY = "j6-editor-theme";
function applyTheme(i) {
  const t = J6_THEMES[i];
  const root = document.documentElement.style;
  root.setProperty("--accent", t.value);
  root.setProperty("--bright", t.bright);
  $("#theme").value = i;
}
function setTheme(i, send) {
  applyTheme(i);
  localStorage.setItem(THEME_KEY, i);
  if (send) sendCC(J6_THEME_CC, i);
}
function buildTheme() {
  const sel = $("#theme");
  J6_THEMES.forEach((t, i) => sel.append(new Option(t.name, i)));
  const raw = localStorage.getItem(THEME_KEY);
  const saved = raw === null ? NaN : Number(raw);
  applyTheme(Number.isInteger(saved) && saved >= 0 && saved < J6_THEMES.length ? saved : J6_THEME_DEFAULT);
  sel.addEventListener("change", () => setTheme(Number(sel.value), true));
}

// --------------------------------------------------------------------- boot ---
function boot() {
  const ch = $("#channel");
  for (let i = 1; i <= 16; i++) ch.append(new Option(i, i));
  buildTheme();
  buildPanel();
  buildPerformance();
  buildFactory();
  renderLibrary();
  wirePatchTools();
  buildKeyboard();
  $("#connect").addEventListener("click", connect);
  $("#output").addEventListener("change", selectOutput);
  ch.addEventListener("change", e => { allNotesOff(); state.channel = Number(e.target.value) - 1; });
  $("#oct-down").addEventListener("click", () => { allNotesOff(); state.octave = Math.max(0, state.octave - 1); buildKeyboard(); });
  $("#oct-up").addEventListener("click", () => { allNotesOff(); state.octave = Math.min(8, state.octave + 1); buildKeyboard(); });
  $("#velocity").addEventListener("input", e => { state.velocity = Number(e.target.value); });
  if (!navigator.requestMIDIAccess) setStatus("This browser has no Web MIDI. Use Chrome, Edge or Opera.", true);
}
boot();
