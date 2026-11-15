# -*- coding: utf-8 -*-
"""
PopDeck ceviri araci - ceviri dosyalarini (diller/*.json) program koduyla esler.

  python ceviri_araci.py kontrol   -> Hangi dilde hangi cumleler eksik / kullanilmiyor
  python ceviri_araci.py sablon    -> diller/_eksik.json olusturur (sadece eksik cumleler; doldurulacak yerler "")
  python ceviri_araci.py ekle      -> _eksik.json'daki DOLU cevirileri dil dosyalarina isler
  python ceviri_araci.py temizle   -> Kullanilmayan cumleleri dil dosyalarindan siler

Boylece yeni bir ozellik eklenince sadece YENI cumleler cevrilir; tum dil dosyalari yeniden yazilmaz.
"""
import ast
import json
import os
import re
import sys

KLASOR = os.path.dirname(os.path.abspath(__file__))
DILLER = os.path.join(KLASOR, "diller")
KOD = os.path.join(KLASOR, "popdeck.py")


def kullanilan():
    src = open(KOD, encoding="utf-8").read()
    anahtarlar = set()
    for n in ast.walk(ast.parse(src)):
        if (isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id == "L" and n.args
                and isinstance(n.args[0], ast.Constant) and isinstance(n.args[0].value, str)):
            anahtarlar.add(n.args[0].value)
    anahtarlar |= set(re.findall(r"T\('((?:[^'\\]|\\.)*)'\)", src))               # Telefon sayfasi (JS)
    anahtarlar |= {m.strip() for m in re.findall(r"<[^>]*data-t[^>]*>([^<]*)<", src)}
    anahtarlar |= {"bağlı", "bağlantı yok"}
    return anahtarlar


def dosyalar():
    sonuc = {}
    for ad in sorted(os.listdir(DILLER)):
        if ad.endswith(".json") and not ad.startswith("_"):
            with open(os.path.join(DILLER, ad), encoding="utf-8") as f:
                sonuc[ad[:-5]] = json.load(f)
    return sonuc


def yaz(kod, veri):
    with open(os.path.join(DILLER, kod + ".json"), "w", encoding="utf-8") as f:
        json.dump(veri, f, ensure_ascii=False, indent=1)


def main():
    komut = sys.argv[1] if len(sys.argv) > 1 else "kontrol"
    kul, dil = kullanilan(), dosyalar()
    eksik = {k: sorted(x for x in kul if not d.get(x)) for k, d in dil.items()}
    if komut == "kontrol":
        for k, d in dil.items():
            fazla = [x for x in d if x != "_ad" and x not in kul]
            print(f"{k}: {len(kul) - len(eksik[k])}/{len(kul)} ceviri tamam | eksik: {len(eksik[k])} | kullanilmayan: {len(fazla)}")
            for x in eksik[k][:8]:
                print("    EKSIK:", repr(x))
    elif komut == "sablon":
        tum = sorted({x for e in eksik.values() for x in e})
        sablon = {x: {k: dil[k].get(x, "") for k in dil} for x in tum}
        with open(os.path.join(DILLER, "_eksik.json"), "w", encoding="utf-8") as f:
            json.dump(sablon, f, ensure_ascii=False, indent=1)
        print(f"{len(tum)} eksik cumle -> diller/_eksik.json (bos \"\" olanlari doldurup 'ekle' calistir)")
    elif komut == "ekle":
        yol = os.path.join(DILLER, "_eksik.json")
        with open(yol, encoding="utf-8") as f:
            sablon = json.load(f)
        eklenen = 0
        for anahtar, ceviriler in sablon.items():
            for k, v in ceviriler.items():
                if v and k in dil and dil[k].get(anahtar) != v:
                    dil[k][anahtar] = v
                    eklenen += 1
        for k, d in dil.items():
            yaz(k, d)
        kalan = {a: c for a, c in sablon.items() if not all(c.values())}
        if kalan:
            with open(yol, "w", encoding="utf-8") as f:
                json.dump(kalan, f, ensure_ascii=False, indent=1)
        else:
            os.remove(yol)
        print(f"{eklenen} ceviri eklendi; hala eksik olan cumle: {len(kalan)}")
    elif komut == "temizle":
        for k, d in dil.items():
            silinecek = [x for x in d if x != "_ad" and x not in kul]
            for x in silinecek:
                del d[x]
            yaz(k, d)
            print(f"{k}: {len(silinecek)} kullanilmayan cumle silindi")
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
