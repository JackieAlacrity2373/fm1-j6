/* SPDX-License-Identifier: MPL-2.0 */
/* J6: the UI and the platform glue around the JUNO-60 style engine (juno.c). Part of the unity build
 * (device/unity.c, host/plat_host.c), after the platform: gfx.c and announce.c are in scope.
 *
 * Panel: EDIT = DCO, SEL = VCF / HPF, ENV, LFO, FX = chorus / mode / portamento, GLO = tune / arp / theme, HOME.
 * K1..K4 edit the four values shown; SELECT (or pressing the page's button again) flips its second half;
 * PRESET steps the memory, ALGO by eight; SAVE stores the sound in its memory; ARP, PLAY (hold); REC records a
 * 16 step pattern (ARP adds a rest), SEQ plays it; OCT- and OCT+ together are the panic. */
#include <stddef.h>
#include "fastmath.h"
#include "juno.h"

#define SET_MAGIC 0x4A363031u            /* "J601" */
#define BANK_MAGIC 0x4A363042u           /* "J60B" */

typedef struct {
    uint32_t magic;
    int8_t octave;                       /* -2 .. +2 */
    uint8_t patch;                       /* the memory in use */
    int8_t tune;
    uint8_t vel, arp_mode, arp_range, arp_rate;
    uint8_t theme;                       /* the screen's palette */
    uint8_t seq_n;                       /* the recorded steps */
    uint8_t seq[J_SEQ_MAX], seq_vel[J_SEQ_MAX];
} settings_t;
static settings_t set;
static uint8_t set_dirty;                /* the settings changed since the last save */

static struct {
    uint32_t magic;
    j_patch_t p[J_NPATCH];
} bank;
static uint8_t patch_dirty;              /* the live sound differs from its memory */

/* ------------------------------------------------------------- parameters --- */
typedef struct {
    const char *name;                    /* NULL: nothing here */
    uint8_t base;                        /* 0 j_p, 1 j_g (unsigned), 2 j_g (signed) */
    uint8_t off;
    int8_t min, max;
    const char *const *lab;              /* names for the values, or NULL: the number */
} prm_t;

static const char *const L_RANGE[] = {"16'", "8'", "4'"};
static const char *const L_SRC[] = {"MANUAL", "LFO"};
static const char *const L_HPF[] = {"BOOST", "FLAT", "THIN", "THINNER"};
static const char *const L_POL[] = {"NORMAL", "INVERT"};
static const char *const L_VCA[] = {"ENV", "GATE"};
static const char *const L_CHO[] = {"OFF", "I", "II", "I+II"};
static const char *const L_MODE[] = {"POLY", "UNISON", "SOLO"};
static const char *const L_ARP[] = {"UP", "DOWN", "UP/DN"};
static const char *const L_THEME[] = {"GREEN", "AMBER", "CYAN", "RED", "MONO", "JUNO"};

#define P(n, f, mx) {n, 0, (uint8_t)offsetof(j_patch_t, f), 0, mx, 0}
#define PL(n, f, mx, l) {n, 0, (uint8_t)offsetof(j_patch_t, f), 0, mx, l}

typedef struct {
    const char *title;
    uint8_t nsub;
    prm_t p[2][4];
} page_t;

