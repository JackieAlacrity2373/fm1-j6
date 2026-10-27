/* Engine test: renders scenes of the J6 engine to WAV files and checks them (no platform needed).
 *   gcc -O2 -ffp-contract=off -I ../fm1-base/include -o engine_test engine_test.c ../juno.c -lm
 *   engine_test OUTDIR     then analyze.py OUTDIR for the spectra */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../juno.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } else { printf("  ok: " __VA_ARGS__); printf("\n"); } } while (0)

static float *rec;
static uint32_t rec_n, rec_cap;
static float peak;
static int nan_seen;

static void run(float seconds)
{
    uint32_t blocks = (uint32_t)(seconds * 44100.0f / 256.0f), b, i;
    static int32_t buf[512];
    for (b = 0; b < blocks; b++) {
        j_render(buf, 256, 8300000.0f);
        for (i = 0; i < 512; i++) {
            float s = (float)buf[i] / 8388608.0f;
            if (s != s)
                nan_seen = 1;
            if (fabsf(s) > peak)
                peak = fabsf(s);
            if (rec_n < rec_cap)
                rec[rec_n++] = s;
        }
    }
}

static void save(const char *dir, const char *name)
{
    char path[512];
    FILE *f;
    uint32_t i, bytes = rec_n * 2u, v;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    f = fopen(path, "wb");
    if (!f)
        return;
    fwrite("RIFF", 1, 4, f);
    v = 36 + bytes;
    fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16;
    fwrite(&v, 4, 1, f);
    { uint16_t a[2] = {1, 2}; fwrite(a, 2, 2, f); }
    v = 44100;
    fwrite(&v, 4, 1, f);
    v = 44100 * 4;
    fwrite(&v, 4, 1, f);
    { uint16_t a[2] = {4, 16}; fwrite(a, 2, 2, f); }
    fwrite("data", 1, 4, f);
    fwrite(&bytes, 4, 1, f);
    for (i = 0; i < rec_n; i++) {
        float s = rec[i] * 32767.0f;
        int16_t q = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
        fwrite(&q, 2, 1, f);
    }
    fclose(f);
}

static void scene_begin(void)
{
    rec_n = 0;
    peak = 0;
    nan_seen = 0;
    j_init();
    j_cmd(J_CMD_PANIC, 0);
}

