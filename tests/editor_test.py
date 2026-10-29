"""Checks that src/juno.c, editor/params.js, editor/patches.js and docs/MIDI_CC.md agree.   python tests/editor_test.py"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
rd = lambda *p: open(os.path.join(ROOT, *p), encoding="utf-8").read()
juno, params, patches, doc = rd("src", "juno.c"), rd("editor", "params.js"), rd("editor", "patches.js"), rd("docs", "MIDI_CC.md")
errors = []


def check(ok, msg):
    if not ok:
        errors.append(msg)


# firmware: cc -> (field, min, max)
fw = {}
for c, f, mx in re.findall(r"CCP\((\d+), (\w+), (\d+)\)", juno):
    fw[int(c)] = (f, 0, int(mx))
for c, f, mn, mx in re.findall(r"CCG\((\d+), (\w+), (\d+), (\d+)\)", juno):
    fw[int(c)] = (f, int(mn), int(mx))
fw[102] = ("tune", 0, 100)

# front end
js = {}
for m in re.finditer(r"\{cc: (\d+), key: \"(\w+)\".*?min: (\d+), max: (\d+)", params):
    js[int(m.group(1))] = (m.group(2), int(m.group(3)), int(m.group(4)))

check(set(fw) == set(js), "CC sets differ: firmware-only %s, editor-only %s" % (sorted(set(fw) - set(js)), sorted(set(js) - set(fw))))
for cc, v in fw.items():
    if cc in js:
        check(js[cc] == v, "CC %d: firmware %s, editor %s" % (cc, v, js[cc]))
    check(re.search(r"^\| \*\*%d\*\* \|" % cc, doc, re.M) is not None, "CC %d is not in docs/MIDI_CC.md" % cc)

# the switches the firmware handles in cc()
for cc in re.findall(r"n == (10[7-9]|110)\)", juno):
    check(re.search(r"^\| \*\*%s\*\* \|" % cc, doc, re.M) is not None, "switch CC %s is not in docs/MIDI_CC.md" % cc)
    check(("cc: %s," % cc) in params, "switch CC %s is not in params.js" % cc)

# the theme CC: handled in app.c, listed in the doc and the editor, one colour per firmware palette
theme_cc = re.search(r"CC_THEME (\d+)u", rd("src", "app.c")).group(1)
check(re.search(r"^\| \*\*%s\*\* \|" % theme_cc, doc, re.M) is not None, "theme CC %s is not in docs/MIDI_CC.md" % theme_cc)
check("J6_THEME_CC = %s;" % theme_cc in params, "theme CC %s is not in params.js" % theme_cc)
pal = re.findall(r"\{\"(\w+)\", \{NEUTRALS, RGB\((\d+), (\d+), (\d+)\), RGB\((\d+), (\d+), (\d+)\)\}\}", rd("fm1-base", "device", "gfx.c"))
th = re.findall(r"\{name: \"(\w+)\", value: \"rgb\((\d+),(\d+),(\d+)\)\", bright: \"rgb\((\d+),(\d+),(\d+)\)\"\}", params)
check(pal == th and len(pal) == 6, "editor themes differ from the firmware palettes")

# the field order of a sound is the j_patch_t order
struct = re.search(r"typedef struct \{\s*char name\[8\];(.*?)\} j_patch_t;", rd("src", "juno.h"), re.S).group(1)
struct = re.sub(r"/\*.*?\*/", "", struct, flags=re.S)
fields = [n.strip() for d in re.findall(r"uint8_t ([^;]+);", struct) for n in d.split(",")]
js_fields = re.findall(r'"(\w+)"', re.search(r"J6_FIELDS = \[(.*?)\];", params, re.S).group(1))
check(fields == js_fields, "J6_FIELDS differs from j_patch_t: %s vs %s" % (fields, js_fields))
for f in js_fields:
    check(any(v[0] == f for v in js.values()), "field %s has no CC" % f)

# patches.js is up to date
gen = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "gen_patches.py")], capture_output=True, text=True)
check(gen.returncode == 0, "gen_patches.py failed: " + gen.stderr)
check(rd("editor", "patches.js") == patches, "editor/patches.js was out of date (it has been regenerated)")
names = re.findall(r'name: "([^"]+)"', patches)
check(len(names) == 56 and len(set(names)) == 56, "expected 56 distinct factory names, got %d/%d" % (len(names), len(set(names))))
for name, vals in re.findall(r'name: "([^"]+)", v: \[([^\]]*)\]', patches):
    for f, x in zip(js_fields, (int(i) for i in vals.split(","))):
        lo, hi = next((v[1], v[2]) for v in js.values() if v[0] == f)
        check(lo <= x <= hi, "%s: %s = %d is outside %d..%d" % (name, f, x, lo, hi))

print("\n".join("FAIL: " + e for e in errors) if errors else "editor_test: all passed (%d CCs, %d patches)" % (len(fw), len(names)))
sys.exit(1 if errors else 0)
