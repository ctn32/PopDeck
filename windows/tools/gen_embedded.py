#!/usr/bin/env python3
# Derleme oncesi bir kez calistirilir (zaten calistirilmis halde gelir): diller/*.json + web.html -> src/embedded_data.cpp
import os, sys
kok = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
dosyalar = []
for f in sorted(os.listdir(os.path.join(kok, "diller"))):
    if f.endswith(".json"):
        dosyalar.append(("diller/" + f, os.path.join(kok, "diller", f)))
dosyalar.append(("web.html", os.path.join(kok, "src", "web.html")))
out = ['// OTOMATIK URETILDI (tools/gen_embedded.py) - elle duzenleme', '#include "embedded.hpp"', '']
tablo = []
for i, (ad, yol) in enumerate(dosyalar):
    veri = open(yol, "rb").read()
    out.append("static const unsigned char d%d[] = {" % i)
    for k in range(0, len(veri), 24):
        out.append(",".join(str(b) for b in veri[k:k + 24]) + ",")
    out.append("0};")
    tablo.append('    {"%s", d%d, %d},' % (ad, i, len(veri)))
out.append("static const Embedded tablo[] = {")
out += tablo
out.append("};")
out.append("const Embedded* embedded_list(size_t& n) { n = sizeof(tablo) / sizeof(tablo[0]); return tablo; }")
out.append("const Embedded* embedded_find(const char* name) {")
out.append("    for (size_t i = 0; i < sizeof(tablo) / sizeof(tablo[0]); i++) { const char* a = tablo[i].name; const char* b = name; while (*a && *a == *b) { a++; b++; } if (*a == 0 && *b == 0) return &tablo[i]; }")
out.append("    return nullptr;")
out.append("}")
open(os.path.join(kok, "src", "embedded_data.cpp"), "w", encoding="utf-8").write("\n".join(out) + "\n")
print("embedded_data.cpp:", len(dosyalar), "dosya")