enum { PG_HOME, PG_DCO, PG_VCF, PG_ENV, PG_LFO, PG_FX, PG_GLO, NPAGE };
static const page_t PAGE[NPAGE] = {
    {"SOUND", 1, {{P("CUTOFF", cutoff, 127), P("RES", res, 127), P("ATTACK", a, 127), P("RELEASE", r, 127)}}},
    {"DCO", 2, {{P("SAW", saw, 127), P("PULSE", pulse, 127), P("SUB", sub, 127), P("NOISE", noise, 127)},
                {PL("RANGE", range, 2, L_RANGE), P("PW", pw, 127), PL("PWM", pwm_src, 1, L_SRC), {0}}}},
    {"VCF", 2, {{P("CUTOFF", cutoff, 127), P("RES", res, 127), P("ENV", env_amt, 127), P("KEYS", kbd, 127)},
                {PL("HPF", hpf, 3, L_HPF), PL("ENV", env_pol, 1, L_POL), PL("VCA", vca_mode, 1, L_VCA),
                 P("LEVEL", level, 127)}}},
    {"ENVELOPE", 1, {{P("ATTACK", a, 127), P("DECAY", d, 127), P("SUSTAIN", s, 127), P("RELEASE", r, 127)}}},
    {"LFO", 1, {{P("RATE", lfo_rate, 127), P("DELAY", lfo_delay, 127), P("TO DCO", lfo_dco, 127),
                 P("TO VCF", lfo_vcf, 127)}}},
    {"CHORUS", 1, {{PL("CHORUS", chorus, 3, L_CHO), PL("MODE", mode, 2, L_MODE), P("PORTA", porta, 127),
                    P("SPREAD", detune, 127)}}},
    {"GLOBAL", 2, {{{"TUNE", 2, (uint8_t)offsetof(j_global_t, tune), -50, 50, 0},
                    {"VELOCITY", 1, (uint8_t)offsetof(j_global_t, vel), 0, 127, 0},
                    {"ARP", 1, (uint8_t)offsetof(j_global_t, arp_mode), 0, 2, L_ARP},
                    {"OCTAVES", 1, (uint8_t)offsetof(j_global_t, arp_range), 1, 3, 0}},
                   {{"TEMPO", 1, (uint8_t)offsetof(j_global_t, arp_rate), 0, 127, 0},
                    {"THEME", 3, (uint8_t)offsetof(settings_t, theme), 0, 5, L_THEME}, {0}, {0}}}},
};

static int pget(const prm_t *q)
{
    if (q->base == 0)
        return ((const uint8_t *)&j_p)[q->off];
    if (q->base == 1)
        return ((const uint8_t *)&j_g)[q->off];
    if (q->base == 3)
        return ((const uint8_t *)&set)[q->off];
    return ((const int8_t *)&j_g)[q->off];
}

static void pset(const prm_t *q, int v)
{
    v = (int)clamp(v, q->min, q->max);
    if (q->base == 0)
        ((uint8_t *)&j_p)[q->off] = (uint8_t)v;
    else if (q->base == 1)
        ((uint8_t *)&j_g)[q->off] = (uint8_t)v;
    else if (q->base == 3)
        ((uint8_t *)&set)[q->off] = (uint8_t)v;
    else
        ((int8_t *)&j_g)[q->off] = (int8_t)v;
}

/* ------------------------------------------------------------------- text --- */
static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void name_text(const char *n8, char *b)   /* 8 space-padded characters */
{
    int i, e = 0;
    for (i = 0; i < 8; i++) {
        b[i] = n8[i];
        if (n8[i] != ' ')
            e = i + 1;
    }
    b[e] = 0;
}

static void slot_text(int i, char *b)       /* "3-5": bank, memory */
{
    b[0] = (char)('1' + i / 8);
    b[1] = '-';
    b[2] = (char)('1' + i % 8);
    b[3] = 0;
}

static void prm_text(const prm_t *q, char *t)
{
    int v = pget(q);
    if (q->lab)
        t[0] = 0, text_cat(t, q->lab[v]);
    else
        int_text(v, t);
}

/* ------------------------------------------------------------ the engine --- */
static volatile int16_t pc_pending = -1;        /* a MIDI program change waiting for the main loop */
static volatile uint32_t midi_count;

static void midi_in(void)                       /* the render context: the packets that came in */
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        if (st >= 0xF0u)
            continue;
        midi_count++;
        if ((st & 0xF0u) == 0xC0u)
            pc_pending = (int16_t)(d1 % J_NPATCH);
        else
            j_event((uint8_t)st, (uint8_t)d1, (uint8_t)d2);
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    float gain = (float)master_q12 / 4096.0f * 8300000.0f;
    midi_in();
    j_render(lr, n, gain);
}

void app_panic(void) { j_silence(); }
void app_overload(void) { j_shed(); }

/* --------------------------------------------------------------- settings --- */
static uint8_t cur;                      /* the memory in use */

static void pcopy(j_patch_t *d, const j_patch_t *s)   /* (not a struct copy: the device has no memcpy) */
{
    uint8_t *dd = (uint8_t *)d;
    const uint8_t *ss = (const uint8_t *)s;
    uint32_t i;
    for (i = 0; i < sizeof *d; i++)
        dd[i] = ss[i];
}

static void bank_defaults(void)
{
    int i;
    bank.magic = BANK_MAGIC;
    for (i = 0; i < J_NPATCH; i++)
        j_factory(i, &bank.p[i]);
}