static void on(int n, int v) { j_post(0x90, (uint8_t)n, (uint8_t)v); }
static void off(int n) { j_post(0x80, (uint8_t)n, 0); }

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int i;
    rec_cap = 44100u * 2u * 20u;
    rec = (float *)malloc(rec_cap * sizeof(float));

    printf("factory patches\n");
    for (i = 0; i < j_factory_count(); i++) {
        j_patch_t p;
        char nm[16];
        int k;
        scene_begin();
        j_factory(i, &p);
        for (k = 0; k < 8; k++)
            nm[k] = p.name[k];
        nm[8] = 0;
        j_p = p;
        on(48, 100); on(55, 100); on(60, 100); on(64, 100);
        run(1.0f);
        off(48); off(55); off(60); off(64);
        run(1.5f);
        CHECK(!nan_seen && peak > 0.02f && peak < 1.0f, "patch %d %s peak %.3f", i, nm, peak);
        { char fn[32]; snprintf(fn, sizeof fn, "patch%02d", i); save(dir, fn); }
    }

    printf("saw, filter open (aliasing / level)\n");
    scene_begin();
    j_default_patch(&j_p);
    j_p.cutoff = 127; j_p.res = 0; j_p.s = 127; j_p.chorus = 0;
    on(81, 127);
    run(1.0f);
    CHECK(peak > 0.05f && peak < 0.9f, "single voice peak %.3f", peak);
    save(dir, "saw_a5");

    printf("filter slope and resonance\n");
    scene_begin();
    j_default_patch(&j_p);
    j_p.cutoff = 70; j_p.res = 0; j_p.kbd = 0; j_p.s = 127; j_p.chorus = 0;
    on(36, 127);
    run(1.0f);
    save(dir, "lpf_res0");
    scene_begin();
    j_default_patch(&j_p);
    j_p.cutoff = 70; j_p.res = 110; j_p.kbd = 0; j_p.s = 127; j_p.chorus = 0;
    on(36, 127);
    run(1.0f);
    CHECK(!nan_seen && peak < 1.0f, "resonant peak %.3f", peak);
    save(dir, "lpf_res110");

    printf("self oscillation (almost no input)\n");
    scene_begin();
    j_default_patch(&j_p);
    j_p.saw = 0; j_p.noise = 4; j_p.cutoff = 70; j_p.res = 127; j_p.kbd = 0; j_p.s = 127; j_p.chorus = 0;
    on(36, 127);
    run(1.5f);
    CHECK(!nan_seen && peak > 0.01f && peak < 1.0f, "self-oscillation peak %.3f", peak);
    save(dir, "selfosc");

    printf("polyphony: 8 notes on 6 voices\n");
    scene_begin();
    j_default_patch(&j_p);
    j_p.chorus = 0; j_p.s = 127; j_p.r = 100;
    for (i = 0; i < 8; i++)
        on(48 + i * 3, 100);
    run(0.3f);
    CHECK(j_nvoices == 6, "voices %d", (int)j_nvoices);
    for (i = 0; i < 8; i++)
        off(48 + i * 3);
    run(8.0f);
    CHECK(j_nvoices == 0, "voices released and freed: %d", (int)j_nvoices);

    printf("unison and solo\n");
    scene_begin();
    j_default_patch(&j_p);
    j_p.mode = J_MODE_UNISON; j_p.detune = 60; j_p.s = 127;
    on(60, 100); on(64, 100);
    run(0.3f);
    CHECK(j_nvoices == 6, "unison voices %d", (int)j_nvoices);
    off(64);
    run(0.1f);
    off(60);
    run(4.0f);
    CHECK(j_nvoices == 0, "unison freed %d", (int)j_nvoices);
    scene_begin();
    j_default_patch(&j_p);
    j_p.mode = J_MODE_SOLO;
    on(60, 100); on(64, 100); on(67, 100);
    run(0.2f);
    CHECK(j_nvoices == 1, "solo voices %d", (int)j_nvoices);

    printf("hold and arpeggiator\n");
    scene_begin();
    j_default_patch(&j_p);
    j_p.chorus = 0; j_p.s = 127;
    j_cmd(J_CMD_HOLD, 1);
    on(60, 100); run(0.1f); off(60); run(0.1f);
    CHECK(j_nvoices == 1, "held after release: %d", (int)j_nvoices);
    j_cmd(J_CMD_HOLD, 0);
    run(4.0f);
    CHECK(j_nvoices == 0, "hold released: %d", (int)j_nvoices);
    scene_begin();
    j_default_patch(&j_p);
    j_p.chorus = 0; j_p.a = 0; j_p.d = 20; j_p.r = 20; j_p.s = 0;
    j_g.arp_rate = 90;
    j_cmd(J_CMD_ARP, 1);
    on(60, 100); on(64, 100); on(67, 100);
    run(3.0f);
    save(dir, "arp");
    CHECK(j_arp_on == 1 && peak > 0.02f, "arp sounds, peak %.3f", peak);
    off(60); off(64); off(67);
    run(3.0f);
    CHECK(j_nvoices == 0, "arp stops with the keys: %d", (int)j_nvoices);

    printf("LFO delay and vibrato do not blow up\n");
    scene_begin();
    j_default_patch(&j_p);
    j_p.lfo_dco = 127; j_p.lfo_vcf = 127; j_p.lfo_rate = 100; j_p.lfo_delay = 90; j_p.pwm_src = 1; j_p.pulse = 100; j_p.pw = 127;
    j_p.s = 127;
    on(60, 100);
    run(3.0f);
    CHECK(!nan_seen && peak < 1.0f, "mod peak %.3f", peak);
    save(dir, "lfo");

    printf("CPU: 6 voices\n");
    scene_begin();
    j_factory(5, &j_p);
    for (i = 0; i < 6; i++)
        on(40 + i * 4, 100);
    {
        clock_t t0 = clock();
        float secs;
        run(10.0f);
        secs = (float)(clock() - t0) / CLOCKS_PER_SEC;
        printf("  10 s of audio in %.3f s of host time (x%.0f realtime)\n", secs, 10.0f / secs);
    }

    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
