# -*- coding: utf-8 -*-
"""
PopDeck dil yukleyici. Ceviriler 'diller' klasorundeki JSON dosyalarindadir (en.json, de.json, es.json, fr.json, ru.json).

* Her dosya: {"_ad": "Deutsch", "<Turkce metin>": "<ceviri>", ...}  (Turkce metin = programdaki anahtar)
* Yeni dil eklemek: diller klasorune <kod>.json koy ("_ad" satirini unutma). Program otomatik gorur.
* Eksik ceviri olursa o metin Turkce gorunur (program calismaya devam eder).
* Exe kullaniyorsan exe'nin yanindaki 'diller' klasoru, paketlenmis olanin yerine gecer:
  yeniden derlemeden ceviri duzeltebilirsin.
* Eksik/kullanilmayan cevirileri bulmak icin: python ceviri_araci.py kontrol
"""
import json
import os
import sys

SIRA = ["tr", "en", "de", "es", "fr", "ru"]          # Listede bu sirayla gorunur; digerleri alfabetik eklenir
CEVIRI = {}                                           # kod -> {turkce metin: ceviri}
_ADLAR = {"tr": "Türkçe"}


def _klasorler():
    taban = os.path.dirname(os.path.abspath(sys.executable if getattr(sys, "frozen", False) else __file__))
    yollar = [os.path.join(taban, "diller")]
    if getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS"):
        yollar.append(os.path.join(sys._MEIPASS, "diller"))
    return yollar


def yukle():
    CEVIRI.clear()
    for klasor in reversed(_klasorler()):             # once paketlenmis, sonra exe yanindaki (ustune yazar)
        if not os.path.isdir(klasor):
            continue
        for dosya in sorted(os.listdir(klasor)):
            if not dosya.endswith(".json") or dosya.startswith("_"):
                continue
            kod = dosya[:-5]
            try:
                with open(os.path.join(klasor, dosya), "r", encoding="utf-8") as f:
                    veri = json.load(f)
            except (OSError, ValueError):
                continue
            if not isinstance(veri, dict):
                continue
            _ADLAR[kod] = str(veri.pop("_ad", kod))
            CEVIRI.setdefault(kod, {}).update({str(k): str(v) for k, v in veri.items() if v})


yukle()
DIL_KODLARI = ["tr"] + [k for k in SIRA[1:] if k in CEVIRI] + sorted(k for k in CEVIRI if k not in SIRA)
DIL_ADLARI = [_ADLAR.get(k, k) for k in DIL_KODLARI]