static void load_patch(int i)
{
    cur = (uint8_t)clamp(i, 0, J_NPATCH - 1);
    pcopy(&j_p, &bank.p[cur]);
    patch_dirty = 0;
    set.patch = cur;
    set_dirty = 1;
}

static void globals_from_settings(void)
{
    j_g.tune = set.tune;
    j_g.vel = set.vel;
    j_g.arp_mode = set.arp_mode;
    j_g.arp_range = set.arp_range;
    j_g.arp_rate = set.arp_rate;
}

static void settings_defaults(void)
{
    set.magic = SET_MAGIC;
    set.octave = 0;
    set.patch = 0;
    set.tune = 0;
    set.vel = 80;
    set.arp_mode = J_ARP_UP;
    set.arp_range = 1;
    set.arp_rate = 80;
    set.theme = 5;
    set.seq_n = 0;
}

static int seq_ok(void)
{
    uint32_t i;
    if (set.seq_n > J_SEQ_MAX)
        return 0;
    for (i = 0; i < set.seq_n; i++)
        if ((set.seq[i] > 127u && set.seq[i] != J_SEQ_REST) || set.seq_vel[i] > 127u)
            return 0;
    return 1;
}

void app_init(void)
{
    uint32_t i;
    j_init();
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC ||
        set.octave < -2 || set.octave > 2 || set.patch >= J_NPATCH || set.tune < -50 || set.tune > 50 ||
        set.vel > 127u || set.arp_mode >= J_NARP || set.arp_range < 1u || set.arp_range > 3u || set.arp_rate > 127u ||
        set.theme > 5u || !seq_ok())
        settings_defaults();
    if (plat_store_load(OBJ_PATCH, &bank, sizeof bank) != (int)sizeof bank || bank.magic != BANK_MAGIC)
        bank_defaults();
    globals_from_settings();
    for (i = 0; i < set.seq_n; i++) {
        j_seq[i] = set.seq[i];
        j_seq_vel[i] = set.seq_vel[i];
    }
    j_seq_n = set.seq_n;
    palette_set(set.theme);
    cur = set.patch;
    pcopy(&j_p, &bank.p[cur]);
    set_dirty = 0;
}

/* --------------------------------------------------------------------- UI --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    uint8_t vel[NKEYS], prs[NKEYS];
    int8_t knote[NKEYS];                 /* the note each held key started (the octave may change) */
    int32_t last_note;
    uint8_t page, sub;
    int8_t oct_prev;                     /* the octave before the last single OCT press (a chord undoes it) */
    uint8_t at;                          /* the channel pressure last sent */
    uint8_t redraw;
    uint32_t turned_at[NE];
} ui;

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void say_patch(void)
{
    char s[4], n[9];
    slot_text(cur, s);
    name_text(bank.p[cur].name, n);
    app_say(s, n);
}

static void save_all(void)
{
    int bad = 0;
    uint32_t i;
    pcopy(&bank.p[cur], &j_p);
    set.tune = j_g.tune;
    set.vel = j_g.vel;
    set.arp_mode = j_g.arp_mode;
    set.arp_range = j_g.arp_range;
    set.arp_rate = j_g.arp_rate;
    set.patch = cur;
    for (i = 0; i < J_SEQ_MAX; i++) {
        set.seq[i] = j_seq[i];
        set.seq_vel[i] = j_seq_vel[i];
    }
    set.seq_n = j_seq_n;
    bad |= plat_store_save(OBJ_PATCH, &bank, sizeof bank);
    bad |= plat_store_save(OBJ_SETTINGS, &set, sizeof set);
    if (bad) {
        app_say("SAVE", "FAILED");
    } else {
        patch_dirty = 0;
        set_dirty = 0;
        app_say("SAVED", "");
    }
}

static void octave_step(int32_t d)
{
    int32_t o = clamp(set.octave + d, -2, 2);
    char t[4];
    if (o == set.octave)
        return;
    set.octave = (int8_t)o;
    set_dirty = 1;
    t[0] = o < 0 ? '-' : '+';
    t[1] = (char)('0' + (o < 0 ? -o : o));
    t[2] = 0;
    app_say("OCTAVE", t);
}

