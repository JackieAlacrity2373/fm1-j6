"""Spectral checks of the engine_test WAVs: python analyze.py DIR"""
import sys, wave
import numpy as np

d = sys.argv[1]

def load(n):
    w = wave.open(f"{d}/{n}.wav")
    x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(float) / 32768
    return x[0::2], w.getframerate()

def spec(x, sr, a=0.3, b=0.9):
    x = x[int(a * sr):int(b * sr)]
    s = np.abs(np.fft.rfft(x * np.hanning(len(x)))) / len(x)
    return np.fft.rfftfreq(len(x), 1 / sr), s

def db(v):
    return 20 * np.log10(v + 1e-12)

x, sr = load("saw_a5")
f, s = spec(x, sr)
f0 = 880.0 * 2 ** (0 / 12) * 1.0
k0 = np.argmax(s)
print("saw A5: strongest bin %.1f Hz" % f[k0])
harm = set(int(round(h * f0 / (f[1]))) for h in range(1, 25))
mask = np.ones(len(s), bool)
for h in range(1, 25):
    c = int(round(h * f[k0] / f[1]))
    mask[max(c - 6, 0):c + 7] = False
mask[:20] = False
print("saw A5: worst non-harmonic component %.1f dB re fundamental" % (db(s[mask].max()) - db(s[k0])))

for n in ("lpf_res0", "lpf_res110", "selfosc"):
    x, sr = load(n)
    f, s = spec(x, sr, 0.5, 1.0)
    k = np.argmax(s)
    print(f"{n}: peak at {f[k]:.0f} Hz, {db(s[k]):.1f} dB")
    if n == "lpf_res0":
        for lo, hi in ((300, 400), (600, 800), (1200, 1600), (2400, 3200)):
            m = (f >= lo) & (f < hi)
            print(f"   band {lo}-{hi}: {db(s[m].max()):.1f} dB")

for n in ("patch01", "patch09"):
    w = wave.open(f"{d}/{n}.wav")
    x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(float)
    print(n, "L/R correlation %.3f" % np.corrcoef(x[0::2], x[1::2])[0, 1])