static void page_button(uint32_t pg)
{
    if (ui.page == pg && PAGE[pg].nsub > 1)
        ui.sub = (uint8_t)((ui.sub + 1u) % PAGE[pg].nsub);
    else
        ui.sub = 0;
    ui.page = (uint8_t)pg;
    ui.redraw = 1;
}

static void param_turn(uint32_t k, int32_t e)    /* K1..K4 */
{
    const prm_t *q = &PAGE[ui.page].p[ui.sub][k];
    int step = (q->max - q->min) > 12 ? 2 : 1;
    char t[12];
    if (!q->name)
        return;
    pset(q, pget(q) + e * step);
    if (q->base == 0)
        patch_dirty = 1;
    else
        set_dirty = 1;
    if (q->base == 3)
        palette_set(set.theme);
    prm_text(q, t);
    app_say(q->name, t);
}

static void input(void)
{
    uint32_t keys, btn = plat_buttons(), k, pmax = 0;
    const uint32_t oc = 1u << B_OCTDN | 1u << B_OCTUP;
    int chord = (btn & oc) == oc && ((btn & ~ui.btn) & oc);
    int32_t e;
    touch_poll();
    keys = touch_keys();
    if (pc_pending >= 0) {
        int pc = pc_pending;
        pc_pending = -1;
        load_patch(pc);
        say_patch();
    }
    for (k = 0; k < NKEYS; k++) {
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit)) {
            uint32_t v = touch_vel(k);
            int32_t note = clamp(53 + (int32_t)k + 12 * set.octave, 0, 127);
            ui.vel[k] = (uint8_t)v;
            ui.prs[k] = 0;
            ui.knote[k] = (int8_t)note;
            ui.last_note = note;
            j_post(0x90u, (uint8_t)note, (uint8_t)v);
            plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)note << 16 | v << 24, TO_USB);
        } else if (!(keys & bit) && (ui.keys & bit)) {
            int32_t note = ui.knote[k];
            ui.prs[k] = 0;
            j_post(0x80u, (uint8_t)note, 0);
            plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)note << 16, TO_USB);
        } else if ((keys & bit) && touch_pressure(k) != ui.prs[k]) {
            ui.prs[k] = touch_pressure(k);
            ui.redraw = 1;
        }
        if ((keys & bit) && ui.prs[k] > pmax)
            pmax = ui.prs[k];
    }
    if (pmax != ui.at) {                     /* the JUNO has no aftertouch; here it moves the LFO like the mod lever */
        ui.at = (uint8_t)pmax;
        j_post(0xD0u, (uint8_t)pmax, 0);
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if (!(btn & bit) || (ui.btn & bit))
            continue;
        switch (k) {
        case B_OCTDN:
        case B_OCTUP:
            if (chord) {
                if (ui.btn & oc)
                    set.octave = ui.oct_prev;
                j_cmd(J_CMD_PANIC, 0);
                app_say("PANIC", "");
            } else {
                if (!(ui.btn & oc))
                    ui.oct_prev = set.octave;
                octave_step(k == B_OCTDN ? -1 : 1);
            }
            break;
        case B_HOME: page_button(PG_HOME); break;
        case B_EDIT: page_button(PG_DCO); break;
        case B_SEL: page_button(PG_VCF); break;
        case B_ENV: page_button(PG_ENV); break;
        case B_LFO: page_button(PG_LFO); break;
        case B_FX: page_button(PG_FX); break;
        case B_GLO: page_button(PG_GLO); break;
        case B_SAVE: save_all(); break;
        case B_ARP:
            if (j_seq_rec) {
                j_cmd(J_CMD_REST, 0);
                app_say("REST", "");
            } else if (j_seq_on) {
                app_say("SEQ", "RUNNING");
            } else {
                j_cmd(J_CMD_ARP, -1);
                app_say("ARP", j_arp_on ? "OFF" : "ON");   /* j_arp_on changes with the next block */
            }
            break;
        case B_PLAY:
            j_cmd(J_CMD_HOLD, -1);
            app_say("HOLD", j_hold ? "OFF" : "ON");
            break;
        case B_REC:
            j_cmd(J_CMD_REC, -1);
            set_dirty = 1;
            app_say("REC", j_seq_rec ? "DONE" : "PLAY KEYS");
            break;
        case B_SEQ:
            if (!j_seq_n) {
                app_say("SEQ", "EMPTY: REC");
            } else {
                j_cmd(J_CMD_SEQ, -1);
                app_say("SEQ", j_seq_on ? "STOP" : "PLAY");
            }
            break;
        default: break;
        }
    }
    for (k = 0; k < NE; k++) {
        if ((e = plat_enc((int)k)) == 0)
            continue;
        ui.turned_at[k] = plat_ms();
        ui.redraw = 1;
        if (k == EN_SELECT) {
            if (PAGE[ui.page].nsub > 1)
                ui.sub = (uint8_t)((ui.sub + PAGE[ui.page].nsub + (uint32_t)(e > 0 ? 1 : -1)) % PAGE[ui.page].nsub);
        } else if (k == EN_PRESET || k == EN_ALGO) {
            load_patch((int)cur + e * (k == EN_ALGO ? 8 : 1));
            say_patch();
        } else {
            param_turn(k - EN_K1, e);
        }
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
    {
        static const uint8_t PB[NPAGE] = {B_HOME, B_EDIT, B_SEL, B_ENV, B_LFO, B_FX, B_GLO};
        uint32_t lit = 1u << PB[ui.page];
        if (j_arp_on)
            lit |= 1u << B_ARP;
        if (j_hold)
            lit |= 1u << B_PLAY;
        if (j_seq_on)
            lit |= 1u << B_SEQ;
        if (j_seq_rec)
            lit |= 1u << B_REC;
        if (patch_dirty || set_dirty)
            lit |= 1u << B_SAVE;
        plat_glow(1);
        plat_leds(lit, keys);
    }
}

void app_input_only(void) { input(); }

/* ---------------------------------------------------------------- drawing --- */
static void key_cell(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t k, uint16_t off)
{
    int32_t v, p;
    cv_rect(x, y, w, h, off);
    if (!(ui.keys >> k & 1u))
        return;
    v = (ui.vel[k] * h + 126) / 127;
    p = (ui.prs[k] * h + 126) / 127;
    cv_rect(x, y + h - v, w, v, C_HI);
    if (p)
        cv_rect(x + 2, y + h - p, w - 4, p, C_WHITE);
}

static int32_t lvl_y(int v, int32_t top, int32_t h) { return top + h - (v * h) / 127; }

static void draw_env(void)                   /* the ADSR as the hardware's sliders set it */
{
    int32_t top = 80, h = 56, x0 = 8, xa, xd, xs, xr;
    int s = j_p.s;
    xa = x0 + 4 + (j_p.a * 50) / 127;
    xd = xa + 4 + (j_p.d * 50) / 127;
    xs = xd + 34;
    xr = xs + 4 + (j_p.r * 50) / 127;
    cv_line(x0, top + h, xa, top, C_HI);
    cv_line(xa, top, xd, lvl_y(s, top, h), C_HI);
    cv_line(xd, lvl_y(s, top, h), xs, lvl_y(s, top, h), C_HI);
    cv_line(xs, lvl_y(s, top, h), xr, top + h, C_HI);
    cv_line(x0, top + h, 236, top + h, C_LINE);
}

static void draw_vcf(void)                   /* the low-pass: the corner, the resonance peak, the 24 dB fall */
{
    int32_t top = 80, h = 56, x, y, prev_y = top + 18;
    int32_t fc = 24 + (j_p.cutoff * 180) / 127, peak = (j_p.res * 20) / 127;
    for (x = 8; x < 236; x += 2) {
        int32_t d = x - fc;
        y = top + 18;
        if (d < 0 && d > -10)
            y -= peak * (10 + d) / 10;
        else if (d >= 0)
            y += d * 2 / 3 + d * d / 90;
        y = (int32_t)clamp(y, top, top + h);
        if (x > 8)
            cv_line(x - 2, prev_y, x, y, C_HI);
        prev_y = y;
    }
    cv_line(fc, top + h - 4, fc, top + h, C_GRAY);
}

static void draw_status(void)
{
    char t[24];
    uint32_t i;
    t[0] = 0;
    text_cat(t, "VOICES ");
    int_text(j_nvoices, t + 7);
    cv_text(8, 80, &FONT_XS, t, C_GRAY);
    cv_text(8, 95, &FONT_XS, j_arp_on ? "ARP ON" : "ARP OFF", j_arp_on ? C_HI : C_DIM);
    cv_text(8, 110, &FONT_XS, j_hold ? "HOLD ON" : "HOLD OFF", j_hold ? C_HI : C_DIM);
    cv_text(110, 80, &FONT_XS, L_MODE[j_p.mode % J_NMODE], C_HI);
    cv_text(110, 95, &FONT_XS, "CHORUS", C_GRAY);
    cv_text(150, 95, &FONT_XS, L_CHO[j_p.chorus % J_NCHO], C_HI);
    cv_text(110, 110, &FONT_XS, "SEQ", C_GRAY);
    t[0] = 0;
    if (j_seq_rec)
        text_cat(t, "REC");
    else
        text_cat(t, j_seq_on ? "PLAY" : j_seq_n ? "READY" : "EMPTY");
    cv_text(150, 110, &FONT_XS, t, j_seq_rec || j_seq_on ? C_WHITE : C_DIM);
    for (i = 0; i < J_SEQ_MAX; i++) {        /* the pattern: a cell per step, tall for a note, low for a rest */
        int32_t x = 4 + (int32_t)i * 14;
        uint16_t c = C_LINE;
        int32_t h = 3;
        if (i < j_seq_n) {
            int rest = j_seq[i] == J_SEQ_REST;
            c = rest ? C_DIM : C_HI;
            h = rest ? 3 : 3 + (j_seq[i] % 12);
            if (j_seq_on && i == j_seq_pos)
                c = C_WHITE;
        }
        cv_rect(x, 140 - h, 13, h, c);
    }
}

static void draw(void)
{
    char t[24], s[4], n[9];
    int32_t note = ui.last_note;
    uint32_t i, now = plat_ms(), held = ui.keys;
    const page_t *pg = &PAGE[ui.page];
    static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;
    cv_begin(240, 32, C_BLACK);
    if (set.theme == 5u)
        cv_rect(0, 0, 240, 28, RGB(0, 108, 171));   /* the JUNO theme's blue banner */
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        int32_t x;
        slot_text(cur, s);
        name_text(j_p.name, n);
        x = cv_text(10, 6, &FONT_B, s, C_GRAY);
        x = cv_text(x + 8, 6, &FONT_B, n, C_WHITE);
        if (patch_dirty)
            cv_text(x + 6, 10, &FONT_XS, "*", C_HI);
        if (held) {
            static const char *const N[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            char o[8];
            t[0] = 0;
            text_cat(t, N[note % 12]);
            int_text(note / 12 - 1, o);
            text_cat(t, o);
            cv_text(230 - text_w(&FONT_B, t), 6, &FONT_B, t, C_HI);
        }
    }
    cv_rect(10, 29, 220, 1, C_LINE);
    if (j_seq_rec || j_seq_on) {
        const char *b = j_seq_rec ? "REC" : "SEQ";
        int32_t w = text_w(&FONT_XS, b);
        if (!held)
            cv_text(230 - w, 8, &FONT_XS, b, C_WHITE);
    }
    cv_blit(0, 0);
    cv_begin(240, 144, C_BLACK);
    {
        static const char *const TAB[NPAGE] = {"HOME", "DCO", "VCF", "ENV", "LFO", "FX", "GLO"};
        static const uint8_t ORD[NPAGE] = {PG_HOME, PG_DCO, PG_VCF, PG_ENV, PG_LFO, PG_FX, PG_GLO};
        int32_t tx = 4;
        for (i = 0; i < NPAGE; i++) {
            int on = ORD[i] == ui.page;
            int32_t w = text_w(&FONT_XS, TAB[i]);
            cv_text(tx, 0, &FONT_XS, TAB[i], on ? C_HI : C_DIM);
            if (on)
                cv_rect(tx, 14, w, 2, C_HI);
            tx += w + 9;
        }
    }
    if (pg->nsub > 1) {
        int_text(ui.sub + 1, t);
        text_cat(t, "/");
        int_text(pg->nsub, t + 2);
        cv_text(236 - text_w(&FONT_XS, t), 0, &FONT_XS, t, C_AMB);
    }
    cv_rect(4, 17, 232, 1, C_LINE);
    for (i = 0; i < 4u; i++) {
        const prm_t *q = &pg->p[ui.sub][i];
        int32_t x = 4 + (int32_t)i * 58, w = 54, f;
        int lit = ui.turned_at[EN_K1 + i] && now - ui.turned_at[EN_K1 + i] < 300u;
        if (!q->name)
            continue;
        cv_text(x, 20, &FONT_XS, q->name, lit ? C_HI : C_GRAY);
        prm_text(q, t);
        cv_text(x, 34, &FONT_B, t, lit ? C_WHITE : C_HI);
        f = ((pget(q) - q->min) * w) / (q->max - q->min);
        cv_rect(x, 58, w, 5, C_LINE);
        cv_rect(x, 58, f > 0 ? f : 1, 5, lit ? C_HI : C_DIM);
    }
    if (ui.page == PG_ENV)
        draw_env();
    else if (ui.page == PG_VCF && ui.sub == 0)
        draw_vcf();
    else
        draw_status();
    cv_blit(0, 36);
    cv_begin(240, 60, C_BLACK);
    for (i = 0; i < 16u; i++)
        key_cell(4 + (int32_t)i * 14, 14, 13, 14, WK[i], C_DIM);
    for (i = 0; i < 11u; i++) {
        static const uint8_t AT[11] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
        key_cell(4 + (int32_t)AT[i] * 14 + 8, 0, 11, 12, BK[i], C_LINE);
    }
    t[0] = set.octave < 0 ? '-' : '+';
    t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave));
    t[2] = 0;
    cv_text(4, 36, &FONT_XS, "OCT", C_GRAY);
    cv_text(30, 36, &FONT_XS, t, C_HI);
    cv_text(64, 36, &FONT_XS, "VOL", C_GRAY);
    int_text((int32_t)(plat_master() * 100u / 4096u), t);
    text_cat(t, "%");
    cv_text(88, 36, &FONT_XS, t, C_HI);
    if (j_arp_on)
        cv_text(130, 36, &FONT_XS, "ARP", C_HI);
    if (j_hold)
        cv_text(158, 36, &FONT_XS, "HOLD", C_HI);
    if (j_seq_rec || j_seq_on)
        cv_text(192, 36, &FONT_XS, j_seq_rec ? "REC" : "SEQ", C_WHITE);
    cv_blit(0, 180);
}

void app_ui_init(void)
{
    touch_on(1);
    palette_set(set.theme);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_frame(void)
{
    static uint32_t last_pct, last_v, last_arp, last_seq;
    uint32_t now = plat_ms(), pct = plat_master() * 100u / 4096u, i, any = 0;
    uint32_t seq_state = j_seq_on | j_seq_rec << 1 | (uint32_t)j_seq_pos << 2 | (uint32_t)j_seq_n << 8;
    input();
    ann_tick(now);
    for (i = 0; i < NE; i++)
        if (ui.turned_at[i] && now - ui.turned_at[i] < 400u)
            any = 1;
    if (any || (ui.msg_until && now >= ui.msg_until) || pct != last_pct || j_nvoices != last_v ||
        j_arp_on != last_arp || seq_state != last_seq) {
        if (now >= ui.msg_until)
            ui.msg_until = 0;
        last_pct = pct;
        last_v = j_nvoices;
        last_arp = j_arp_on;
        last_seq = seq_state;
        ui.redraw = 1;
    }
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "patch"))
        *got = cur;
    else if (!strcmp(what, "page"))
        *got = ui.page;
    else if (!strcmp(what, "sub"))
        *got = ui.sub;
    else if (!strcmp(what, "dirty"))
        *got = patch_dirty | set_dirty << 1;
    else if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "voices"))
        *got = j_nvoices;
    else if (!strcmp(what, "arp"))
        *got = j_arp_on;
    else if (!strcmp(what, "hold"))
        *got = j_hold;
    else if (!strcmp(what, "seqn"))
        *got = j_seq_n;
    else if (!strcmp(what, "seq"))
        *got = j_seq_on;
    else if (!strcmp(what, "rec"))
        *got = j_seq_rec;
    else if (!strcmp(what, "theme"))
        *got = set.theme;
    else if (!strcmp(what, "cutoff"))
        *got = j_p.cutoff;
    else if (!strcmp(what, "res"))
        *got = j_p.res;
    else if (!strcmp(what, "mode"))
        *got = j_p.mode;
    else if (!strcmp(what, "chorus"))
        *got = j_p.chorus;
    else if (!strcmp(what, "tune"))
        *got = j_g.tune;
    else if (!strcmp(what, "note"))
        *got = ui.last_note;
    else
        return 0;
    return 1;
}
#endif
