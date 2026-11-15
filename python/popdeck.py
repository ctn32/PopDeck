# -*- coding: utf-8 -*-
"""
POPDECK - GIF / PNG / ses kumandasi (OBS gerektirmez)
------------------------------------------------
* Her sey "main" klasorunun icinde toplanir.
* "main" icindeki HER ALT KLASOR bir kategoridir. Arayuzden istedigin kadar klasor
  olusturup her birine bir TUS atayabilirsin.
* Tusa basinca o klasorden rastgele GIF/PNG secilir, ekranin ortasinda seffaf
  zeminle, her zaman ustte, tiklamalari engellemeden oynatilir ve kapanir.
* Gorselle ayni isimli ses (komik.gif + komik.mp3) birlikte calinir.
"""

import os
import re
import sys
import json
import queue
import random
import shutil
import io
import gc
import bisect
import itertools
import base64
import subprocess
import ctypes
import socket
import time
import hmac
import threading
import collections
import tkinter as tk
from tkinter import font as tkfont
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs, quote, unquote
from tkinter import ttk, messagebox, simpledialog, filedialog

import keyboard                                   # global tus dinleme
from PIL import Image, ImageOps, ImageSequence, ImageTk

from diller import CEVIRI, DIL_KODLARI, DIL_ADLARI   # ceviriler: diller/*.json

# =====================================================================
#   SABITLER (cogu ayar arayuzden degisir; burasi ileri seviye icin)
# =====================================================================
ANA_KLASOR_ADI = "main"                  # Her seyin toplandigi ana klasorun adi
try:                                     # Istege bagli: iPhone HEIC/HEIF destegi  (pip install pillow-heif)
    import pillow_heif
    pillow_heif.register_heif_opener()
except Exception:
    pass
_EK_GORSEL = (".jpg", ".jpeg", ".jfif", ".bmp", ".tif", ".tiff", ".ico", ".avif", ".heic", ".heif")
# GIF/PNG/WEBP + Pillow'un bu bilgisayarda acabildigi diger resimler (JPEG, BMP, TIFF, ICO, varsa AVIF/HEIC)
GORSEL_UZANTILARI = (".gif", ".png", ".webp") + tuple(e for e in _EK_GORSEL if e in Image.registered_extensions())
SES_UZANTILARI = (".mp3", ".wav")        # Desteklenen ses turleri
SES_ONBELLEK_ADET = 32                   # Ayni anda hazir (acik) tutulacak en fazla ses dosyasi
SAYDAM_RENK = "#010203"                  # Seffaf yapilan renk
SAYDAM_RGB = (1, 2, 3)
UYARI_SURESI = 3                         # "Klasor bos" uyarisi kac saniye gorunsun

# Betigin bulundugu klasoru baz al
ANA_DIZIN = os.path.dirname(os.path.abspath(sys.executable if getattr(sys, "frozen", False) else __file__))   # .exe ise exe'nin yani
ANA_KLASOR = os.path.join(ANA_DIZIN, ANA_KLASOR_ADI)
AYAR_DOSYASI = os.path.join(ANA_KLASOR, "ayarlar.json")
DECK_DOSYASI = os.path.join(ANA_KLASOR, "deck.json")      # Favoriler, siralama, son oynatilanlar

# Arayuzden degisen genel ayarlarin varsayilanlari
VARSAYILAN_GENEL = {
    "sure": 3.0,                  # GIF kac saniye ekranda kalsin
    "boyut_orani": 1.0,           # 1.0 = tam ekran, 0.6 = ekranin %60'i
    "orani_boz": False,           # True: gorsel gerilip ekrani doldurur
    "fade_in": 0.4,               # Yavasca belirme suresi (sn), 0 = kapali
    "tiklama_gecir": True,        # Tiklamalar arkadaki oyuna gitsin
    "ses_aktif": True,
    "ses_seviyesi": 100,          # 0-100
    "ses_gif_ile_kesil": True,    # GIF kapaninca ses de dursun
    "dinleme": True,              # Tuslar dinleniyor mu
    "gizli_basla": False,         # Program acilinca arayuz gizli baslasin
    "arayuz_tusu": "ctrl+shift+f11",   # Arayuzu goster/gizle
    "cikis_tusu": "ctrl+shift+f12",    # Programi tamamen kapat
    "kapat_tusu": "f10",               # Ekrandaki GIF'i erken kapat (anlik)
    "dil": None,                  # None = Windows dilinden otomatik | tr en de es fr ru
    "ses_gecikme": 0,             # Sesi bu kadar ms GEC baslat (0-500). Ses gorselden erken geliyorsa artir
    "on_yukle": True,             # GIF/PNG ve sesleri acilista arka planda hazirla (ilk gosterim aninda olsun)
    "onbellek_mb": 400,           # Hazir kareler icin en fazla kac MB RAM kullanilsin (200-3000)
    "max_buyutme": 2.0,           # Kucuk GIF'ler en fazla kac kat buyutulsun (RAM tasarrufu). 4.0 = sinirsiz
    "fps_siniri": 30,             # GIF'ler en fazla kac kare/sn ile saklansin (RAM tasarrufu). 60 = sinirsiz
    "bos_sure_dk": 10,            # Bu kadar dakika kullanilmazsa onbellek bosaltilir (0 = hic)
    "ayar_surumu": 2,
    "gif_sonuna_kadar": True,     # True: GIF bitene kadar oynar (sure siniri yok) | False: "sure" kadar
    "fw_otomatik": True,          # Guvenlik duvari izni eksikse otomatik iste
    "web_aktif": True,            # Telefon kumandasi (web sitesi) acik mi
    "web_port": 8080,             # Telefonda acilacak adresin portu
    "web_pin": None,              # None = ilk acilista rastgele 4 haneli PIN uretilir | "" = PIN yok
    "web_yukleme": True,          # Telefon/tarayici sayfasindan dosya yuklemeye izin ver
}

YENIDEN_BOYUTLA = getattr(getattr(Image, "Resampling", Image), "BILINEAR")

# ---------------------------------------------------------------------
#   DIL (Turkce metin = anahtar; diger diller diller.py'deki sozlukten gelir)
# ---------------------------------------------------------------------
DIL = "tr"


def dil_ayarla(kod):
    global DIL
    DIL = kod if kod in DIL_KODLARI else "tr"


def L(metin, *arg):
    """Arayuz metnini secili dile cevirir. {0}, {1} yer tutuculari arg ile dolar."""
    if DIL != "tr":
        ceviri = CEVIRI.get(DIL, {}).get(metin)
        if ceviri:
            metin = ceviri
    return metin.format(*arg) if arg else metin


def sistem_dili():
    """Windows arayuz dilinden varsayilan dili secer (desteklenmiyorsa Ingilizce)."""
    try:
        kimlik = ctypes.windll.kernel32.GetUserDefaultUILanguage() & 0xFF
        return {0x1F: "tr", 0x09: "en", 0x07: "de", 0x0A: "es", 0x0C: "fr", 0x19: "ru"}.get(kimlik, "en")
    except Exception:
        return "tr"


def dil_yukle_erken():
    """Ayar dosyasindaki dili (yoksa sistem dilini) Tk acilmadan once yukler."""
    kod = None
    try:
        with open(AYAR_DOSYASI, "r", encoding="utf-8") as f:
            kod = json.load(f).get("genel", {}).get("dil")
    except (OSError, ValueError, AttributeError):
        pass
    dil_ayarla(kod or sistem_dili())


# =====================================================================
#   AYARLAR (main/ayarlar.json)
# =====================================================================
class Ayarlar:
    def __init__(self):
        self.genel = dict(VARSAYILAN_GENEL)
        self.tuslar = collections.OrderedDict()      # klasor adi -> tus ("" = atanmamis)

    def yukle(self):
        try:
            with open(AYAR_DOSYASI, "r", encoding="utf-8") as f:
                veri = json.load(f)
            for k, v in veri.get("genel", {}).items():
                if k in VARSAYILAN_GENEL:
                    self.genel[k] = v
            if int(veri.get("genel", {}).get("ayar_surumu", 0) or 0) < 2:   # Eski kurulum: RAM dostu degerlere gec
                self.genel["onbellek_mb"] = min(int(self.genel.get("onbellek_mb", 400) or 400), 400)
                self.genel["ayar_surumu"] = 2
            for ad, tus in veri.get("klasorler", {}).items():
                self.tuslar[str(ad)] = str(tus or "")
        except (OSError, ValueError):
            pass

    def kaydet(self):
        try:
            os.makedirs(ANA_KLASOR, exist_ok=True)
            with open(AYAR_DOSYASI, "w", encoding="utf-8") as f:
                json.dump({"genel": self.genel, "klasorler": self.tuslar},
                          f, ensure_ascii=False, indent=2)
        except OSError as hata:
            print("Ayarlar kaydedilemedi:", hata)

    def klasor_esitle(self):
        """main icindeki gercek klasorlerle ayar listesini esitler. Degisiklik olduysa True doner."""
        mevcut = klasor_listesi()
        eski = list(self.tuslar.items())
        yeni = collections.OrderedDict()
        for ad in self.tuslar:                       # eski sirayi koru
            if ad in mevcut:
                yeni[ad] = self.tuslar[ad]
        for ad in mevcut:                            # elle eklenen yeni klasorler
            if ad not in yeni:
                yeni[ad] = ""
        self.tuslar = yeni
        return list(yeni.items()) != eski


def klasor_listesi():
    """main icindeki alt klasorlerin adlari ('.' veya '_' ile baslayanlar gizli sayilir)."""
    try:
        return sorted(
            ad for ad in os.listdir(ANA_KLASOR)
            if os.path.isdir(os.path.join(ANA_KLASOR, ad)) and not ad.startswith((".", "_"))
        )
    except OSError:
        return []


def klasor_dosyalari(ad):
    """Bir klasordeki (gorseller, sesler) listelerini dondurur."""
    yol = os.path.join(ANA_KLASOR, ad)
    gorseller, sesler = [], []
    try:
        for dosya in sorted(os.listdir(yol)):
            tam = os.path.join(yol, dosya)
            if not os.path.isfile(tam):
                continue
            kucuk = dosya.lower()
            if kucuk.endswith(GORSEL_UZANTILARI):
                gorseller.append(tam)
            elif kucuk.endswith(SES_UZANTILARI):
                sesler.append(tam)
    except OSError:
        pass
    return gorseller, sesler


# =====================================================================
#   AKILLI RASTGELE ("karistirilmis torba" yontemi)
# =====================================================================
class AkilliRastgele:
    """
    Sade random.choice yerine: liste once karistirilir, sirayla tek tek dagitilir.
    Torba bitmeden ayni oge ikinci kez cikmaz; torba bitip yeniden karistirilirken
    de son cikan oge hemen basa gelmez. Boylece hem rastgele his korunur hem
    art arda ayni seyin gelmesi onlenir.
    """
    def __init__(self):
        self.kilit = threading.Lock()
        self.torba = {}          # anahtar -> kalan ogeler
        self.kume = {}           # anahtar -> torbanin yapildigi ogeler (dosya eklenip silinirse yenilenir)
        self.son = {}            # anahtar -> en son cikan oge

    def sec(self, anahtar, liste):
        liste = list(liste)
        if not liste:
            return None
        with self.kilit:
            if len(liste) == 1:
                self.son[anahtar] = liste[0]
                return liste[0]
            if self.kume.get(anahtar) != set(liste) or not self.torba.get(anahtar):
                torba = liste[:]
                random.shuffle(torba)
                if torba[-1] == self.son.get(anahtar):       # pop() sondan alir: ilk cikacak oge son cikanla ayni olmasin
                    j = random.randrange(len(torba) - 1)
                    torba[-1], torba[j] = torba[j], torba[-1]
                self.kume[anahtar] = set(liste)
                self.torba[anahtar] = torba
            secilen = self.torba[anahtar].pop()
            self.son[anahtar] = secilen
            return secilen

    def bildir(self, anahtar, oge):
        """Elle (rastgele olmadan) secilen ogeyi 'en son cikan' olarak kaydeder."""
        with self.kilit:
            self.son[anahtar] = oge


RASTGELE = AkilliRastgele()


class DeckDurumu:
    """Telefon Deck'inin kalici durumu: favoriler, siralamalar, son oynatilanlar (main/deck.json).
    Ogeler "klasor/dosya.gif" seklinde kimliklerle tutulur."""
    SON_LIMIT = 30

    def __init__(self, yol):
        self.yol = yol
        self.kilit = threading.RLock()
        self.favoriler = []          # Sirali favori kimlikleri
        self.sira = {}               # klasor -> sirali dosya adlari
        self.son = []                # En yeniden eskiye son oynatilanlar
        self.yukle()

    def yukle(self):
        try:
            with open(self.yol, "r", encoding="utf-8") as f:
                veri = json.load(f)
        except (OSError, ValueError):
            return
        if not isinstance(veri, dict):
            return
        self.favoriler = [x for x in veri.get("favoriler", []) if isinstance(x, str)]
        sira = veri.get("sira", {})
        if isinstance(sira, dict):
            self.sira = {str(k): [str(d) for d in v] for k, v in sira.items() if isinstance(v, list)}
        self.son = [x for x in veri.get("son", []) if isinstance(x, str)][:self.SON_LIMIT]

    def _kaydet(self):
        gecici = self.yol + ".tmp"
        try:
            with open(gecici, "w", encoding="utf-8") as f:
                json.dump({"favoriler": self.favoriler, "sira": self.sira, "son": self.son},
                          f, ensure_ascii=False, indent=1)
            os.replace(gecici, self.yol)
        except OSError as hata:
            print("Deck kaydedilemedi:", hata)

    def anlik(self):
        with self.kilit:
            return list(self.favoriler), {k: list(v) for k, v in self.sira.items()}, list(self.son)

    def favori_ayarla(self, kimlik, deger):
        with self.kilit:
            if deger and kimlik not in self.favoriler:
                self.favoriler.append(kimlik)
            elif not deger and kimlik in self.favoriler:
                self.favoriler.remove(kimlik)
            self._kaydet()

    def favori_sirala(self, idler):
        with self.kilit:
            yeni = [i for i in dict.fromkeys(idler) if i in self.favoriler]
            yeni += [i for i in self.favoriler if i not in yeni]     # listede olmayanlar sona
            self.favoriler = yeni
            self._kaydet()

    def sira_ayarla(self, klasor, dosyalar):
        with self.kilit:
            self.sira[klasor] = list(dict.fromkeys(dosyalar))
            self._kaydet()

    def son_ekle(self, kimlik):
        with self.kilit:
            if kimlik in self.son:
                self.son.remove(kimlik)
            self.son.insert(0, kimlik)
            del self.son[self.SON_LIMIT:]
            self._kaydet()

    def klasor_adi_degisti(self, eski, yeni):
        """Klasorun adi degisince favori/sira/son kayitlari kaybolmasin."""
        with self.kilit:
            degis = lambda k: (yeni + k[len(eski):]) if k.startswith(eski + "/") else k
            self.favoriler = [degis(k) for k in self.favoriler]
            self.son = [degis(k) for k in self.son]
            if eski in self.sira:
                self.sira[yeni] = self.sira.pop(eski)
            self._kaydet()


# =====================================================================
#   SES (Windows'un yerlesik MCI sistemi - ek kutuphane gerekmez)
# =====================================================================
def mci(komut):
    try:
        return ctypes.windll.winmm.mciSendStringW(komut, None, 0, None)
    except Exception:
        return -1


def ses_bul(gorsel, gorseller, sesler):
    """1) Gorselle ayni isimli ses  2) Yoksa, hicbir gorsele ait olmayan seslerden rastgele biri."""
    taban = os.path.splitext(gorsel)[0].lower()
    for s in sesler:
        if os.path.splitext(s)[0].lower() == taban:
            return s
    gorsel_tabanlari = {os.path.splitext(g)[0].lower() for g in gorseller}
    serbest = [s for s in sesler if os.path.splitext(s)[0].lower() not in gorsel_tabanlari]
    return RASTGELE.sec(("ses", os.path.dirname(gorsel)), serbest) if serbest else None


def islem_bellegi_mb():
    """Programin su an kullandigi RAM (Gorev Yoneticisi'ndeki 'Bellek'). Olculemezse None."""
    try:
        import ctypes.wintypes as wt

        class PMC(ctypes.Structure):
            _fields_ = [("cb", wt.DWORD), ("PageFaultCount", wt.DWORD), ("PeakWorkingSetSize", ctypes.c_size_t),
                        ("WorkingSetSize", ctypes.c_size_t), ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaPagedPoolUsage", ctypes.c_size_t), ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaNonPagedPoolUsage", ctypes.c_size_t), ("PagefileUsage", ctypes.c_size_t),
                        ("PeakPagefileUsage", ctypes.c_size_t)]
        k32, psapi = ctypes.windll.kernel32, ctypes.windll.psapi
        k32.GetCurrentProcess.restype = ctypes.c_void_p
        psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(PMC), wt.DWORD]
        c = PMC()
        c.cb = ctypes.sizeof(PMC)
        if not psapi.GetProcessMemoryInfo(k32.GetCurrentProcess(), ctypes.byref(c), c.cb):
            return None
        return c.WorkingSetSize / 1048576.0
    except Exception:
        return None


def bellek_kirp():
    """Bosalan bellegi isletim sistemine geri verir (Gorev Yoneticisi'nde RAM duser)."""
    try:
        gc.collect()
        k32, psapi = ctypes.windll.kernel32, ctypes.windll.psapi
        k32.GetCurrentProcess.restype = ctypes.c_void_p
        psapi.EmptyWorkingSet.argtypes = [ctypes.c_void_p]
        psapi.EmptyWorkingSet(k32.GetCurrentProcess())
    except Exception:
        pass


class SesOnbellegi:
    """
    Ses dosyalarini Windows MCI'de ACIK (hazir) tutar: her ses bir kez acilir, sonra sadece 'bastan cal' denir.
    Boylece ses her seferinde diskten acilmaz, gorselle birlikte aninda baslar. En fazla SES_ONBELLEK_ADET
    dosya acik kalir (eskiler kapanir). Sadece ses dosyalari tutulur.
    """
    def __init__(self):
        self.acik = collections.OrderedDict()            # yol -> mci takma adi
        self.sayac = itertools.count()
        self.sb = None                                   # Son soundboard sesi

    def _ac(self, yol):
        alias = self.acik.get(yol)
        if alias:
            self.acik.move_to_end(yol)
            return alias
        alias = f"ps{next(self.sayac)}"
        if mci(f'open "{yol}" alias {alias}') != 0:
            print("Ses acilamadi:", yol)
            return None
        self.acik[yol] = alias
        while len(self.acik) > SES_ONBELLEK_ADET:        # En eskiyi kapat
            _, eski = self.acik.popitem(last=False)
            mci(f"stop {eski}")
            mci(f"close {eski}")
        return alias

    def isit(self, yol):
        """Sesi onceden acar (calmaz)."""
        return self._ac(yol) is not None

    def hazirla(self, yol, seviye):
        alias = self._ac(yol)
        if alias:
            mci(f"stop {alias}")
            mci(f"setaudio {alias} volume to {max(0, min(100, int(seviye))) * 10}")
        return alias

    def oynat(self, alias):
        if alias:
            mci(f"play {alias} from 0")

    def durdur(self, alias):
        if alias:
            mci(f"stop {alias}")

    def soundboard(self, yol, seviye):
        """Deck soundboard: onceki soundboard sesi kesilir, yenisi calar (GIF sesiyle ust uste calabilir)."""
        self.durdur(self.sb)
        alias = self.hazirla(yol, seviye)
        self.oynat(alias)
        self.sb = alias

    def hepsini_kapat(self):
        for alias in list(self.acik.values()):
            mci(f"stop {alias}")
            mci(f"close {alias}")
        self.acik.clear()
        self.sb = None


SES = SesOnbellegi()


# =====================================================================
#   GORSEL HAZIRLAMA (arka plan thread'inde calisir, ekrani dondurmaz)
# =====================================================================
def hedef_boyut(w, h, ekran_w, ekran_h, oran, boz, max_buyutme, kare_sayisi, butce_bayt):
    """Ekranda gosterilecek kare boyutu. Kucuk GIF'ler gereksiz buyutulmez; cok agir olacaksa otomatik kucultulur."""
    if boz:
        # Gerdirme modu: ekrani TAM doldurur. Kucuk GIF'ler icin max_buyutme siniri UYGULANMAZ.
        hw, hh = ekran_w * oran, ekran_h * oran
        butce_bayt = max(butce_bayt * 4, 2 * 1024 ** 3) if butce_bayt else butce_bayt   # Sadece asiri RAM'e karsi emniyet (2 GB)
    else:
        o = min(ekran_w * oran / w, ekran_h * oran / h)
        hw, hh = w * o, h * o
        if max_buyutme and max_buyutme < 4.0:            # 4.0 = sinirsiz
            hw, hh = min(hw, w * max_buyutme), min(hh, h * max_buyutme)
    tahmin = hw * hh * 4 * max(1, kare_sayisi)           # Tk PhotoImage: 4 bayt / piksel
    if butce_bayt and tahmin > butce_bayt:
        k = (butce_bayt / tahmin) ** 0.5
        hw, hh = hw * k, hh * k
    return max(1, int(round(hw))), max(1, int(round(hh)))


def gorsel_kareleri(yol, ekran_w, ekran_h, oran, boz, max_buyutme, fps, butce_bayt):
    """
    Gorselin karelerini TEK TEK uretir (hepsini ayni anda RAM'de tutmaz): (PIL kare, sure_ms).
    RAM tasarrufu: fps_siniri'ndan hizli kareler birlestirilir, birbirinin ayni kareler tek kareye indirilir.
    """
    img = Image.open(yol)
    try:
        if getattr(img, "n_frames", 1) == 1:
            img = ImageOps.exif_transpose(img)           # Fotograflar (JPEG): yonu EXIF'e gore duzelt
        w, h = img.size
        n = getattr(img, "n_frames", 1)
        yeni = hedef_boyut(w, h, ekran_w, ekran_h, oran, boz, max_buyutme, n, butce_bayt)
        min_sure = 1000.0 / max(1, fps) if (n > 1 and fps < 60) else 0
        bekleyen = None                                  # [kare, toplam_sure, ozet]
        for kare in ImageSequence.Iterator(img):         # animasyonun her karesi
            sure = max(20, int(kare.info.get("duration") or 100))
            if bekleyen is not None and bekleyen[1] < min_sure:
                bekleyen[1] += sure                      # fps siniri: bu kareyi atla, oncekinin suresini uzat
                continue
            k = kare.convert("RGBA").resize(yeni, YENIDEN_BOYUTLA)
            # Yari seffaf kenarlar yesilimsi cizgi yapmasin diye alfa'yi 0/255 yap
            alfa = k.getchannel("A").point(lambda p: 255 if p >= 128 else 0)
            zemin = Image.new("RGB", yeni, SAYDAM_RGB)
            zemin.paste(k.convert("RGB"), (0, 0), alfa)  # seffaf yerler SAYDAM_RENK olur
            ozet = hash(zemin.tobytes()) if n > 1 else 0
            if bekleyen is not None and ozet == bekleyen[2]:
                bekleyen[1] += sure                      # Oncekiyle ayni kare: ayri saklama
                continue
            if bekleyen is not None:
                yield bekleyen[0], bekleyen[1]
            bekleyen = [zemin, sure, ozet]
        if bekleyen is not None:
            yield bekleyen[0], bekleyen[1]
    finally:
        img.close()


class GifVeri:
    """Bir gorselin ekrana hazir hali (onbellek ogesi): Tk PhotoImage kareleri + sureleri. Baska bir sey tutulmaz."""
    def __init__(self):
        self.fotolar = []
        self.sureler = []
        self.boyut = (1, 1)
        self.n = 0
        self.atildi = False                              # Onbellekten atildi mi?

    def ekle(self, foto, sure, boyut):
        self.fotolar.append(foto)
        self.sureler.append(sure)
        self.boyut = boyut
        self.n += 1

    def bayt(self):
        w, h = self.boyut
        return w * h * 4 * self.n                        # Tk PhotoImage: 4 bayt / piksel

    def foto(self, i, master=None):
        return self.fotolar[i]


class Is:
    """Arka planda calisan bir gorsel hazirlama isi (kareler tek tek ana thread'e gelir)."""
    def __init__(self, anahtar, yol, ses, onyuk):
        self.anahtar, self.yol, self.ses, self.onyuk = anahtar, yol, ses, onyuk
        self.veri = GifVeri()
        self.iptal = False
        self.tasti = False                               # On yuklemede bellek limiti asildi mi?
        self.sema = threading.Semaphore(3)               # En fazla 3 kare havada: RAM sismesin


class Oturum:
    """Su an ekranda oynayan tek bir gosterim."""
    def __init__(self, pencere, etiket, veri):
        self.pencere = pencere
        self.etiket = etiket
        self.veri = veri
        self.baslangiclar = []                           # Her karenin basladigi an (ms)
        toplam = 0
        for sure in veri.sureler:
            self.baslangiclar.append(toplam)
            toplam += sure
        self.toplam = max(1, toplam)                     # Animasyonun toplam suresi (ms)
        self.t0 = 0.0                                    # Ilk kare ekrana geldigi an
        self.gosterilen = 0
        self.ses = None                                  # Bu gosterimin ses takma adi
        self.tek_tur = False                             # True: animasyon bir kez sonuna kadar oynar, sonra kapanir


# =====================================================================
#   OYNATICI (seffaf, ustte duran, tiklanamaz pencere)
# =====================================================================
class GifOynatici:
    def __init__(self, root, ayar, kuyruk, on_oynat=None):
        self.root = root
        self.ayar = ayar
        self.kuyruk = kuyruk
        self.on_oynat = on_oynat                         # Bir gorsel ekrana gelince cagrilir (son oynatilanlar icin)
        self.ekran_w, self.ekran_h = self.ekran_boyutu(root)
        self.onbellek = collections.OrderedDict()        # anahtar -> GifVeri (ekrana hazir kareler)
        self.isler = {}                                  # anahtar -> calisan hazirlama isi
        self.onyuk_kuyrugu = collections.deque()         # On yuklenecek (anahtar, yol)
        self.onyuk_bekleyen = set()
        self.onyuk_is = None                             # Calisan on yukleme isi (tek tek)
        self.onyuk_dur = False                           # Bellek dolunca on yuklemeyi durdur
        self.son_kullanim = time.time()                  # Bosta bellek bosaltma icin
        self.sigmayanlar = set()                         # Bellege sigmayan / acilamayan gorseller
        self.oturum = None
        self.mesgul = False                              # Yukleniyor ya da oynuyor mu?
        self.iptal = False                               # Yukleme sirasinda "durdur"a basildi mi?
        self.uyari_pencere = None

    @staticmethod
    def ekran_boyutu(root):
        """Ana ekranin GERCEK piksel boyutu (Windows olcekleme/DPI ne olursa olsun)."""
        try:
            u = ctypes.windll.user32
            w, h = u.GetSystemMetrics(0), u.GetSystemMetrics(1)
            if w > 0 and h > 0:
                return w, h
        except Exception:
            pass
        return root.winfo_screenwidth(), root.winfo_screenheight()

    # ---------------- tetikleme ----------------
    def tetikle(self, klasor, yol=None, kes=False):
        """
        Bir klasorun tusuna basilinca cagrilir.
        yol verilirse rastgele secim yerine o dosya oynatilir (Deck sekmesi).
        kes=True ise oynayan GIF varsa hemen kapatilip yenisi acilir.
        """
        if self.mesgul:
            if kes and self.oturum is not None:
                self.kapat(self.oturum)
            else:
                return
        self.iptal = False
        gorseller, sesler = klasor_dosyalari(klasor)
        if not gorseller:
            self.uyari_goster(L("Lütfen '{0}' klasörüne bir görsel yükleyin!", klasor))
            return

        g = self.ayar.genel
        if yol in gorseller:
            RASTGELE.bildir(klasor, yol)                 # Elle secilen dosya
        else:
            yol = RASTGELE.sec(klasor, gorseller)        # AKILLI RASTGELE SECIM (art arda tekrar etmez)
        ses = ses_bul(yol, gorseller, sesler) if g["ses_aktif"] else None
        self.son_kullanim = time.time()
        try:
            mtime = os.path.getmtime(yol)
        except OSError:
            return
        anahtar = self.anahtar_yap(yol, mtime)

        if anahtar in self.onbellek:                     # Hazirsa aninda ac
            self.onbellek.move_to_end(anahtar)
            self.baslat(self.onbellek[anahtar], ses, yol)
        else:                                            # Ilk kez: arka planda hazirla
            self.mesgul = True
            self.isler.pop(anahtar, None)
            self.is_baslat(anahtar, yol, ses, onyuk=False)

    def ayar_parametreleri(self):
        g = self.ayar.genel
        return (round(float(g["boyut_orani"]), 3), bool(g["orani_boz"]),
                round(float(g.get("max_buyutme", 2.0)), 1), int(g.get("fps_siniri", 30)))

    def anahtar_yap(self, yol, mtime):
        return (yol, mtime, self.ekran_w, self.ekran_h) + self.ayar_parametreleri()

    # ---------------- gorsel hazirlama isleri (arka plan thread'i -> kare kare ana thread) ----------------
    def is_baslat(self, anahtar, yol, ses, onyuk):
        is_ = Is(anahtar, yol, ses, onyuk)
        self.isler[anahtar] = is_
        oran, boz, maxb, fps = self.ayar_parametreleri()
        butce = self.limit_bayt() // 2                   # Tek gorsel butcenin yarisini asmasin (gerekirse kucultulur)
        threading.Thread(target=self._is_thread, args=(is_, oran, boz, maxb, fps, butce), daemon=True).start()
        return is_

    def _is_thread(self, is_, oran, boz, maxb, fps, butce):
        try:
            for pil, sure in gorsel_kareleri(is_.yol, self.ekran_w, self.ekran_h, oran, boz, maxb, fps, butce):
                if is_.iptal:
                    break
                is_.sema.acquire()
                if is_.iptal:
                    break
                self.kuyruk.put(("kare", is_, pil, sure))
            self.kuyruk.put(("is_bitti", is_, None))
        except Exception as hata:
            self.kuyruk.put(("is_bitti", is_, str(hata)))

    def kare_geldi(self, komut):
        """(Ana thread) Gelen kareyi ekrana hazir hale getirir; ham kare hemen serbest kalir."""
        _, is_, pil, sure = komut
        try:
            if not is_.iptal:
                is_.veri.ekle(ImageTk.PhotoImage(pil, master=self.root), sure, pil.size)
                if is_.onyuk and self.toplam_bayt() + is_.veri.bayt() > self.limit_bayt():
                    is_.iptal = is_.tasti = True         # On yukleme bellegi asiyor: birak
        except Exception as hata:
            print("Kare hazirlanamadi:", hata)
            is_.iptal = True
        finally:
            is_.sema.release()

    def is_bitti(self, komut):
        _, is_, hata = komut
        self.isler.pop(is_.anahtar, None)
        veri = is_.veri
        if is_.onyuk:                                    # ----- on yukleme isi -----
            self.onyuk_is = None
            if hata is None and not is_.iptal and veri.n:
                if not self.onbellege_ekle(is_.anahtar, veri, onyuk=True):
                    self.sigmayanlar.add(is_.anahtar)
                    self.onyuk_dur = True
            elif is_.tasti:
                self.sigmayanlar.add(is_.anahtar)
                self.onyuk_dur = True                    # Bellek doldu: daha fazla yukleme
            elif hata:
                print("On yukleme atlandi:", is_.yol, hata)
                self.sigmayanlar.add(is_.anahtar)
            self.onyuk_ilerlet()
            return
        # ----- kullanicinin tetikledigi is -----
        if hata is not None or not veri.n:
            print("Gorsel acilamadi:", is_.yol, hata)
            self.mesgul = False
            return
        self.onbellege_ekle(is_.anahtar, veri)           # Sigmazsa yine de bu sefer oynatilir
        if self.iptal:                                   # Yuklenirken durdur'a basilmisti: gosterme
            self.iptal = False
            self.mesgul = False
            return
        self.baslat(veri, is_.ses, is_.yol)

    # ---------------- onbellek (sadece gorsel kareleri) ----------------
    def limit_bayt(self):
        return int(self.ayar.genel.get("onbellek_mb", 400)) * 1024 * 1024

    def toplam_bayt(self):
        return sum(v.bayt() for v in self.onbellek.values())

    def onbellege_ekle(self, anahtar, veri, onyuk=False):
        """True: onbellege alindi. Onyuklemede bellek dolarsa eskileri ATMAZ (reddeder)."""
        limit = self.limit_bayt()
        if veri.bayt() > limit:
            return False
        if onyuk and self.toplam_bayt() + veri.bayt() > limit:
            return False
        self.onbellek[anahtar] = veri
        while len(self.onbellek) > 1 and self.toplam_bayt() > limit:
            _, eski = self.onbellek.popitem(last=False)  # en eskiyi at
            eski.atildi = True
        return True

    def temizle(self):
        """Onbellegi bosaltir (calisan on yuklemeyi de durdurur)."""
        for v in self.onbellek.values():
            v.atildi = True
        self.onbellek.clear()
        for is_ in list(self.isler.values()):
            if is_.onyuk:
                is_.iptal = True
        self.onyuk_kuyrugu.clear()
        self.onyuk_bekleyen.clear()
        self.sigmayanlar.clear()
        self.onyuk_dur = False

    def bosta_mi(self, dakika):
        return (not self.mesgul and not self.isler and bool(self.onbellek)
                and time.time() - self.son_kullanim > dakika * 60)

    def bosalt(self):
        """Uzun suredir kullanilmadi: onbellegi bosalt, on yuklemeyi yeniden baslatma."""
        self.temizle()
        self.onyuk_dur = True

    def limit_degisti(self):
        """Limit/ayar degisince: on yukleme yeniden denenir, limit asildiysa en eskiler atilir."""
        self.onyuk_dur = False
        self.sigmayanlar.clear()
        limit = self.limit_bayt()
        while self.onbellek and self.toplam_bayt() > limit:
            _, eski = self.onbellek.popitem(last=False)
            eski.atildi = True

    def eskileri_at(self, gecerli_yollar):
        """Silinen/yeniden adlandirilan dosyalarin onbellek girdilerini atar."""
        for anahtar in [k for k in self.onbellek if k[0] not in gecerli_yollar]:
            self.onbellek.pop(anahtar).atildi = True

    def ozet(self, tum_yollar):
        """(kullanilan_mb, limit_mb, hazir_adet, toplam_adet) - arayuzdeki durum yazisi icin."""
        return (round(self.toplam_bayt() / 1048576), round(self.limit_bayt() / 1048576),
                len(self.onbellek), len(tum_yollar))

    # ---------------- on yukleme: bir seferde TEK gorsel, arka planda ----------------
    def onyukle_tetikle(self, yollar):
        if self.onyuk_dur or not self.ayar.genel.get("on_yukle", True):
            return
        for yol in yollar:
            try:
                anahtar = self.anahtar_yap(yol, os.path.getmtime(yol))
            except OSError:
                continue
            if (anahtar in self.onbellek or anahtar in self.sigmayanlar or anahtar in self.isler
                    or anahtar in self.onyuk_bekleyen):
                continue
            self.onyuk_kuyrugu.append((anahtar, yol))
            self.onyuk_bekleyen.add(anahtar)
        self.onyuk_ilerlet()

    def onyuk_ilerlet(self):
        while self.onyuk_is is None and self.onyuk_kuyrugu and not self.onyuk_dur:
            anahtar, yol = self.onyuk_kuyrugu.popleft()
            self.onyuk_bekleyen.discard(anahtar)
            if anahtar in self.onbellek or anahtar in self.isler:
                continue
            self.onyuk_is = self.is_baslat(anahtar, yol, None, True)
        if self.onyuk_dur:
            self.onyuk_kuyrugu.clear()
            self.onyuk_bekleyen.clear()

    # ---------------- gosterim ----------------
    def baslat(self, veri, ses, yol=None):
        g = self.ayar.genel
        self.mesgul = True
        try:
            w, h = veri.boyut
            x = (self.ekran_w - w) // 2                  # Yatayda ortala
            y = (self.ekran_h - h) // 2                  # Dikeyde ortala
            ilk = veri.fotolar[0]
            self.son_kullanim = time.time()

            # Sesi ONCEDEN hazirla (acik tutulan ses): gorsel gelince gecikmeden baslar
            ses_alias = SES.hazirla(ses, g["ses_seviyesi"]) if (ses and g["ses_aktif"]) else None

            pencere = tk.Toplevel(self.root)
            pencere.withdraw()                           # Hazir olana kadar gizli
            pencere.overrideredirect(True)               # Cerceve ve butonlar yok
            pencere.configure(bg=SAYDAM_RENK)
            pencere.attributes("-transparentcolor", SAYDAM_RENK)   # Arka plan seffaf
            pencere.attributes("-topmost", True)         # Her zaman en ustte
            pencere.geometry(f"{w}x{h}+{x}+{y}")
            if g["fade_in"] > 0:
                pencere.attributes("-alpha", 0.0)

            etiket = tk.Label(pencere, bg=SAYDAM_RENK, bd=0, highlightthickness=0, padx=0, pady=0)
            etiket.place(x=0, y=0, width=w, height=h)
            oturum = Oturum(pencere, etiket, veri)
            oturum.tek_tur = bool(g["gif_sonuna_kadar"]) and veri.n > 1
            oturum.ses = ses_alias
            etiket.configure(image=ilk)
            pencere.deiconify()
            pencere.update_idletasks()                   # Ilk kare gercekten cizilsin
            self.tiklama_gecir(pencere)

            self.oturum = oturum
            oturum.t0 = time.perf_counter()              # Saat ILK KARE EKRANA GELINCE baslar
            if ses_alias:                                # Ses tam bu anda baslar (istenirse biraz gecikmeli)
                gecikme = max(0, int(g.get("ses_gecikme", 0) or 0))
                if gecikme:
                    self.root.after(gecikme, lambda: SES.oynat(ses_alias) if self.oturum is oturum else None)
                else:
                    SES.oynat(ses_alias)
            self.fade_in(oturum)
            self.animasyon(oturum)
            if not oturum.tek_tur:                       # Sabit gorsel (PNG) ya da sure modu: zamanlayici ile kapat
                self.root.after(int(float(g["sure"]) * 1000), lambda: self.kapat(oturum))
            if self.on_oynat and yol:
                try:
                    self.on_oynat(yol)
                except Exception as hata:
                    print("Son oynatilan kaydedilemedi:", hata)
        except Exception as hata:
            print("Gosterim hatasi:", hata)
            self.mesgul = False

    def tiklama_gecir(self, pencere):
        """Pencereyi tiklanamaz yapar, oyunun odagini calmasini engeller (Windows)."""
        if not self.ayar.genel["tiklama_gecir"]:
            return
        try:
            user32 = ctypes.windll.user32
            hwnd = user32.GetParent(pencere.winfo_id()) or pencere.winfo_id()
            GWL_EXSTYLE = -20
            stil = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
            stil |= 0x00080000 | 0x00000020 | 0x00000080 | 0x08000000
            # LAYERED | TRANSPARENT (tiklama gecirir) | TOOLWINDOW (gorev cubugunda yok) | NOACTIVATE (odagi calmaz)
            user32.SetWindowLongW(hwnd, GWL_EXSTYLE, stil)
        except Exception as hata:
            print("Tiklama gecirme ayarlanamadi:", hata)

    def fade_in(self, oturum):
        sure = float(self.ayar.genel["fade_in"])
        if sure <= 0:
            return
        adim_ms = 20
        toplam = max(1, int(sure * 1000 / adim_ms))

        def adim(i):
            if self.oturum is not oturum:
                return
            try:
                oturum.pencere.attributes("-alpha", min(1.0, i / toplam))
            except tk.TclError:
                return
            if i < toplam:
                self.root.after(adim_ms, lambda: adim(i + 1))
        adim(1)

    def animasyon(self, oturum):
        """
        ZAMAN TABANLI oynatma: gosterilecek kare, ilk karenin ekrana geldigi andan beri GECEN GERCEK SUREYE gore secilir.
        Buyuk GIF'lerde kare cizimi yavaslarsa kare ATLANIR; boylece GIF ne uzar ne kisalir, ses ile senkron kalir.
        Kareler onbellekten (hazir PhotoImage) alinir; hazir degilse o an uretilir.
        """
        if self.oturum is not oturum:
            return
        veri = oturum.veri
        if veri.n == 1:                                  # Tek kareli gorsel (PNG): animasyon yok
            return
        gecen = (time.perf_counter() - oturum.t0) * 1000
        if oturum.tek_tur and gecen >= oturum.toplam:    # GIF sonuna kadar oynadi: kapat
            self.kapat(oturum)
            return
        dongude = gecen % oturum.toplam
        sira = bisect.bisect_right(oturum.baslangiclar, dongude) - 1
        if sira != oturum.gosterilen:
            oturum.etiket.configure(image=veri.foto(sira, self.root))
            oturum.gosterilen = sira
        bitis = oturum.baslangiclar[sira] + veri.sureler[sira]
        self.root.after(max(1, int(bitis - dongude)), lambda: self.animasyon(oturum))

    def durdur(self):
        """Erken kapatma: ekrandaki gorseli ve sesini hemen kapatir."""
        if self.oturum is not None:
            oturum = self.oturum
            self.kapat(oturum)
            SES.durdur(oturum.ses)
        elif self.mesgul:
            self.iptal = True                            # Hala yukleniyor: hazir olunca gosterme

    def kapat(self, oturum):
        """Sure dolunca pencereyi tamamen kapatir; eksik kalan kareleri bos zamanda hazirlamaya devam eder."""
        if self.oturum is not oturum:
            return
        self.oturum = None
        self.mesgul = False
        if self.ayar.genel["ses_gif_ile_kesil"]:
            SES.durdur(oturum.ses)
        try:
            oturum.pencere.destroy()
        except tk.TclError:
            pass
        self.son_kullanim = time.time()

    # ---------------- uyari ----------------
    def uyari_goster(self, mesaj):
        print(mesaj)
        if self.uyari_pencere is not None:
            return
        p = tk.Toplevel(self.root)
        p.overrideredirect(True)
        p.attributes("-topmost", True)
        p.configure(bg="#1e1e1e")
        tk.Label(p, text=mesaj, fg="#ff4d4d", bg="#1e1e1e",
                 font=("Segoe UI", 22, "bold"), padx=30, pady=20).pack()
        p.update_idletasks()
        p.geometry(f"+{(self.ekran_w - p.winfo_width()) // 2}+{(self.ekran_h - p.winfo_height()) // 2}")
        self.uyari_pencere = p

        def kapat_uyari():
            try:
                p.destroy()
            except tk.TclError:
                pass
            self.uyari_pencere = None
        self.root.after(UYARI_SURESI * 1000, kapat_uyari)


# =====================================================================
#   TELEFON KUMANDASI (ayni Wi-Fi uzerinden calisan mini web sitesi)
# =====================================================================
WEB_SAYFA = r"""<!doctype html>
<html lang="tr"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0f1115">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<title>PopDeck</title>
<style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
[hidden]{display:none!important}
body{margin:0;background:#0f1115;color:#f2f3f5;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
  padding:env(safe-area-inset-top) 14px calc(env(safe-area-inset-bottom) + 24px);user-select:none;-webkit-user-select:none}
header{display:flex;align-items:center;justify-content:space-between;padding:16px 2px 10px}
h1{font-size:20px;margin:0}
#durum{font-size:13px;color:#9aa0a6;display:flex;align-items:center;gap:6px}
#durdur{border:0;border-radius:999px;padding:9px 15px;font-size:14px;font-weight:700;background:#e5484d;color:#fff;margin-left:auto;margin-right:12px}
#durdur:active{transform:scale(.94)}
#nokta{width:10px;height:10px;border-radius:50%;background:#e5484d}
#nokta.acik{background:#30a46c}
.sekmeler{display:flex;gap:6px;background:#161922;padding:5px;border-radius:16px;margin:2px 0 14px;position:sticky;top:6px;z-index:5;box-shadow:0 6px 14px #0f1115}
.sekme{flex:1;border:0;border-radius:12px;padding:12px 4px;font-size:14px;font-weight:700;background:transparent;color:#9aa0a6}
.sekme.secili{background:#3b5bdb;color:#fff}
#liste{display:grid;grid-template-columns:1fr 1fr;gap:12px}
button.k{border:0;border-radius:18px;padding:22px 10px;background:#1c2029;color:#f2f3f5;font-size:18px;font-weight:700;
  min-height:104px;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;
  box-shadow:0 2px 0 #00000055;transition:transform .08s,background .15s}
button.k small{font-weight:400;font-size:12px;color:#9aa0a6}
button.k:active,button.k.basildi{transform:scale(.95);background:#3b5bdb}
button.k.bos{opacity:.45}
button.rastgele{grid-column:1/-1;background:linear-gradient(135deg,#6741d9,#3b5bdb);min-height:92px}
.bosmesaj{grid-column:1/-1;text-align:center;color:#9aa0a6;padding:30px 10px;line-height:1.5}
#chips{display:flex;gap:8px;overflow-x:auto;padding:2px 0 10px;scrollbar-width:none}
#chips::-webkit-scrollbar{display:none}
.chip{flex:none;border:0;border-radius:999px;padding:9px 14px;font-size:14px;background:#1c2029;color:#cfd3d8}
.chip.secili{background:#3b5bdb;color:#fff}
#arac{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:0 2px 10px;min-height:40px}
#ipucu{font-size:12px;color:#9aa0a6;line-height:1.35}
#duzenle{flex:none;border:0;border-radius:999px;padding:8px 14px;font-size:13px;font-weight:700;background:#1c2029;color:#cfd3d8}
#deck.duzen #duzenle{background:#f08c00;color:#111}
#izgara{display:grid;grid-template-columns:repeat(auto-fill,minmax(98px,1fr));gap:10px}
.tile{position:relative;aspect-ratio:1/1;border:2px solid #2a2f3b;border-radius:16px;background:#1c2029;padding:0;overflow:hidden;
  transition:transform .08s,border-color .15s;cursor:pointer}
.tile img{width:100%;height:100%;object-fit:contain;padding:8px;pointer-events:none;display:block}
.tile.ses{background:linear-gradient(135deg,#1d3b3f,#1c2029)}
.tile .ikon{width:100%;height:100%;display:flex;align-items:center;justify-content:center;font-size:38px;padding-bottom:14px;pointer-events:none}
.tile .ad{position:absolute;left:0;right:0;bottom:0;font-size:10px;padding:12px 4px 4px;background:linear-gradient(transparent,#000c);
  color:#e8eaed;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;text-align:center;pointer-events:none}
.tile .rozet{position:absolute;top:5px;right:5px;font-size:9px;font-weight:700;background:#000a;color:#fff;border-radius:6px;padding:2px 5px;pointer-events:none}
.tile .yildiz{position:absolute;left:2px;top:2px;font-size:21px;line-height:1;padding:4px 6px;color:#ffffff80;text-shadow:0 1px 3px #000}
.tile .yildiz.acik{color:#ffd43b}
.tile .tutamac{display:none;position:absolute;right:4px;top:4px;width:34px;height:34px;align-items:center;justify-content:center;
  background:#000b;border-radius:9px;font-size:20px;color:#fff;touch-action:none}
#deck.duzen .tutamac{display:flex}
#deck.duzen .rozet{display:none}
#deck.duzen .tile{border-style:dashed;border-color:#f08c00}
.tile:active,.tile.basildi{transform:scale(.93);border-color:#748ffc}
#deck.duzen .tile:active{transform:none;border-color:#f08c00}
.tile.yer{opacity:.35}
.tile.hedef{border-color:#ffd43b!important;border-style:solid!important;transform:scale(.95)}
.klon{position:fixed;z-index:50;pointer-events:none;opacity:.93;transform:scale(1.07);box-shadow:0 12px 26px #000a;margin:0}
#toast{position:fixed;left:50%;bottom:calc(env(safe-area-inset-bottom) + 24px);transform:translate(-50%,40px);
  background:#30a46c;color:#fff;padding:12px 18px;border-radius:14px;font-size:15px;opacity:0;transition:.25s;max-width:90%;text-align:center;pointer-events:none;z-index:60}
#toast.goster{opacity:1;transform:translate(-50%,0)}
#toast.hata{background:#e5484d}
#pinkutu{position:fixed;inset:0;background:#0f1115ee;display:flex;align-items:center;justify-content:center;padding:20px;z-index:70}
#pinkutu div{background:#1c2029;border-radius:18px;padding:22px;width:100%;max-width:340px;text-align:center}
#pinkutu input{width:100%;font-size:26px;text-align:center;letter-spacing:6px;padding:12px;border-radius:12px;border:1px solid #333a47;
  background:#0f1115;color:#fff;margin:12px 0}
#pinkutu button{width:100%;border:0;border-radius:12px;padding:14px;font-size:17px;font-weight:700;background:#3b5bdb;color:#fff}
#pinhata{color:#ff8a8e;min-height:20px;font-size:14px}
.ykart{background:#1c2029;border-radius:18px;padding:16px;display:flex;flex-direction:column;gap:12px}
.ykart label{font-size:13px;color:#9aa0a6}
.ykart select,.ykart input[type=text]{width:100%;padding:13px;font-size:16px;border-radius:12px;border:1px solid #333a47;background:#0f1115;color:#fff;user-select:text;-webkit-user-select:text}
.ybuton{border:0;border-radius:16px;padding:18px 12px;font-size:17px;font-weight:700;color:#fff;background:linear-gradient(135deg,#6741d9,#3b5bdb);display:flex;flex-direction:column;gap:4px;align-items:center}
.ybuton small{font-weight:400;font-size:12px;opacity:.85}
.yikinci{border:1px solid #333a47;border-radius:14px;padding:13px;font-size:15px;font-weight:600;color:#cfd3d8;background:transparent}
.ybirak{border:2px dashed #3a4256;border-radius:16px;padding:20px 12px;text-align:center;color:#9aa0a6;font-size:14px;line-height:1.45}
.ybirak.uzerinde{border-color:#748ffc;background:#1f2540}
#yapistir{min-height:46px;border:1px solid #333a47;border-radius:12px;padding:12px;color:#e8eaed;font-size:14px;background:#0f1115;user-select:text;-webkit-user-select:text;outline:none}
#yapistir:empty:before{content:attr(data-ph);color:#6b7280}
#yuklemeler{display:flex;flex-direction:column;gap:8px}
.ysatir{background:#161922;border-radius:12px;padding:10px 12px;font-size:14px}
.yad{display:flex;justify-content:space-between;gap:10px}
.yad span:first-child{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.ybar{height:6px;background:#2a2f3b;border-radius:4px;margin-top:8px;overflow:hidden}
.ybar i{display:block;height:100%;width:0;background:#3b5bdb;transition:width .15s}
.ysatir.ok .ybar i{background:#30a46c;width:100%}
.ysatir.hata .ybar i{background:#e5484d;width:100%}
.ysonuc{font-size:12px;color:#9aa0a6;margin-top:6px}
.ysatir.hata .ysonuc{color:#ff8a8e}
</style></head><body>
<header><h1>PopDeck</h1><button id="durdur" data-t>⏹ Durdur</button><div id="durum"><span id="nokta"></span><span id="durumyazi" data-t>bağlanıyor</span></div></header>
<nav class="sekmeler">
  <button class="sekme" data-s="rastgele" data-t>🎲 Rastgele</button>
  <button class="sekme" data-s="deck" data-t>🎛 Deck</button>
  <button class="sekme" data-s="yukle" data-t>📤 Yükle</button>
</nav>
<section id="rastgele"><main id="liste"></main></section>
<section id="deck" hidden>
  <div id="chips"></div>
  <div id="arac"><span id="ipucu"></span><button id="duzenle">✎ Düzenle</button></div>
  <div id="izgara"></div>
</section>
<section id="yukle" hidden>
  <div class="ykart">
    <label data-t>Hedef klasör</label>
    <select id="hedef"></select>
    <input id="yeniad" type="text" hidden autocomplete="off">
    <button id="dosyasec" class="ybuton"><span data-t>📁 Dosya seç</span><small data-t>galeri veya dosya yöneticisi</small></button>
    <button id="klasorsec" class="yikinci" data-t>📂 Klasörden yükle</button>
    <div id="birak" class="ybirak" data-t>Dosyaları buraya sürükle • ya da Ctrl+V ile panodaki resmi yapıştır</div>
    <div id="yapistir" contenteditable="true"></div>
    <div id="yuklemeler"></div>
  </div>
  <input type="file" id="girdi" multiple hidden accept="image/*,audio/*,.gif,.png,.webp,.mp3,.wav">
  <input type="file" id="girdi2" multiple hidden webkitdirectory>
</section>
<div id="toast"></div>
<div id="pinkutu" hidden><div>
  <h2 style="margin:0 0 4px" data-t>PIN gir</h2>
  <span style="color:#9aa0a6;font-size:14px" data-t>Bilgisayardaki uygulamada yazan PIN</span>
  <input id="pingiris" type="password" inputmode="text" autocomplete="off">
  <div id="pinhata"></div>
  <button id="pinbtn" data-t>Giriş</button>
</div></div>
<script>
const $ = s => document.querySelector(s);
const D = /*DIL*/{};
const T = s => (D[s] !== undefined ? D[s] : s);
document.querySelectorAll('[data-t]').forEach(e => { e.textContent = T(e.textContent.trim()); });
let pin = localStorage.getItem('gifpin') || '';
(function(){
  const p = new URLSearchParams(location.search).get('pin');
  if (p !== null) { pin = p; localStorage.setItem('gifpin', pin); history.replaceState(null, '', location.pathname); }
})();
let sekme = ['deck', 'yukle'].indexOf(localStorage.getItem('gifsekme')) >= 0 ? localStorage.getItem('gifsekme') : 'rastgele';
let chipSecim = localStorage.getItem('gifchip2') || '';
let deckVeri = { klasorler: [], favoriler: [], son: [] }, harita = {}, deckImza = '', toastZaman;
let duzenModu = false, suruk = null;

function toast(m, hata){
  const t = $('#toast'); t.textContent = m; t.className = 'goster' + (hata ? ' hata' : '');
  clearTimeout(toastZaman); toastZaman = setTimeout(() => t.className = '', 2200);
}
function durum(acik, yazi){ $('#nokta').className = acik ? 'acik' : ''; $('#durumyazi').textContent = yazi || T(acik ? 'bağlı' : 'bağlantı yok'); }
function pinSor(mesaj){ $('#pinhata').textContent = mesaj || ''; $('#pinkutu').hidden = false; setTimeout(() => $('#pingiris').focus(), 50); }
async function api(yol, govde){
  const o = { headers: { 'X-Pin': encodeURIComponent(pin) } };
  if (govde) { o.method = 'POST'; o.headers['Content-Type'] = 'application/json'; o.body = JSON.stringify(govde); }
  const r = await fetch(yol, o);
  let v = {}; try { v = await r.json(); } catch (e) {}
  if (r.status === 401 || r.status === 429) { pinSor(pin ? v.hata : ''); throw new Error('yetki'); }
  return v;
}
async function gonder(ad, dugme, dosya){
  if (navigator.vibrate) navigator.vibrate(30);
  dugme.classList.add('basildi'); setTimeout(() => dugme.classList.remove('basildi'), 250);
  try {
    const govde = { klasor: ad }; if (dosya) govde.dosya = dosya;
    const v = await api('/api/tetik', govde);
    toast(v.ok ? T('Gönderildi:') + ' ' + v.klasor : (v.hata || T('Hata')), !v.ok);
    if (v.ok && sekme === 'deck') setTimeout(deckYukle, 400);
  } catch (e) { if (e.message !== 'yetki') { toast(T('Bağlantı hatası'), true); durum(false); } }
}
function bosMesaj(kutu, yazi){ const d = document.createElement('div'); d.className = 'bosmesaj'; d.textContent = yazi; kutu.appendChild(d); }

/* ---------- Rastgele sekmesi ---------- */
function dugmeYap(ad, alt, sinif, tiklama){
  const b = document.createElement('button'); b.className = 'k ' + (sinif || '');
  const a = document.createElement('span'); a.textContent = ad; b.appendChild(a);
  if (alt) { const s = document.createElement('small'); s.textContent = alt; b.appendChild(s); }
  b.addEventListener('click', () => tiklama(b)); return b;
}
function ciz(klasorler){
  const l = $('#liste'); l.textContent = '';
  if (!klasorler.length) { bosMesaj(l, T('Henüz klasör yok. Bilgisayardaki uygulamadan klasör ekle.')); return; }
  l.appendChild(dugmeYap(T('🎲 Karışık'), T('tüm görsellerden rastgele'), 'rastgele', b => gonder('__rastgele__', b)));
  klasorler.forEach(k => {
    const alt = (k.tus ? k.tus + ' • ' : '') + k.gorsel + ' ' + T('görsel') + (k.ses ? ' • ' + k.ses + ' ' + T('ses') : '');
    l.appendChild(dugmeYap(k.ad, alt, k.gorsel ? '' : 'bos', b => gonder(k.ad, b)));
  });
}
async function yukle(){
  if (!$('#pinkutu').hidden) return;
  try { const v = await api('/api/klasorler'); ciz(v.klasorler); durum(true); }
  catch (e) { if (e.message !== 'yetki') durum(false); }
}

/* ---------- Deck sekmesi ---------- */
const SES_TURLERI = ['mp3', 'wav'];
const sesMi = o => SES_TURLERI.includes(o.t);
const siralanabilir = () => chipSecim === 'fav' || chipSecim.indexOf('k:') === 0;

function haritaKur(){
  harita = {};
  deckVeri.klasorler.forEach(k => k.ogeler.forEach(o => { harita[k.ad + '/' + o.d] = { k: k.ad, o: o }; }));
}
function gorunumIdleri(){
  if (chipSecim === 'fav') return deckVeri.favoriler.filter(i => harita[i]);
  if (chipSecim === 'son') return deckVeri.son.filter(i => harita[i]).slice(0, 20);
  const ids = [];
  deckVeri.klasorler.forEach(k => {
    if (chipSecim === '*' || chipSecim === 'k:' + k.ad) k.ogeler.forEach(o => ids.push(k.ad + '/' + o.d));
  });
  return ids;
}
function chipYap(anahtar, yazi){
  const c = document.createElement('button'); c.className = 'chip' + (chipSecim === anahtar ? ' secili' : ''); c.textContent = yazi;
  c.addEventListener('click', () => { chipSecim = anahtar; localStorage.setItem('gifchip2', anahtar); deckCiz(); });
  return c;
}
function tileYap(id){
  const h = harita[id], o = h.o, ses = sesMi(o);
  const b = document.createElement('div'); b.className = 'tile' + (ses ? ' ses' : ''); b.dataset.id = id; b.setAttribute('role', 'button');
  if (ses) { const ik = document.createElement('div'); ik.className = 'ikon'; ik.textContent = '🔊'; b.appendChild(ik); }
  else {
    const im = document.createElement('img'); im.loading = 'lazy'; im.alt = o.d;
    im.src = '/api/kucuk?k=' + encodeURIComponent(h.k) + '&d=' + encodeURIComponent(o.d) + '&v=' + o.v + '&pin=' + encodeURIComponent(pin);
    b.appendChild(im);
  }
  const ad = document.createElement('div'); ad.className = 'ad'; ad.textContent = o.d.replace(/\.[^.]+$/, ''); b.appendChild(ad);
  if (ses || o.t === 'gif' || o.t === 'webp') { const r = document.createElement('div'); r.className = 'rozet'; r.textContent = ses ? 'SES' : o.t.toUpperCase(); b.appendChild(r); }
  const y = document.createElement('span'); y.className = 'yildiz' + (deckVeri.favoriler.includes(id) ? ' acik' : '');
  y.textContent = y.className.indexOf('acik') >= 0 ? '★' : '☆';
  y.addEventListener('click', e => { e.stopPropagation(); favoriDegistir(id); });
  b.appendChild(y);
  if (duzenModu && siralanabilir()) {
    const t = document.createElement('span'); t.className = 'tutamac'; t.textContent = '⠿';
    t.addEventListener('pointerdown', e => surukleBasla(e, b)); b.appendChild(t);
  }
  b.addEventListener('click', () => { if (!duzenModu) gonder(h.k, b, o.d); });
  return b;
}
function deckCiz(){
  haritaKur();
  if (!chipSecim) chipSecim = deckVeri.favoriler.length ? 'fav' : '*';
  if (chipSecim.indexOf('k:') === 0 && !deckVeri.klasorler.some(k => 'k:' + k.ad === chipSecim)) chipSecim = '*';
  const chips = $('#chips'); chips.textContent = '';
  const toplam = deckVeri.klasorler.reduce((t, k) => t + k.ogeler.length, 0);
  chips.appendChild(chipYap('fav', T('⭐ Favoriler') + ' (' + deckVeri.favoriler.filter(i => harita[i]).length + ')'));
  chips.appendChild(chipYap('son', T('🕘 Son')));
  chips.appendChild(chipYap('*', T('Tümü') + ' (' + toplam + ')'));
  deckVeri.klasorler.forEach(k => chips.appendChild(chipYap('k:' + k.ad, k.ad + ' (' + k.ogeler.length + ')')));
  const secili = chips.querySelector('.secili'); if (secili) secili.scrollIntoView({ inline: 'center', block: 'nearest' });

  $('#deck').classList.toggle('duzen', duzenModu);
  $('#duzenle').textContent = duzenModu ? T('✔ Bitti') : T('✎ Düzenle');
  $('#ipucu').textContent = !duzenModu ? '' : (siralanabilir()
    ? T('Sıralamak için ⠿ tutamacını sürükle • ☆ ile favori ekle/çıkar')
    : T('Bu görünümde sıralama yok. Favoriler veya bir klasör seç.'));

  const izgara = $('#izgara'); izgara.textContent = '';
  const ids = gorunumIdleri();
  ids.forEach(id => izgara.appendChild(tileYap(id)));
  if (!ids.length) {
    const g = document.createElement('div'); g.style.gridColumn = '1/-1';
    bosMesaj(g, chipSecim === 'fav' ? T('Henüz favori yok. Bir görselin ☆ simgesine dokun.')
      : chipSecim === 'son' ? T('Henüz bir şey oynatılmadı.')
      : T('Burada öğe yok. Bilgisayardaki uygulamadan dosya ekle.'));
    izgara.appendChild(g);
  }
}
async function deckYukle(){
  if (!$('#pinkutu').hidden || suruk) return;
  try {
    const v = await api('/api/gorseller'); durum(true);
    const imza = JSON.stringify([v.klasorler, v.favoriler, v.son]);
    if (imza !== deckImza) { deckImza = imza; deckVeri = { klasorler: v.klasorler, favoriler: v.favoriler, son: v.son }; deckCiz(); }
  } catch (e) { if (e.message !== 'yetki') durum(false); }
}
async function deckKaydet(govde){
  try { const v = await api('/api/deck', govde); if (!v.ok) toast(v.hata || T('Kaydedilemedi'), true); }
  catch (e) { if (e.message !== 'yetki') toast(T('Kaydedilemedi'), true); }
  deckImza = ''; deckYukle();
}
function favoriDegistir(id){
  const yeni = !deckVeri.favoriler.includes(id);
  if (navigator.vibrate) navigator.vibrate(15);
  deckVeri.favoriler = yeni ? deckVeri.favoriler.concat([id]) : deckVeri.favoriler.filter(x => x !== id);
  toast(yeni ? T('★ Favorilere eklendi') : T('Favorilerden çıkarıldı'));
  deckCiz();
  deckKaydet({ islem: 'favori', id: id, deger: yeni });
}

/* ---- Surukleyerek siralama (tutamac ile) ---- */
function hedefBul(x, y){
  const el = document.elementFromPoint(x, y); if (!el) return null;
  const t = el.closest('.tile');
  return (t && t !== suruk.tile && t.parentNode === $('#izgara')) ? t : null;
}
function hedefIsaretle(){
  const h = hedefBul(suruk.x, suruk.y);
  if (suruk.hedef && suruk.hedef !== h) suruk.hedef.classList.remove('hedef');
  if (h) h.classList.add('hedef');
  suruk.hedef = h;
}
function surukleBasla(e, tile){
  if (suruk || !siralanabilir()) return;
  e.preventDefault(); e.stopPropagation();
  const r = tile.getBoundingClientRect();
  const klon = tile.cloneNode(true); klon.className = 'tile klon';
  klon.style.width = r.width + 'px'; klon.style.height = r.height + 'px'; klon.style.left = r.left + 'px'; klon.style.top = r.top + 'px';
  document.body.appendChild(klon); tile.classList.add('yer');
  const tut = e.currentTarget;
  suruk = { tile: tile, klon: klon, tut: tut, ox: e.clientX - r.left, oy: e.clientY - r.top, x: e.clientX, y: e.clientY, hedef: null };
  try { tut.setPointerCapture(e.pointerId); } catch (x) {}
  tut.addEventListener('pointermove', surukleHareket);
  tut.addEventListener('pointerup', surukleBitir);
  tut.addEventListener('pointercancel', surukleBitir);
  suruk.zaman = setInterval(() => {
    if (!suruk) return;
    const k = window.innerHeight;
    if (suruk.y < 90) window.scrollBy(0, -14); else if (suruk.y > k - 90) window.scrollBy(0, 14);
    hedefIsaretle();
  }, 30);
}
function surukleHareket(e){
  if (!suruk) return;
  suruk.x = e.clientX; suruk.y = e.clientY;
  suruk.klon.style.left = (e.clientX - suruk.ox) + 'px'; suruk.klon.style.top = (e.clientY - suruk.oy) + 'px';
  hedefIsaretle();
}
function surukleBitir(){
  if (!suruk) return;
  const s = suruk; suruk = null;
  clearInterval(s.zaman);
  s.tut.removeEventListener('pointermove', surukleHareket);
  s.tut.removeEventListener('pointerup', surukleBitir);
  s.tut.removeEventListener('pointercancel', surukleBitir);
  s.klon.remove(); s.tile.classList.remove('yer');
  const h = s.hedef; if (h) h.classList.remove('hedef');
  if (!h) return;
  const izgara = $('#izgara'), liste = Array.from(izgara.children);
  if (liste.indexOf(s.tile) < liste.indexOf(h)) izgara.insertBefore(s.tile, h.nextSibling); else izgara.insertBefore(s.tile, h);
  const idler = Array.from(izgara.querySelectorAll('.tile')).map(t => t.dataset.id);
  if (chipSecim === 'fav') {
    deckVeri.favoriler = idler;
    deckKaydet({ islem: 'favori_sira', idler: idler });
  } else {
    const ad = chipSecim.slice(2), dosyalar = idler.map(i => harita[i].o.d);
    const k = deckVeri.klasorler.find(x => x.ad === ad);
    if (k) k.ogeler.sort((a, b) => dosyalar.indexOf(a.d) - dosyalar.indexOf(b.d));
    deckKaydet({ islem: 'sira', klasor: ad, dosyalar: dosyalar });
  }
}

/* ---------- Sekme gecisi ---------- */
function yenile(){ if (sekme === 'deck') deckYukle(); else if (sekme === 'yukle') yukleHazirla(); else yukle(); }
function sekmeGoster(s){
  sekme = s; localStorage.setItem('gifsekme', s);
  $('#rastgele').hidden = s !== 'rastgele'; $('#deck').hidden = s !== 'deck'; $('#yukle').hidden = s !== 'yukle';
  document.querySelectorAll('.sekme').forEach(b => b.classList.toggle('secili', b.dataset.s === s));
  yenile();
}
document.querySelectorAll('.sekme').forEach(b => b.addEventListener('click', () => sekmeGoster(b.dataset.s)));
$('#durdur').addEventListener('click', async () => {
  if (navigator.vibrate) navigator.vibrate(30);
  try { const v = await api('/api/durdur', {}); toast(v.ok ? T('Durduruldu') : (v.hata || T('Hata')), !v.ok); }
  catch (e) { if (e.message !== 'yetki') { toast(T('Bağlantı hatası'), true); durum(false); } }
});
$('#duzenle').addEventListener('click', () => { duzenModu = !duzenModu; deckCiz(); });
$('#pinbtn').addEventListener('click', () => { pin = $('#pingiris').value.trim(); localStorage.setItem('gifpin', pin); $('#pinkutu').hidden = true; deckImza = ''; yenile(); });
$('#pingiris').addEventListener('keydown', e => { if (e.key === 'Enter') $('#pinbtn').click(); });
document.addEventListener('visibilitychange', () => { if (!document.hidden) yenile(); });
/* ---------- Yukle sekmesi (galeri / dosya yoneticisi / pano / surukle-birak) ---------- */
const UZ_OK = /*UZ*/[];                            // Sunucu desteklenen uzantilari gonderir
const YUK_LIMIT = 60 * 1024 * 1024;
let yukKlasorler = [], yukImza = '', yukTercih = '', yukKuyruk = [], yukCalisiyor = false;
const dokunmatik = !!(window.matchMedia && matchMedia('(pointer: coarse)').matches);
const uzanti = ad => (ad.split('.').pop() || '').toLowerCase();

function hedefDoldur(){
  const s = $('#hedef'), onceki = yukTercih || s.value || localStorage.getItem('gifhedef') || '';
  yukTercih = ''; s.textContent = '';
  yukKlasorler.forEach(ad => { const o = document.createElement('option'); o.value = ad; o.textContent = ad; s.appendChild(o); });
  const y = document.createElement('option'); y.value = '__yeni__'; y.textContent = T('➕ Yeni klasör…'); s.appendChild(y);
  s.value = (yukKlasorler.indexOf(onceki) >= 0 || onceki === '__yeni__') ? onceki : (yukKlasorler[0] || '__yeni__');
  hedefDegisti();
}
function hedefDegisti(){ $('#yeniad').hidden = $('#hedef').value !== '__yeni__'; localStorage.setItem('gifhedef', $('#hedef').value); }
function hedefBilgisi(){
  const v = $('#hedef').value;
  if (v === '__yeni__') { const ad = $('#yeniad').value.trim(); return ad ? { klasor: ad, yeni: true } : null; }
  return v ? { klasor: v, yeni: false } : null;
}
async function yukleHazirla(){
  if (!$('#pinkutu').hidden) return;
  try {
    const v = await api('/api/klasorler'); durum(true);
    const imza = JSON.stringify(v.klasorler.map(k => k.ad));
    if (imza !== yukImza) { yukImza = imza; yukKlasorler = v.klasorler.map(k => k.ad); hedefDoldur(); }
  } catch (e) { if (e.message !== 'yetki') durum(false); }
}
function ysatirYap(ad){
  const k = document.createElement('div'); k.className = 'ysatir';
  const a = document.createElement('div'); a.className = 'yad';
  const s1 = document.createElement('span'); s1.textContent = ad;
  const s2 = document.createElement('span'); s2.textContent = '0%';
  a.appendChild(s1); a.appendChild(s2); k.appendChild(a);
  const bar = document.createElement('div'); bar.className = 'ybar'; const i = document.createElement('i'); bar.appendChild(i); k.appendChild(bar);
  const sn = document.createElement('div'); sn.className = 'ysonuc'; k.appendChild(sn);
  $('#yuklemeler').prepend(k);
  return {
    ilerle: o => { i.style.width = Math.round(o * 100) + '%'; s2.textContent = Math.round(o * 100) + '%'; },
    bitti: (ok, metin) => { k.classList.add(ok ? 'ok' : 'hata'); s2.textContent = ok ? '✓' : '✕'; sn.textContent = metin; }
  };
}
function yukEkle(f, hedef){
  let ad = f.name || '';
  if (!ad || /^image\.\w+$/i.test(ad)) {            // Panodan yapistirilan resim: zamanli ad ver
    const uz = ((f.type || 'image/png').split('/')[1] || 'png').replace('jpeg', 'jpg').replace(/[^a-z0-9]/gi, '');
    const z = new Date(), p = n => String(n).padStart(2, '0');
    ad = 'yapistirilan_' + z.getFullYear() + p(z.getMonth() + 1) + p(z.getDate()) + '_' + p(z.getHours()) + p(z.getMinutes()) + p(z.getSeconds()) + '_' + Math.floor(Math.random() * 900 + 100) + '.' + uz;
  }
  const satir = ysatirYap(ad);
  if (UZ_OK.indexOf(uzanti(ad)) < 0) { satir.bitti(false, T('Desteklenmeyen dosya:') + ' .' + uzanti(ad)); return; }
  if (f.size > YUK_LIMIT) { satir.bitti(false, T('Dosya çok büyük')); return; }
  yukKuyruk.push({ f: f, ad: ad, hedef: hedef, satir: satir });
  yukIsle();
}
function dosyalariYukle(dosyalar){
  const hedef = hedefBilgisi();
  if (!hedef) { toast(T('Önce bir hedef klasör seç.'), true); return; }
  Array.from(dosyalar).forEach(f => yukEkle(f, hedef));
}
function xhrYukle(is){
  return new Promise(cevap => {
    const x = new XMLHttpRequest();
    x.open('POST', '/api/yukle?k=' + encodeURIComponent(is.hedef.klasor) + '&d=' + encodeURIComponent(is.ad) + (is.hedef.yeni ? '&yeni=1' : ''));
    x.setRequestHeader('X-Pin', encodeURIComponent(pin));
    x.setRequestHeader('Content-Type', 'application/octet-stream');
    x.upload.onprogress = e => { if (e.lengthComputable) is.satir.ilerle(e.loaded / e.total); };
    x.onload = () => {
      let v = {}; try { v = JSON.parse(x.responseText); } catch (e) {}
      if (x.status === 401 || x.status === 429) pinSor(pin ? v.hata : '');
      cevap(v);
    };
    x.onerror = () => cevap({ ok: false, hata: T('Bağlantı hatası') });
    x.send(is.f);
  });
}
async function yukIsle(){
  if (yukCalisiyor) return;
  yukCalisiyor = true;
  while (yukKuyruk.length) {
    const is = yukKuyruk.shift();
    const v = await xhrYukle(is);
    if (v.ok) {
      is.satir.bitti(true, v.dosya + ' → ' + v.klasor);
      if (is.hedef.yeni) { is.hedef.yeni = false; $('#yeniad').value = ''; yukTercih = v.klasor; yukImza = ''; yukleHazirla(); }
    } else is.satir.bitti(false, v.hata || T('Yüklenemedi'));
  }
  yukCalisiyor = false;
}
$('#yeniad').placeholder = T('Yeni klasörün adı');
$('#yapistir').setAttribute('data-ph', T('Yapıştırma alanı: uzun bas → Yapıştır'));
$('#hedef').addEventListener('change', hedefDegisti);
$('#dosyasec').addEventListener('click', () => $('#girdi').click());
$('#girdi').addEventListener('change', e => { dosyalariYukle(e.target.files); e.target.value = ''; });
$('#klasorsec').addEventListener('click', () => $('#girdi2').click());
$('#girdi2').addEventListener('change', e => { dosyalariYukle(Array.from(e.target.files).filter(f => UZ_OK.indexOf(uzanti(f.name)) >= 0)); e.target.value = ''; });
if (dokunmatik) $('#klasorsec').hidden = true;      // Telefonda klasor secimi yok
['dragenter', 'dragover'].forEach(ev => document.addEventListener(ev, e => { if (sekme === 'yukle') { e.preventDefault(); $('#birak').classList.add('uzerinde'); } }));
document.addEventListener('dragleave', () => $('#birak').classList.remove('uzerinde'));
document.addEventListener('drop', e => {
  $('#birak').classList.remove('uzerinde');
  if (sekme !== 'yukle') return;
  e.preventDefault();
  if (e.dataTransfer && e.dataTransfer.files.length) dosyalariYukle(e.dataTransfer.files);
});
document.addEventListener('paste', e => {          // Ctrl+V / uzun bas -> Yapistir: panodaki resim/dosya dogrudan yuklenir
  if (sekme !== 'yukle') return;
  const dosyalar = [], items = (e.clipboardData && e.clipboardData.items) || [];
  for (let i = 0; i < items.length; i++) { if (items[i].kind === 'file') { const f = items[i].getAsFile(); if (f) dosyalar.push(f); } }
  if (dosyalar.length) { e.preventDefault(); dosyalariYukle(dosyalar); }
  setTimeout(() => { if ($('#yapistir').textContent) $('#yapistir').textContent = ''; }, 0);
});
sekmeGoster(sekme); setInterval(() => { if (!document.hidden) yenile(); }, 6000);
</script></body></html>"""


def web_sayfasi():
    """Telefon sayfasini secili dilde hazirlar (secili dilin tum ceviri sozlugu sayfaya gomulur)."""
    sozluk = CEVIRI.get(DIL, {}) if DIL != "tr" else {}
    uzantilar = [e.lstrip(".") for e in YUKLEME_UZANTILARI]
    return (WEB_SAYFA.replace("/*DIL*/{}", json.dumps(sozluk, ensure_ascii=False))
            .replace("/*UZ*/[]", json.dumps(uzantilar))
            .replace('<html lang="tr">', f'<html lang="{DIL}">'))


def yerel_ip():
    """Bilgisayarin yerel agdaki (Wi-Fi) IP adresini bulur."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))        # Gercekte paket gonderilmez, sadece dogru ag karti secilir
        return s.getsockname()[0]
    except OSError:
        try:
            return socket.gethostbyname(socket.gethostname())
        except OSError:
            return "127.0.0.1"
    finally:
        s.close()


def yerel_ip_listesi():
    """Bilgisayarin tum olasi yerel IP'leri. Ilk sira Wi-Fi/ana ag karti, sonra ev agi aralıklari."""
    ana = yerel_ip()
    digerleri = []
    try:
        for bilgi in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            digerleri.append(bilgi[4][0])
    except OSError:
        pass

    def oncelik(ip):
        if ip.startswith("192.168."):
            return 0
        if ip.startswith("10."):
            return 1
        if ip.startswith("172.") and 16 <= int(ip.split(".")[1]) <= 31:
            return 2
        return 3

    sonuc = []
    for ip in [ana] + sorted(digerleri, key=oncelik):
        if ip not in sonuc and not ip.startswith(("127.", "169.254.", "0.")):
            sonuc.append(ip)
    return sonuc or ["127.0.0.1"]


_KUCUK_ONBELLEK = collections.OrderedDict()
_KUCUK_KILIT = threading.Lock()


def kucuk_resim(yol, boyut=192):
    """Deck sekmesi icin kucuk onizleme (PNG, seffaflik korunur). GIF/WEBP'de ilk kare kullanilir."""
    try:
        anahtar = (yol, os.path.getmtime(yol))
    except OSError:
        raise
    with _KUCUK_KILIT:
        if anahtar in _KUCUK_ONBELLEK:
            _KUCUK_ONBELLEK.move_to_end(anahtar)
            return _KUCUK_ONBELLEK[anahtar]
    with Image.open(yol) as img:
        kare = ImageOps.exif_transpose(img).convert("RGBA")
    kare.thumbnail((boyut, boyut), YENIDEN_BOYUTLA)
    tampon = io.BytesIO()
    kare.save(tampon, "PNG")
    veri = tampon.getvalue()
    with _KUCUK_KILIT:
        _KUCUK_ONBELLEK[anahtar] = veri
        while len(_KUCUK_ONBELLEK) > 150:
            _KUCUK_ONBELLEK.popitem(last=False)
    return veri


YUKLEME_LIMIT_MB = 60                                    # Tek dosya icin en fazla boyut
YUKLEME_UZANTILARI = GORSEL_UZANTILARI + (".mp3", ".wav")


def _benzersiz_yol(klasor_yolu, taban, uzanti):
    yol = os.path.join(klasor_yolu, taban + uzanti)
    i = 2
    while os.path.exists(yol):
        yol = os.path.join(klasor_yolu, f"{taban}_{i}{uzanti}")
        i += 1
    return yol


def yukleme_kaydet(klasor_yolu, ad, okuyucu, uzunluk):
    """
    Telefon/tarayicidan gelen dosyayi guvenle kaydeder. Dondurur: (basarili_mi, son_dosya_adi | hata_mesaji).
    - Dosya adi temizlenir (yol/ozel karakter yok), uzanti kontrol edilir, icerik DOGRULANIR (gercekten gorsel/ses mi).
    - GIF/PNG/WEBP oldugu gibi. JPEG oldugu gibi (yonu yanlis ya da 2048 px'ten buyukse duzeltilip kucultulur).
      Diger resimler (BMP, TIFF, ICO, HEIC...) PNG'ye cevrilir. Ayni ad varsa _2, _3 eklenir.
    """
    ad = os.path.basename(str(ad).replace("\\", "/"))
    ad = GECERSIZ_AD.sub("_", ad).strip().lstrip(".")[:90]
    taban, uz = os.path.splitext(ad)
    uz = uz.lower()
    taban = taban.strip() or "dosya"
    if uz not in YUKLEME_UZANTILARI:
        return False, L("Desteklenmeyen dosya türü")
    gecici = os.path.join(klasor_yolu, f".yukleniyor_{threading.get_ident()}_{int(time.time() * 1000)}.tmp")
    try:
        kalan = uzunluk
        with open(gecici, "wb") as f:
            while kalan > 0:
                parca = okuyucu(min(1 << 20, kalan))
                if not parca:
                    return False, L("Yükleme başarısız")
                f.write(parca)
                kalan -= len(parca)
        if uz in (".mp3", ".wav"):                       # Ses: dosya basligini kontrol et
            with open(gecici, "rb") as f:
                bas = f.read(12)
            mp3 = bas[:3] == b"ID3" or (len(bas) > 1 and bas[0] == 0xFF and (bas[1] & 0xE0) == 0xE0)
            wav = bas[:4] == b"RIFF" and bas[8:12] == b"WAVE"
            if not ((uz == ".mp3" and mp3) or (uz == ".wav" and wav)):
                return False, L("Geçersiz ya da bozuk dosya")
            hedef = _benzersiz_yol(klasor_yolu, taban, uz)
            os.replace(gecici, hedef)
            return True, os.path.basename(hedef)
        try:                                             # Gorsel: gercekten gorsel mi?
            with Image.open(gecici) as im:
                bicim, (w, h) = im.format, im.size
                im.verify()
        except Exception:
            return False, L("Geçersiz ya da bozuk dosya")
        if w * h > 120000000:
            return False, L("Geçersiz ya da bozuk dosya")
        if bicim in ("GIF", "PNG", "WEBP"):              # Oldugu gibi (gercek bicime gore uzanti)
            hedef = _benzersiz_yol(klasor_yolu, taban, {"GIF": ".gif", "PNG": ".png", "WEBP": ".webp"}[bicim])
            os.replace(gecici, hedef)
            return True, os.path.basename(hedef)
        if bicim in ("JPEG", "MPO"):                     # Fotograf: genelde oldugu gibi kalir
            with Image.open(gecici) as im:
                duzelt = im.getexif().get(274, 1) not in (0, 1) or max(im.size) > 2048   # EXIF yonu / cok buyuk
                if duzelt:
                    im = ImageOps.exif_transpose(im).convert("RGB")
                    im.thumbnail((2048, 2048), YENIDEN_BOYUTLA)
                    hedef = _benzersiz_yol(klasor_yolu, taban, ".jpg")
                    im.save(hedef, "JPEG", quality=92)
                    return True, os.path.basename(hedef)
            hedef = _benzersiz_yol(klasor_yolu, taban, ".jpg")
            os.replace(gecici, hedef)
            return True, os.path.basename(hedef)
        with Image.open(gecici) as im:                   # BMP, TIFF, ICO, HEIC, AVIF...: PNG'ye cevir
            im = ImageOps.exif_transpose(im).convert("RGBA")
            im.thumbnail((2048, 2048), YENIDEN_BOYUTLA)
            hedef = _benzersiz_yol(klasor_yolu, taban, ".png")
            im.save(hedef, "PNG")
        return True, os.path.basename(hedef)
    except OSError:
        return False, L("Yükleme başarısız")
    finally:
        try:
            os.remove(gecici)
        except OSError:
            pass


class WebIsleyici(BaseHTTPRequestHandler):
    uyg = None                                   # Uygulama nesnesi (WebSunucu atar)

    def log_message(self, *args):                # Konsola log yazma
        pass

    def gonder(self, kod, icerik, tur="application/json; charset=utf-8", cache="no-store"):
        if not isinstance(icerik, bytes):
            icerik = json.dumps(icerik, ensure_ascii=False).encode("utf-8")
        try:
            self.send_response(kod)
            self.send_header("Content-Type", tur)
            self.send_header("Content-Length", str(len(icerik)))
            self.send_header("Cache-Control", cache)
            self.end_headers()
            self.wfile.write(icerik)
        except (ConnectionError, OSError):
            pass

    def yetkili_mi(self):
        sorgu = parse_qs(urlparse(self.path).query)
        verilen = unquote(self.headers.get("X-Pin") or "") or (sorgu.get("pin") or [""])[0]
        ok, mesaj, kod = self.uyg.web_yetki(self.client_address[0], verilen)
        if not ok:
            self.gonder(kod, {"ok": False, "hata": mesaj})
        return ok

    def do_GET(self):
        yol = urlparse(self.path).path
        if yol in ("/", "/index.html"):
            self.gonder(200, web_sayfasi().encode("utf-8"), "text/html; charset=utf-8")
        elif yol == "/api/klasorler":
            if self.yetkili_mi():
                self.gonder(200, {"ok": True, "klasorler": self.uyg.web_klasor_listesi()})
        elif yol == "/api/gorseller":
            if self.yetkili_mi():
                self.gonder(200, dict(self.uyg.web_gorsel_listesi(), ok=True))
        elif yol == "/api/kucuk":
            if not self.yetkili_mi():
                return
            sorgu = parse_qs(urlparse(self.path).query)
            dosya_yolu = self.uyg.web_dosya_yolu((sorgu.get("k") or [""])[0], (sorgu.get("d") or [""])[0])
            if not dosya_yolu:
                self.gonder(404, {"ok": False, "hata": L("Dosya bulunamadı")})
                return
            try:
                veri = kucuk_resim(dosya_yolu)
            except Exception:
                self.gonder(500, {"ok": False, "hata": L("Önizleme üretilemedi")})
                return
            self.gonder(200, veri, "image/png", cache="private, max-age=86400")   # Telefon onbellekler
        elif yol == "/favicon.ico":
            self.gonder(204, b"", "image/x-icon")
        else:
            self.gonder(404, {"ok": False, "hata": L("Bulunamadı")})

    def yukle_isle(self):
        sorgu = parse_qs(urlparse(self.path).query)
        klasor = (sorgu.get("k") or [""])[0]
        dosya = (sorgu.get("d") or [""])[0]
        yeni = (sorgu.get("yeni") or ["0"])[0] == "1"
        try:
            uzunluk = int(self.headers.get("Content-Length") or 0)
        except ValueError:
            uzunluk = 0
        try:
            self.connection.settimeout(120)              # Yavas baglantida takili kalmasin
        except Exception:
            pass
        ok, sonuc, klasor = self.uyg.web_yukle(klasor, dosya, yeni, self.rfile.read, uzunluk)
        self.gonder(200, {"ok": True, "dosya": sonuc, "klasor": klasor} if ok else {"ok": False, "hata": sonuc})

    def do_POST(self):
        yol = urlparse(self.path).path
        if yol not in ("/api/tetik", "/api/deck", "/api/durdur", "/api/yukle"):
            self.gonder(404, {"ok": False, "hata": L("Bulunamadı")})
            return
        if not self.yetkili_mi():
            return
        if yol == "/api/yukle":                          # Dosya yukleme: ham govde (JSON degil)
            self.yukle_isle()
            return
        try:
            uzunluk = min(int(self.headers.get("Content-Length") or 0), 65536)
            veri = json.loads(self.rfile.read(uzunluk) or b"{}")
            if not isinstance(veri, dict):
                raise ValueError
        except (ValueError, AttributeError):
            self.gonder(400, {"ok": False, "hata": L("Geçersiz istek")})
            return
        if yol == "/api/durdur":                         # Ekrandaki GIF'i kapat
            self.uyg.web_durdur()
            self.gonder(200, {"ok": True})
            return
        if yol == "/api/deck":                           # Favori / siralama islemleri
            ok, mesaj = self.uyg.web_deck_islem(veri)
            self.gonder(200, {"ok": ok, "hata": mesaj})
            return
        klasor = str(veri.get("klasor", ""))
        dosya = str(veri.get("dosya", "")) or None
        ok, sonuc = self.uyg.web_tetik(klasor, dosya)
        self.gonder(200, {"ok": True, "klasor": sonuc} if ok else {"ok": False, "hata": sonuc})

class _Sunucu(ThreadingHTTPServer):
    allow_reuse_address = False                  # Windows'ta ayni portu iki programin almasini engeller
    daemon_threads = True


class WebSunucu:
    def __init__(self, uyg):
        self.uyg = uyg
        self.sunucu = None

    def baslat(self, port):
        self.durdur()
        isleyici = type("Isleyici", (WebIsleyici,), {"uyg": self.uyg})
        try:
            self.sunucu = _Sunucu(("0.0.0.0", int(port)), isleyici)
        except OSError as hata:
            print("Web sunucu baslatilamadi:", hata)
            self.sunucu = None
            return False
        threading.Thread(target=self.sunucu.serve_forever, daemon=True).start()
        return True

    def durdur(self):
        if self.sunucu is not None:
            try:
                self.sunucu.shutdown()
                self.sunucu.server_close()
            except Exception:
                pass
            self.sunucu = None


# =====================================================================
#   ARAYUZ
# =====================================================================
RENK = {"bg": "#13151c", "yuzey": "#1a1d27", "yuzey2": "#222637", "kenar": "#2c3145", "liste": "#161922",
        "yazi": "#e6e8ee", "soluk": "#8b92a5",
        "vurgu": "#6372e6", "vurgu2": "#7480ee", "vurgu_acik": "#aab4ff", "vurgu_soft": "#252a4a",
        "vurgu_kenar": "#3a437d",
        "yesil": "#4cc38a", "kirmizi": "#f06a6a", "sari": "#ffd43b",
        "kart": "#1a1d27", "kart2": "#222637"}


def koyu_baslik(pencere):
    """Windows 10/11'de pencere baslik cubugunu koyu yapar (olmazsa sessizce gecer)."""
    try:
        pencere.update_idletasks()
        hwnd = ctypes.windll.user32.GetParent(pencere.winfo_id()) or pencere.winfo_id()
        deger = ctypes.c_int(1)
        for ozellik in (20, 19):
            if ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, ozellik, ctypes.byref(deger),
                                                          ctypes.sizeof(deger)) == 0:
                break
    except Exception:
        pass


FONT = "Segoe UI"
_FONT_ONBELLEK = {}


def yazi_genislik(metin, boyut=10, kalin=False):
    """Metnin piksel genisligi (dugme/kirpma hesabi icin)."""
    try:
        anahtar = (boyut, kalin)
        f = _FONT_ONBELLEK.get(anahtar)
        if f is None:
            f = tkfont.Font(family=FONT, size=boyut, weight="bold" if kalin else "normal")
            _FONT_ONBELLEK[anahtar] = f
        return f.measure(metin)
    except Exception:
        return int(len(metin) * boyut * 0.62)


def kirp(metin, azami, boyut=10, kalin=False):
    """Metin azami pikselden uzunsa sonuna ... koyarak kisaltir."""
    if yazi_genislik(metin, boyut, kalin) <= azami:
        return metin
    while len(metin) > 1 and yazi_genislik(metin + "…", boyut, kalin) > azami:
        metin = metin[:-1]
    return metin + "…"


def yuvarlak(tuval, x1, y1, x2, y2, r, **kw):
    """Yuvarlak kenarli dikdortgen (canvas poligonu)."""
    nok = [x1 + r, y1, x2 - r, y1, x2, y1, x2, y1 + r, x2, y2 - r, x2, y2, x2 - r, y2,
           x1 + r, y2, x1, y2, x1, y2 - r, x1, y1 + r, x1, y1]
    return tuval.create_polygon(nok, smooth=True, **kw)


class Dugme(tk.Canvas):
    """Yuvarlak kenarli, ozel cizimli dugme. Stiller: normal | birincil | tehlike | hayalet."""
    STILLER = {
        "normal":   {"dolgu": "#222637", "ustu": "#2b3147", "basili": "#1a1d2a", "kenar": "#30364c", "yazi": "#dde0e8"},
        "birincil": {"dolgu": "#5b68d4", "ustu": "#6875e0", "basili": "#4d59bd", "kenar": "#5b68d4", "yazi": "#ffffff"},
        "tehlike":  {"dolgu": "#2a1d24", "ustu": "#38252f", "basili": "#221820", "kenar": "#553039", "yazi": "#ff9b9b"},
        "hayalet":  {"dolgu": None, "ustu": "#222637", "basili": "#1a1d2a", "kenar": "#30364c", "yazi": "#a2a9bd"},
    }

    def __init__(self, ust, text, command=None, stil="normal", genislik=None, yukseklik=38, arka=None, yazi_boyut=10):
        self.metin = text
        self.komut = command
        self.stil = stil
        self.arka = arka or RENK["bg"]
        self.yazi_boyut = yazi_boyut
        self.sabit_genislik = genislik
        self.gen = genislik or max(96, yazi_genislik(text, yazi_boyut, True) + 40)
        self.yuk = yukseklik
        self.ustunde = False
        self.basili = False
        super().__init__(ust, width=self.gen, height=yukseklik, bg=self.arka, highlightthickness=0, bd=0,
                         cursor="hand2")
        self.bind("<Enter>", self._gir)
        self.bind("<Leave>", self._cik)
        self.bind("<ButtonPress-1>", self._bas)
        self.bind("<ButtonRelease-1>", self._birak)
        self._ciz()

    def configure(self, cnf=None, **kw):
        if "text" in kw:
            self.metin = kw.pop("text")
            if not self.sabit_genislik:
                self.gen = max(96, yazi_genislik(self.metin, self.yazi_boyut, True) + 40)
                kw["width"] = self.gen
            self._ciz()
        if kw or cnf:
            super().configure(cnf, **kw) if cnf else super().configure(**kw)
    config = configure

    def _ciz(self):
        st = self.STILLER[self.stil]
        zemin = st["basili"] if self.basili else (st["ustu"] if self.ustunde else st["dolgu"])
        try:
            self.delete("all")
            yuvarlak(self, 1, 1, self.gen - 1, self.yuk - 1, 11, fill=zemin or self.arka, outline=st["kenar"])
            self.create_text(self.gen / 2, self.yuk / 2 + (1 if self.basili else 0), text=self.metin,
                             fill=st["yazi"], font=(FONT, self.yazi_boyut, "bold"))
        except tk.TclError:
            pass

    def _gir(self, _e=None):
        self.ustunde = True
        self._ciz()

    def _cik(self, _e=None):
        self.ustunde = False
        self.basili = False
        self._ciz()

    def _bas(self, _e=None):
        self.basili = True
        self._ciz()

    def _birak(self, e):
        icerde = 0 <= e.x <= self.gen and 0 <= e.y <= self.yuk
        self.basili = False
        self._ciz()
        if icerde and self.komut:
            self.komut()


class Anahtar(tk.Canvas):
    """Acik/kapali anahtari (toggle). Bir BooleanVar'a baglidir."""
    def __init__(self, ust, var, arka=None):
        super().__init__(ust, width=46, height=26, bg=arka or RENK["yuzey"], highlightthickness=0, bd=0,
                         cursor="hand2")
        self.var = var
        self.bind("<Button-1>", self._tikla)
        var.trace_add("write", lambda *_a: self._ciz())
        self._ciz()

    def _tikla(self, _e=None):
        try:
            self.var.set(not self.var.get())
        except tk.TclError:
            pass

    def _ciz(self):
        try:
            acik = bool(self.var.get())
            self.delete("all")
            yuvarlak(self, 1, 2, 45, 24, 11, fill=RENK["vurgu"] if acik else "#343a52", outline="")
            x = 34 if acik else 13
            self.create_oval(x - 8, 5, x + 8, 21, fill="#f4f5fb", outline="")
        except tk.TclError:
            pass


def anahtar_satiri(ust, metin, var, arka):
    """Anahtar + tiklanabilir yazi (tek satir)."""
    kap = tk.Frame(ust, bg=arka)
    Anahtar(kap, var, arka).pack(side="left")
    et = tk.Label(kap, text=metin, bg=arka, fg=RENK["yazi"], font=(FONT, 10), cursor="hand2")
    et.pack(side="left", padx=(10, 0))
    et.bind("<Button-1>", lambda _e: var.set(not var.get()))
    return kap


class Kaydirici(tk.Canvas):
    """Yuvarlak uclu kaydirici; secili deger sagda yazar."""
    PAD = 11

    def __init__(self, ust, var, a, b, adim=1, genislik=180, arka=None, birim=""):
        self.gen = genislik
        super().__init__(ust, width=genislik + 66, height=30, bg=arka or RENK["yuzey"], highlightthickness=0,
                         bd=0, cursor="hand2")
        self.var, self.a, self.b, self.adim, self.birim = var, a, b, adim, birim
        self.bind("<Button-1>", self._tikla)
        self.bind("<B1-Motion>", self._tikla)
        var.trace_add("write", lambda *_a: self._ciz())
        self._ciz()

    def _deger(self):
        try:
            return float(self.var.get())
        except (tk.TclError, ValueError):
            return float(self.a)

    def _ciz(self):
        try:
            self.delete("all")
            v = min(self.b, max(self.a, self._deger()))
            oran = (v - self.a) / float(self.b - self.a)
            x1, x2, y = self.PAD, self.gen - self.PAD, 15
            xk = x1 + oran * (x2 - x1)
            self.create_line(x1, y, x2, y, width=4, fill="#343a52", capstyle="round")
            if xk > x1:
                self.create_line(x1, y, xk, y, width=4, fill=RENK["vurgu"], capstyle="round")
            self.create_oval(xk - 8, y - 8, xk + 8, y + 8, fill="#f4f5fb", outline=RENK["vurgu"], width=2)
            yazi = (str(int(round(v))) if self.adim >= 1 else "%.1f" % v) + self.birim
            self.create_text(self.gen + 6, y, text=yazi, anchor="w", fill=RENK["yazi"], font=(FONT, 9, "bold"))
        except tk.TclError:
            pass

    def _tikla(self, e):
        x1, x2 = self.PAD, self.gen - self.PAD
        oran = min(1.0, max(0.0, (e.x - x1) / float(x2 - x1)))
        ham = self.a + oran * (self.b - self.a)
        v = round(ham / self.adim) * self.adim
        try:
            self.var.set(int(v) if self.adim >= 1 else round(v, 1))
        except tk.TclError:
            pass


class Kart(tk.Canvas):
    """Yuvarlak kenarli kart. Icerigi 'govde' cercevesine koy; yukseklik icerige gore otomatik ayarlanir."""
    def __init__(self, ust, baslik=None, arka=None, dolgu=18):
        super().__init__(ust, bg=arka or RENK["bg"], highlightthickness=0, bd=0, height=40)
        self.dolgu = dolgu
        self.ic = tk.Frame(self, bg=RENK["yuzey"])
        self.pen = self.create_window(dolgu, dolgu, window=self.ic, anchor="nw")
        if baslik:
            tk.Label(self.ic, text=baslik, bg=RENK["yuzey"], fg=RENK["vurgu_acik"],
                     font=(FONT, 11, "bold")).pack(anchor="w", pady=(0, 6))
        self.govde = tk.Frame(self.ic, bg=RENK["yuzey"])
        self.govde.pack(fill="x")
        self.bind("<Configure>", self._boyut)
        self.ic.bind("<Configure>", self._icerik)

    def _boyut(self, e):
        try:
            self.delete("zemin")
            yuvarlak(self, 1, 1, e.width - 1, e.height - 1, 16, fill=RENK["yuzey"], outline=RENK["kenar"],
                     tags="zemin")
            self.tag_lower("zemin")
            self.itemconfigure(self.pen, width=max(10, e.width - 2 * self.dolgu))
        except tk.TclError:
            pass

    def _icerik(self, _e=None):
        try:
            h = self.ic.winfo_reqheight() + 2 * self.dolgu
            if abs(int(float(self.cget("height"))) - h) > 1:
                self.configure(height=h)
        except (tk.TclError, ValueError):
            pass


class SekmeCubugu(tk.Canvas):
    """Uc (n) esit parcali sekme cubugu. Secili sekme yumusak vurgulu."""
    YUK = 48

    def __init__(self, ust, basliklar, komut, arka=None):
        super().__init__(ust, height=self.YUK, bg=arka or RENK["bg"], highlightthickness=0, bd=0, cursor="hand2")
        self.basliklar, self.komut = basliklar, komut
        self.secili, self.ustte = 0, -1
        self.bind("<Configure>", lambda _e: self._ciz())
        self.bind("<Button-1>", self._tikla)
        self.bind("<Motion>", self._hareket)
        self.bind("<Leave>", self._cik)

    def _parca(self, x):
        w = max(1, self.winfo_width())
        return min(len(self.basliklar) - 1, max(0, int(x / (w / float(len(self.basliklar))))))

    def sec(self, i, cagir=True):
        self.secili = i
        self._ciz()
        if cagir and self.komut:
            self.komut(i)

    def _tikla(self, e):
        self.sec(self._parca(e.x))

    def _hareket(self, e):
        i = self._parca(e.x)
        if i != self.ustte:
            self.ustte = i
            self._ciz()

    def _cik(self, _e=None):
        self.ustte = -1
        self._ciz()

    def _ciz(self):
        try:
            w, h, n = self.winfo_width(), self.YUK, len(self.basliklar)
            self.delete("all")
            if w < 30:
                return
            yuvarlak(self, 1, 1, w - 1, h - 1, 15, fill=RENK["yuzey"], outline=RENK["kenar"])
            parca = (w - 10) / float(n)
            for i, baslik in enumerate(self.basliklar):
                x1 = 5 + i * parca
                x2 = x1 + parca
                if i == self.secili:
                    yuvarlak(self, x1 + 2, 6, x2 - 2, h - 6, 11, fill=RENK["vurgu_soft"], outline=RENK["vurgu_kenar"])
                elif i == self.ustte:
                    yuvarlak(self, x1 + 2, 6, x2 - 2, h - 6, 11, fill="#20243a", outline="")
                renk = RENK["vurgu_acik"] if i == self.secili else (RENK["yazi"] if i == self.ustte else RENK["soluk"])
                self.create_text((x1 + x2) / 2, h / 2, text=baslik, fill=renk, font=(FONT, 10, "bold"))
        except tk.TclError:
            pass


class SatirKarti(tk.Canvas):
    """Klasor listesindeki tek satir: simge, ad, icerik ozeti ve tus rozeti. Yuvarlak kart olarak cizilir."""
    YUK = 64

    def __init__(self, ust, ad, tikla, cift):
        super().__init__(ust, height=self.YUK, bg=RENK["liste"], highlightthickness=0, bd=0, cursor="hand2")
        self.ad, self.tus, self.atanmis, self.icerik = ad, "", False, ""
        self.secili = self.ustte = False
        self.bind("<Configure>", lambda _e: self._ciz())
        self.bind("<Button-1>", lambda _e: tikla(ad))
        self.bind("<Double-Button-1>", lambda _e: cift(ad))
        self.bind("<Enter>", lambda _e: self._durum(ustte=True))
        self.bind("<Leave>", lambda _e: self._durum(ustte=False))

    def _durum(self, ustte=None, secili=None):
        if ustte is not None:
            self.ustte = ustte
        if secili is not None:
            self.secili = secili
        self._ciz()

    def guncelle(self, tus, atanmis, icerik):
        self.tus, self.atanmis, self.icerik = tus, atanmis, icerik
        self._ciz()

    def _ciz(self):
        try:
            w = self.winfo_width()
            if w < 60:
                return
            self.delete("all")
            if self.secili:
                zemin, kenar, kalin = RENK["vurgu_soft"], RENK["vurgu_kenar"], 2
            else:
                zemin, kenar, kalin = ("#1f2331" if self.ustte else RENK["yuzey"]), RENK["kenar"], 1
            yuvarlak(self, 4, 3, w - 4, self.YUK - 3, 14, fill=zemin, outline=kenar, width=kalin)
            self.create_oval(18, 14, 54, 50, fill="#2a3050" if self.secili else "#262b40", outline="")
            self.create_text(36, 32, text=(self.ad[:1] or "?").upper(), fill=RENK["vurgu_acik"],
                             font=(FONT, 13, "bold"))
            rozet_g = max(96, yazi_genislik(self.tus, 9, True) + 30)
            x2 = w - 22
            x1 = x2 - rozet_g
            if self.atanmis:
                yuvarlak(self, x1, 20, x2, 46, 13, fill=RENK["vurgu_soft"], outline=RENK["vurgu_kenar"])
                self.create_text((x1 + x2) / 2, 33, text=self.tus, fill=RENK["vurgu_acik"], font=(FONT, 9, "bold"))
            else:
                yuvarlak(self, x1, 20, x2, 46, 13, fill=RENK["yuzey"], outline=RENK["kenar"])
                self.create_text((x1 + x2) / 2, 33, text=self.tus, fill=RENK["soluk"], font=(FONT, 9))
            azami = max(60, x1 - 70 - 12)
            self.create_text(70, 25, text=kirp(self.ad, azami, 11, True), anchor="w", fill=RENK["yazi"],
                             font=(FONT, 11, "bold"))
            self.create_text(70, 45, text=kirp(self.icerik, azami, 9), anchor="w", fill=RENK["soluk"],
                             font=(FONT, 9))
        except tk.TclError:
            pass


class KlasorListesi(tk.Frame):
    """Kaydirilabilir yuvarlak kartlardan olusan klasor listesi."""
    def __init__(self, ust, secim_cb=None, cift_cb=None):
        super().__init__(ust, bg=RENK["liste"])
        self.secim_cb, self.cift_cb = secim_cb, cift_cb
        self.tuval = tk.Canvas(self, bg=RENK["liste"], highlightthickness=0, bd=0)
        self.cubuk = ttk.Scrollbar(self, orient="vertical", command=self.tuval.yview)
        self.tuval.configure(yscrollcommand=self.cubuk.set)
        self.tuval.pack(side="left", fill="both", expand=True)
        self.cubuk.pack(side="right", fill="y")
        self.ic = tk.Frame(self.tuval, bg=RENK["liste"])
        self.pen = self.tuval.create_window(0, 0, window=self.ic, anchor="nw")
        self.ic.bind("<Configure>", lambda _e: self.tuval.configure(scrollregion=self.tuval.bbox("all")))
        self.tuval.bind("<Configure>", lambda e: self.tuval.itemconfigure(self.pen, width=e.width))
        self.satirlar = collections.OrderedDict()
        self.secili = None
        self.bos = None

    def selection(self):
        return (self.secili,) if self.secili else ()

    def sec(self, ad):
        self.secili = ad if ad in self.satirlar else None
        for a, s in self.satirlar.items():
            s._durum(secili=(a == self.secili))
        if self.secim_cb and self.secili:
            self.secim_cb(self.secili)

    def doldur(self, satirlar, secili=None):
        """satirlar: [(ad, tus_yazisi, atanmis_mi, icerik)]"""
        adlar = [s[0] for s in satirlar]
        if secili in adlar:
            self.secili = secili
        elif self.secili not in adlar:
            self.secili = None
        if list(self.satirlar.keys()) != adlar:
            for s in self.satirlar.values():
                s.destroy()
            self.satirlar.clear()
            if self.bos is not None:
                self.bos.destroy()
                self.bos = None
            for ad in adlar:
                kart = SatirKarti(self.ic, ad, self.sec, self.cift_cb or (lambda _a: None))
                kart.pack(fill="x", padx=8, pady=3)
                self.satirlar[ad] = kart
            if not adlar:
                self.bos = tk.Label(self.ic, text=L("Henüz klasör yok. «Yeni Klasör» ile başla."), bg=RENK["liste"],
                                    fg=RENK["soluk"], font=(FONT, 10), pady=30)
                self.bos.pack(fill="x")
        for ad, tus, atanmis, icerik in satirlar:
            kart = self.satirlar[ad]
            kart.secili = (ad == self.secili)
            kart.guncelle(tus, atanmis, icerik)

    def tekerlek(self, e):
        if self.ic.winfo_reqheight() > self.tuval.winfo_height():
            self.tuval.yview_scroll(int(-e.delta / 120), "units")


class KaydirmaliSayfa(tk.Frame):
    """Icerik sigmazsa dikey kaydirilan sayfa (kucuk ekranlar icin). Icerigi 'ic' cercevesine koy."""
    def __init__(self, ust):
        super().__init__(ust, bg=RENK["bg"])
        self.tuval = tk.Canvas(self, bg=RENK["bg"], highlightthickness=0, bd=0)
        self.cubuk = ttk.Scrollbar(self, orient="vertical", command=self.tuval.yview)
        self.tuval.configure(yscrollcommand=self.cubuk.set)
        self.tuval.pack(side="left", fill="both", expand=True)
        self.ic = tk.Frame(self.tuval, bg=RENK["bg"])
        self.pen = self.tuval.create_window(0, 0, window=self.ic, anchor="nw")
        self.cubuk_gorunur = False
        self.ic.bind("<Configure>", self._ic_boyut)
        self.tuval.bind("<Configure>", self._tuval_boyut)

    def _ic_boyut(self, _e=None):
        self.tuval.configure(scrollregion=self.tuval.bbox("all"))
        self._cubuk_ayarla()

    def _tuval_boyut(self, e):
        self.tuval.itemconfigure(self.pen, width=e.width)
        self._cubuk_ayarla()

    def _cubuk_ayarla(self):
        gerekli = self.ic.winfo_reqheight() > self.tuval.winfo_height() > 1
        if gerekli and not self.cubuk_gorunur:
            self.cubuk.pack(side="right", fill="y")
            self.cubuk_gorunur = True
        elif not gerekli and self.cubuk_gorunur:
            self.cubuk.pack_forget()
            self.cubuk_gorunur = False

    def tekerlek(self, e):
        if self.cubuk_gorunur:
            self.tuval.yview_scroll(int(-e.delta / 120), "units")


GECERSIZ_AD = re.compile(r'[\\/:*?"<>|]')


class Uygulama:
    def __init__(self):
        # Yuksek DPI ekranlarda dogru ortalama icin (Tk olusmadan once)
        try:
            ctypes.windll.shcore.SetProcessDpiAwareness(2)
        except Exception:
            try:
                ctypes.windll.user32.SetProcessDPIAware()
            except Exception:
                pass

        self.ilk_kurulum()
        self.ayar = Ayarlar()
        self.ayar.yukle()
        self.ayar.klasor_esitle()
        if self.ayar.genel["web_pin"] is None:           # Ilk acilista rastgele 4 haneli PIN uret
            sr = random.SystemRandom()
            self.ayar.genel["web_pin"] = "".join(sr.choice("0123456789") for _ in range(4))
        if not self.ayar.genel.get("dil"):               # Ilk acilis: Windows dilini sec
            self.ayar.genel["dil"] = sistem_dili()
        dil_ayarla(self.ayar.genel["dil"])
        self.ayar.kaydet()

        self.root = tk.Tk()
        self.root.title("PopDeck")
        try:                                              # Pencere simgesi (exe icinde de calisir)
            self.root.iconbitmap(os.path.join(getattr(sys, "_MEIPASS", ANA_DIZIN), "popdeck.ico"))
        except Exception:
            pass
        self.root.geometry("980x740")
        self.root.minsize(900, 640)

        self.kuyruk = queue.Queue()                      # keyboard thread'i -> tkinter koprusu
        self.deck = DeckDurumu(DECK_DOSYASI)             # Favoriler, siralar, son oynatilanlar
        self.oynatici = GifOynatici(self.root, self.ayar, self.kuyruk, on_oynat=self.oynatildi)
        self.yakalama = False                            # Tus atama penceresi acik mi?
        self.kapaniyor = False
        self.kayitli = []                                # Kayitli hotkey'ler
        self.kayit_zamanlayici = None
        self.ui_hazir = False
        self.web = WebSunucu(self)                       # Telefon kumandasi sunucusu
        self.web_kilit = threading.Lock()
        self.web_hatalar = {}                            # IP -> (yanlis sayisi, kilit bitis zamani)
        self.web_son = 0.0                               # Son telefon tetigi zamani
        self.web_son_ses = 0.0                           # Son soundboard tetigi zamani
        self.qr_foto = None
        self.fw_var = None                               # Guvenlik duvari kurali var mi
        self.fw_soruluyor = False
        self.son_qr = None
        self.secili_url = None                           # Telefon sekmesinde secilen adres
        self.ana = None                                  # Tum arayuzun kapsayici cercevesi (dil degisince yeniden kurulur)
        self.logo_foto = None
        self.aktif_sekme = 0
        self.ses_kuyrugu = []
        self.ses_isitiliyor = False
        self.bellek_bosaltildi = False                   # Bosta bellek bosaltildi: on yukleme yeniden baslamaz
        self.root.bind_all("<MouseWheel>", self.tekerlek_yonlendir)

        self.tema_uygula()
        self.arayuz_olustur()
        self.listeyi_yenile()
        self.tuslari_kaydet()
        self.ui_hazir = True
        self.web_yeniden_baslat()
        self.root.after(200, lambda: koyu_baslik(self.root))
        self.root.after(2000, self.onyukle_baslat)        # GIF/PNG ve sesleri arka planda hazirla

        self.root.protocol("WM_DELETE_WINDOW", self.pencere_kapatilirken)
        self.root.after(30, self.kontrol)
        self.root.after(3000, self.otomatik_yenile)
        if self.ayar.genel["gizli_basla"]:
            self.root.withdraw()

    # ---------------- ilk kurulum ----------------
    @staticmethod
    def ilk_kurulum():
        os.makedirs(ANA_KLASOR, exist_ok=True)
        if not os.path.exists(AYAR_DOSYASI) and not klasor_listesi():
            os.makedirs(os.path.join(ANA_KLASOR, "Genel"), exist_ok=True)
            a = Ayarlar()
            a.tuslar["Genel"] = "f9"                     # Ilk acilista Genel klasoru F9'a bagli
            a.kaydet()

    # ---------------- arayuz ----------------
    # ---------------- tema ----------------
    def tema_uygula(self):
        """Koyu tema. Ozel cizimli kartlar/dugmeler disinda kalan ttk parcalarini (kutu, kaydirma cubugu) boyar."""
        try:
            r = self.root
            c = RENK
            st = ttk.Style(r)
            try:
                st.theme_use("clam")
            except tk.TclError:
                pass
            r.configure(bg=c["bg"])
            f = (FONT, 10)
            st.configure(".", background=c["bg"], foreground=c["yazi"], fieldbackground=c["yuzey2"],
                         bordercolor=c["kenar"], lightcolor=c["kenar"], darkcolor=c["kenar"],
                         troughcolor=c["liste"], focuscolor=c["yuzey"], font=f)
            for ad in ("TEntry", "TSpinbox", "TCombobox"):
                st.configure(ad, fieldbackground=c["yuzey2"], foreground=c["yazi"], insertcolor=c["yazi"],
                             bordercolor=c["kenar"], arrowcolor=c["soluk"], padding=6, relief="flat")
            st.map("TCombobox", fieldbackground=[("readonly", c["yuzey2"])], foreground=[("readonly", c["yazi"])],
                   selectbackground=[("readonly", c["yuzey2"])], selectforeground=[("readonly", c["yazi"])],
                   bordercolor=[("focus", c["vurgu_kenar"])])
            st.map("TEntry", bordercolor=[("focus", c["vurgu_kenar"])])
            st.map("TSpinbox", bordercolor=[("focus", c["vurgu_kenar"])])
            st.configure("Vertical.TScrollbar", background=c["yuzey2"], troughcolor=c["liste"],
                         bordercolor=c["liste"], arrowcolor=c["soluk"], relief="flat", gripcount=0)
            st.map("Vertical.TScrollbar", background=[("active", c["kenar"])])
            r.option_add("*TCombobox*Listbox.background", c["yuzey2"])
            r.option_add("*TCombobox*Listbox.foreground", c["yazi"])
            r.option_add("*TCombobox*Listbox.selectBackground", c["vurgu_soft"])
            r.option_add("*TCombobox*Listbox.selectForeground", c["vurgu_acik"])
            r.option_add("*TCombobox*Listbox.font", f)
        except Exception as hata:
            print("Tema uygulanamadi:", hata)

    def logo_yukle(self, boyut=42):
        try:
            yol = os.path.join(getattr(sys, "_MEIPASS", ANA_DIZIN), "popdeck.ico")
            img = Image.open(yol).convert("RGBA").resize((boyut, boyut), YENIDEN_BOYUTLA)
            return ImageTk.PhotoImage(img, master=self.root)
        except Exception:
            return None

    def metin_sor(self, baslik, mesaj, ilk=""):
        """Koyu temali kucuk metin giris penceresi. Iptal edilirse None doner."""
        c = RENK
        pen = tk.Toplevel(self.root)
        pen.title(baslik)
        pen.configure(bg=c["bg"])
        pen.transient(self.root)
        pen.resizable(False, False)
        sonuc = {"v": None}
        tk.Label(pen, text=mesaj, bg=c["bg"], fg=c["yazi"], font=(FONT, 10), wraplength=400,
                 justify="left").pack(padx=24, pady=(22, 8), anchor="w")
        v = tk.StringVar(value=ilk)
        giris = ttk.Entry(pen, textvariable=v, width=48)
        giris.pack(padx=24, pady=6)

        def tamam(_olay=None):
            sonuc["v"] = v.get()
            pen.destroy()

        def iptal(_olay=None):
            pen.destroy()

        alt = tk.Frame(pen, bg=c["bg"])
        alt.pack(fill="x", padx=24, pady=(12, 20))
        Dugme(alt, L("Tamam"), tamam, "birincil", genislik=110, arka=c["bg"]).pack(side="right")
        Dugme(alt, L("İptal"), iptal, "normal", genislik=110, arka=c["bg"]).pack(side="right", padx=10)
        giris.bind("<Return>", tamam)
        pen.bind("<Escape>", iptal)
        pen.protocol("WM_DELETE_WINDOW", iptal)
        pen.update_idletasks()
        pen.geometry("+%d+%d" % (self.root.winfo_rootx() + 160, self.root.winfo_rooty() + 150))
        koyu_baslik(pen)
        giris.focus_set()
        try:
            pen.grab_set()
        except tk.TclError:
            pass
        self.root.wait_window(pen)
        return sonuc["v"]

    def tekerlek_yonlendir(self, e):
        """Fare tekerlegi: imlecin altindaki kaydirilabilir alani kaydirir."""
        try:
            w = self.root.winfo_containing(e.x_root, e.y_root)
        except (tk.TclError, KeyError):
            return
        while w is not None:
            if hasattr(w, "tekerlek"):
                w.tekerlek(e)
                return
            w = getattr(w, "master", None)

    def sekme_sec(self, i):
        self.aktif_sekme = i
        for j, sayfa in enumerate(self.sayfalar):
            if j == i:
                sayfa.grid()
            else:
                sayfa.grid_remove()

    def ayar_satiri(self, govde, satir, metin, kontrol_yap):
        """Kart icinde: solda aciklama, sagda (hizali) kontrol."""
        c = RENK
        tk.Label(govde, text=metin, bg=c["yuzey"], fg=c["yazi"], font=(FONT, 10), anchor="w", justify="left",
                 wraplength=250).grid(row=satir, column=0, sticky="w", pady=7)
        kontrol = kontrol_yap(govde)
        kontrol.grid(row=satir, column=1, sticky="e", padx=(16, 0), pady=7)
        return kontrol

    # ---------------- arayuz ----------------
    def arayuz_olustur(self):
        r = self.root
        g = self.ayar.genel
        c = RENK
        ana = tk.Frame(r, bg=c["bg"])
        ana.pack(fill="both", expand=True)
        self.ana = ana
        ana.columnconfigure(0, weight=1)
        ana.rowconfigure(2, weight=1)

        # ---------- Degiskenler ----------
        self.v_sure = tk.DoubleVar(value=g["sure"])
        self.v_boyut = tk.IntVar(value=int(round(g["boyut_orani"] * 100)))
        self.v_fade = tk.DoubleVar(value=g["fade_in"])
        self.v_boz = tk.BooleanVar(value=g["orani_boz"])
        self.v_tikla = tk.BooleanVar(value=g["tiklama_gecir"])
        self.v_ses = tk.BooleanVar(value=g["ses_aktif"])
        self.v_seviye = tk.IntVar(value=int(g["ses_seviyesi"]))
        self.v_gecikme = tk.IntVar(value=int(g.get("ses_gecikme", 0) or 0))
        self.v_kes = tk.BooleanVar(value=g["ses_gif_ile_kesil"])
        self.v_dinle = tk.BooleanVar(value=g["dinleme"])
        self.v_gizli = tk.BooleanVar(value=g["gizli_basla"])
        self.v_web = tk.BooleanVar(value=g["web_aktif"])
        self.v_port = tk.IntVar(value=int(g["web_port"]))
        self.v_pin = tk.StringVar(value=g["web_pin"] or "")
        self.v_url = tk.StringVar(value="")
        self.v_sonuna = tk.BooleanVar(value=g["gif_sonuna_kadar"])
        self.v_fwoto = tk.BooleanVar(value=g["fw_otomatik"])
        self.v_yukle = tk.BooleanVar(value=g.get("web_yukleme", True))
        self.v_onyukle = tk.BooleanVar(value=g["on_yukle"])
        self.v_onbellek = tk.IntVar(value=int(g["onbellek_mb"]))
        self.v_maxb = tk.DoubleVar(value=float(g.get("max_buyutme", 2.0)))
        self.v_fps = tk.IntVar(value=int(g.get("fps_siniri", 30)))
        self.v_bos = tk.IntVar(value=int(g.get("bos_sure_dk", 10)))

        # ---------- Ust bar: logo, baslik, dinleme durumu, dil ----------
        ust = tk.Frame(ana, bg=c["bg"])
        ust.grid(row=0, column=0, sticky="ew", padx=24, pady=(20, 12))
        ust.columnconfigure(1, weight=1)
        self.logo_foto = self.logo_yukle(48)
        if self.logo_foto:
            tk.Label(ust, image=self.logo_foto, bg=c["bg"], bd=0).grid(row=0, column=0, rowspan=2, padx=(0, 14))
        tk.Label(ust, text="PopDeck", bg=c["bg"], fg=c["yazi"], font=(FONT, 20, "bold")).grid(
            row=0, column=1, sticky="sw")
        tk.Label(ust, text=L("GIF • PNG • Ses kumandası"), bg=c["bg"], fg=c["soluk"], font=(FONT, 10)).grid(
            row=1, column=1, sticky="nw")
        sag = tk.Frame(ust, bg=c["bg"])
        sag.grid(row=0, column=2, rowspan=2, sticky="e")
        self.lbl_dinleme = tk.Label(sag, text="", bg=c["bg"], fg=c["yesil"], font=(FONT, 10, "bold"))
        self.lbl_dinleme.pack(side="left", padx=(0, 20))
        tk.Label(sag, text=L("Dil:"), bg=c["bg"], fg=c["soluk"], font=(FONT, 10)).pack(side="left", padx=(0, 8))
        self.v_dil = tk.StringVar(value=DIL_ADLARI[DIL_KODLARI.index(DIL)])
        cmb_dil = ttk.Combobox(sag, textvariable=self.v_dil, values=DIL_ADLARI, state="readonly", width=11)
        cmb_dil.pack(side="left")
        cmb_dil.bind("<<ComboboxSelected>>", self.dil_secildi)

        # ---------- Sekme cubugu (3 esit parca) + sayfalar ----------
        self.sekme_cubugu = SekmeCubugu(
            ana, [L("Klasörler"), L("Ayarlar"), L("Telefon Kumandası")], self.sekme_sec, c["bg"])
        self.sekme_cubugu.grid(row=1, column=0, sticky="ew", padx=24, pady=(0, 14))
        alan = tk.Frame(ana, bg=c["bg"])
        alan.grid(row=2, column=0, sticky="nsew", padx=24)
        alan.columnconfigure(0, weight=1)
        alan.rowconfigure(0, weight=1)
        s1 = tk.Frame(alan, bg=c["bg"])
        s2 = KaydirmaliSayfa(alan)
        s3 = KaydirmaliSayfa(alan)
        self.sayfalar = [s1, s2, s3]
        for sayfa in self.sayfalar:
            sayfa.grid(row=0, column=0, sticky="nsew")

        # ---------- Sayfa 1: Klasorler ----------
        s1.columnconfigure(0, weight=1)
        s1.rowconfigure(1, weight=1)
        tk.Label(s1, bg=c["bg"], fg=c["soluk"], font=(FONT, 10), justify="left", anchor="w", wraplength=880, text=L(
            "Her klasöre bir tuş ata. O tuşa basınca o klasörden rastgele görsel çıkar.\n"
            "Sesi olan görselle AYNI İSİMDE mp3/wav koy (komik.gif + komik.mp3). Ana klasör: {0}",
            ANA_KLASOR)).grid(row=0, column=0, sticky="ew", pady=(0, 10))
        panel = tk.Frame(s1, bg=c["liste"], highlightthickness=1, highlightbackground=c["kenar"],
                         highlightcolor=c["kenar"])
        panel.grid(row=1, column=0, sticky="nsew")
        self.agac = KlasorListesi(panel, None, lambda _ad: self.tus_ata())
        self.agac.pack(fill="both", expand=True, padx=3, pady=3)
        arac = tk.Frame(s1, bg=c["bg"])
        arac.grid(row=2, column=0, pady=(16, 6))                 # Tum dugme blogu ortada
        dugmeler = (
            ((L("Yeni Klasör"), self.yeni_klasor, "birincil"), (L("Tuş Ata"), self.tus_ata, "birincil"),
             (L("Dosya Ekle..."), self.dosya_ekle, "normal"), (L("Test Et"), self.test_et, "normal")),
            ((L("Adını Değiştir"), self.yeniden_adlandir, "normal"), (L("Tuşu Kaldır"), self.tus_kaldir, "normal"),
             (L("Klasörü Aç"), self.klasor_ac, "normal"), (L("Klasörü Sil"), self.klasor_sil, "tehlike")),
        )
        gen = max(172, max(yazi_genislik(d[0], 10, True) for sira in dugmeler for d in sira) + 44)
        for ri, sira in enumerate(dugmeler):
            for ki, (yazi, komut, stil) in enumerate(sira):
                Dugme(arac, yazi, komut, stil, genislik=gen, yukseklik=40, arka=c["bg"]).grid(
                    row=ri, column=ki, padx=6, pady=5)

        # ---------- Sayfa 2: Ayarlar (iki esit sutun, kartlar) ----------
        ic = s2.ic
        ic.columnconfigure(0, weight=1, uniform="ayar")
        ic.columnconfigure(1, weight=1, uniform="ayar")
        sol = tk.Frame(ic, bg=c["bg"])
        sag2 = tk.Frame(ic, bg=c["bg"])
        sol.grid(row=0, column=0, sticky="new", padx=(0, 9))
        sag2.grid(row=0, column=1, sticky="new", padx=(9, 0))
        sol.columnconfigure(0, weight=1)
        sag2.columnconfigure(0, weight=1)

        k_gor = Kart(sol, L("Görünüm"))
        k_gor.grid(row=0, column=0, sticky="ew", pady=(0, 16))
        k_gor.govde.columnconfigure(0, weight=1)
        yz = c["yuzey"]
        self.ayar_satiri(k_gor.govde, 0, L("GIF'i sonuna kadar oynat (süre sınırı yok)"),
                         lambda p: Anahtar(p, self.v_sonuna, yz))
        self.ayar_satiri(k_gor.govde, 1, L("PNG / sabit görsel süresi (sn):"),
                         lambda p: ttk.Spinbox(p, from_=0.5, to=60, increment=0.5, width=7, textvariable=self.v_sure))
        self.ayar_satiri(k_gor.govde, 2, L("Boyut (% ekran):"),
                         lambda p: Kaydirici(p, self.v_boyut, 10, 100, 1, 150, yz, "%"))
        self.ayar_satiri(k_gor.govde, 3, L("Belirme / fade-in (sn):"),
                         lambda p: ttk.Spinbox(p, from_=0, to=5, increment=0.1, width=7, textvariable=self.v_fade))
        self.ayar_satiri(k_gor.govde, 4, L("Oranı bozup tüm ekranı doldur"), lambda p: Anahtar(p, self.v_boz, yz))
        self.ayar_satiri(k_gor.govde, 5, L("Tıklamalar arkadaki oyuna geçsin"), lambda p: Anahtar(p, self.v_tikla, yz))
        self.ayar_satiri(k_gor.govde, 6, L("Program açılınca arayüzü gizle"), lambda p: Anahtar(p, self.v_gizli, yz))

        k_tus = Kart(sol, L("Özel Tuşlar"))
        k_tus.grid(row=1, column=0, sticky="ew")
        k_tus.govde.columnconfigure(0, weight=1)
        self.btn_arayuz = self.btn_cikis = self.btn_kapat = None
        self.btn_arayuz = self.ayar_satiri(k_tus.govde, 0, L("Arayüzü göster / gizle:"), lambda p: Dugme(
            p, "", lambda: self.ozel_tus_ata("arayuz_tusu"), "normal", genislik=200, arka=yz))
        self.btn_cikis = self.ayar_satiri(k_tus.govde, 1, L("Programı tamamen kapat:"), lambda p: Dugme(
            p, "", lambda: self.ozel_tus_ata("cikis_tusu"), "normal", genislik=200, arka=yz))
        self.btn_kapat = self.ayar_satiri(k_tus.govde, 2, L("Ekrandaki GIF'i kapat:"), lambda p: Dugme(
            p, "", lambda: self.ozel_tus_ata("kapat_tusu"), "normal", genislik=200, arka=yz))
        self.ozel_tus_yazilarini_guncelle()

        k_ses = Kart(sag2, L("Ses"))
        k_ses.grid(row=0, column=0, sticky="ew", pady=(0, 16))
        k_ses.govde.columnconfigure(0, weight=1)
        self.ayar_satiri(k_ses.govde, 0, L("Ses çal"), lambda p: Anahtar(p, self.v_ses, yz))
        self.ayar_satiri(k_ses.govde, 1, L("Ses seviyesi:"), lambda p: Kaydirici(p, self.v_seviye, 0, 100, 1, 150, yz, "%"))
        self.ayar_satiri(k_ses.govde, 2, L("Ses senkron gecikmesi (ms):"),
                         lambda p: Kaydirici(p, self.v_gecikme, 0, 500, 10, 150, yz, " ms"))
        tk.Label(k_ses.govde, text=L("Ses görselden erken başlıyorsa gecikmeyi artır."), bg=yz, fg=c["soluk"],
                 font=(FONT, 9), wraplength=360, justify="left", anchor="w").grid(
            row=3, column=0, columnspan=2, sticky="w")
        self.ayar_satiri(k_ses.govde, 4, L("Süre bitince ses de dursun"), lambda p: Anahtar(p, self.v_kes, yz))

        k_perf = Kart(sag2, L("Performans ve Önbellek"))
        k_perf.grid(row=1, column=0, sticky="ew")
        k_perf.govde.columnconfigure(0, weight=1)
        self.ayar_satiri(k_perf.govde, 0, L("Önceden yükle (ilk gösterim anında)"), lambda p: Anahtar(p, self.v_onyukle, yz))
        self.ayar_satiri(k_perf.govde, 1, L("Önbellek limiti:"),
                         lambda p: Kaydirici(p, self.v_onbellek, 200, 3000, 100, 150, yz, " MB"))
        self.ayar_satiri(k_perf.govde, 2, L("Maksimum büyütme (4x = sınırsız):"),
                         lambda p: Kaydirici(p, self.v_maxb, 1, 4, 0.5, 150, yz, "x"))
        self.ayar_satiri(k_perf.govde, 3, L("Akıcılık sınırı (FPS, 60 = sınırsız):"),
                         lambda p: Kaydirici(p, self.v_fps, 10, 60, 5, 150, yz, ""))
        self.ayar_satiri(k_perf.govde, 4, L("Boştayken belleği boşalt (dk, 0 = kapalı):"),
                         lambda p: Kaydirici(p, self.v_bos, 0, 30, 1, 150, yz, ""))
        self.lbl_onbellek = tk.Label(k_perf.govde, text="", bg=yz, fg=c["soluk"], font=(FONT, 9),
                                     wraplength=360, justify="left", anchor="w")
        self.lbl_onbellek.grid(row=5, column=0, columnspan=2, sticky="w", pady=(4, 8))
        Dugme(k_perf.govde, L("Önbelleği temizle"), self.onbellegi_temizle, "hayalet", genislik=190,
              arka=yz).grid(row=6, column=0, columnspan=2, sticky="e")

        # ---------- Sayfa 3: Telefon kumandasi ----------
        k_tel = Kart(s3.ic, L("Telefon Kumandası (aynı Wi-Fi)"))
        k_tel.pack(fill="x")
        k_tel.govde.columnconfigure(0, weight=1)
        sol_t = tk.Frame(k_tel.govde, bg=yz)
        sag_t = tk.Frame(k_tel.govde, bg=yz)
        sol_t.grid(row=0, column=0, sticky="nw")
        sag_t.grid(row=0, column=1, sticky="n", padx=(36, 0))
        sol_t.columnconfigure(0, weight=1)
        self.ayar_satiri(sol_t, 0, L("Telefon kumandası açık"), lambda p: Anahtar(p, self.v_web, yz))
        self.ayar_satiri(sol_t, 1, L("Port:"),
                         lambda p: ttk.Spinbox(p, from_=1024, to=65535, width=8, textvariable=self.v_port))
        self.ayar_satiri(sol_t, 2, L("PIN (boş = şifresiz):"), lambda p: ttk.Entry(p, width=12, textvariable=self.v_pin))
        tk.Label(sol_t, text=L("Telefonda açılacak adres:"), bg=yz, fg=c["yazi"], font=(FONT, 10), anchor="w").grid(
            row=3, column=0, columnspan=2, sticky="w", pady=(14, 4))
        adres = tk.Frame(sol_t, bg=yz)
        adres.grid(row=4, column=0, columnspan=2, sticky="ew")
        adres.columnconfigure(0, weight=1)
        self.cmb_url = ttk.Combobox(adres, textvariable=self.v_url, state="readonly", width=30)
        self.cmb_url.grid(row=0, column=0, sticky="ew")
        self.cmb_url.bind("<<ComboboxSelected>>", self.web_adres_secildi)
        Dugme(adres, L("Kopyala"), self.url_kopyala, "normal", genislik=110, yukseklik=34, arka=yz).grid(
            row=0, column=1, padx=(10, 0))
        fw = tk.Frame(sol_t, bg=yz)
        fw.grid(row=5, column=0, columnspan=2, sticky="ew", pady=(16, 0))
        Dugme(fw, L("Güvenlik duvarına izin ver (otomatik)"), self.guvenlik_duvari_ac, "birincil",
              arka=yz).pack(side="left")
        self.lbl_fw = tk.Label(fw, text=L("Güvenlik duvarı: kontrol ediliyor..."), bg=yz, fg=c["soluk"],
                               font=(FONT, 10, "bold"))
        self.lbl_fw.pack(side="left", padx=14)
        self.ayar_satiri(sol_t, 6, L("İzin eksikse açılışta otomatik iste"), lambda p: Anahtar(p, self.v_fwoto, yz))
        self.ayar_satiri(sol_t, 7, L("Telefondan dosya yüklemeye izin ver"), lambda p: Anahtar(p, self.v_yukle, yz))
        tk.Label(sol_t, bg=yz, fg=c["soluk"], font=(FONT, 9), justify="left", anchor="w", wraplength=430, text=L(
            "1) Telefon ve bilgisayar AYNI Wi-Fi'ye bağlı olmalı (mobil veri/VPN kapalı).\n"
            "2) QR kodu okut ya da adresi telefonun tarayıcısına http:// ile yaz.\n"
            "3) İzin yoksa program açılışta kendisi sorar; ya da düğmeye bas ve onaya Evet de.\n"
            "4) Adres yanlışsa listeden başka adres seç (QR da değişir).")).grid(
            row=8, column=0, columnspan=2, sticky="w", pady=(12, 0))
        self.lbl_qr = tk.Label(sag_t, text="", bg=yz, fg=c["soluk"], justify="center")
        self.lbl_qr.pack()
        tk.Label(sag_t, text=L("Telefon kamerasıyla okut"), bg=yz, fg=c["soluk"], font=(FONT, 9)).pack(pady=(8, 0))

        for v in (self.v_sure, self.v_boyut, self.v_fade, self.v_boz, self.v_tikla, self.v_ses, self.v_seviye,
                  self.v_gecikme, self.v_kes, self.v_dinle, self.v_gizli, self.v_web, self.v_port, self.v_pin, self.v_yukle,
                  self.v_sonuna, self.v_fwoto, self.v_onyukle, self.v_onbellek, self.v_maxb, self.v_fps,
                  self.v_bos):
            v.trace_add("write", self.ayar_degisti)
        self.v_dinle.trace_add("write", lambda *_a: self.dinleme_guncelle())

        # ---------- Alt bar ----------
        alt = tk.Frame(ana, bg=c["bg"])
        alt.grid(row=3, column=0, sticky="ew", padx=24, pady=(14, 6))
        alt.columnconfigure(1, weight=1)
        anahtar_satiri(alt, L("Tuş dinleme"), self.v_dinle, c["bg"]).grid(row=0, column=0, sticky="w")
        Dugme(alt, L("Arka Plana Gizle"), self.gizle, "normal", genislik=190, yukseklik=40, arka=c["bg"]).grid(
            row=0, column=2, padx=(0, 10))
        Dugme(alt, L("Programı Kapat"), self.tamamen_kapat, "tehlike", genislik=190, yukseklik=40,
              arka=c["bg"]).grid(row=0, column=3)
        self.durum_var = tk.StringVar(value=L("Hazır."))
        tk.Label(ana, textvariable=self.durum_var, bg=c["bg"], fg=c["soluk"], font=(FONT, 9), anchor="w").grid(
            row=4, column=0, sticky="w", padx=26, pady=(0, 12))
        self.secili_url = self.v_url.get()
        self.sekme_cubugu.secili = self.aktif_sekme
        self.sekme_sec(self.aktif_sekme)
        self.dinleme_guncelle()

    def dinleme_guncelle(self):
        """Ust bardaki 'Dinleme acik/kapali' gostergesi."""
        try:
            acik = bool(self.v_dinle.get())
            self.lbl_dinleme.configure(
                text="●  " + (L("Dinleme açık") if acik else L("Dinleme kapalı")),
                fg=RENK["yesil"] if acik else RENK["kirmizi"])
        except (tk.TclError, AttributeError):
            pass

    def dil_secildi(self, _olay=None):
        ad = self.v_dil.get()
        if ad not in DIL_ADLARI:
            return
        kod = DIL_KODLARI[DIL_ADLARI.index(ad)]
        if kod == DIL:
            return
        self.ayar.genel["dil"] = kod
        self.ayar.kaydet()
        dil_ayarla(kod)
        self.root.after(40, self.arayuzu_yeniden_kur)    # Combobox olayi bitince kur

    def arayuzu_yeniden_kur(self):
        """Dil degisince tum arayuzu yeni dilde yeniden olusturur (ayarlar ve secimler korunur)."""
        self.ui_hazir = False
        if self.kayit_zamanlayici:
            try:
                self.root.after_cancel(self.kayit_zamanlayici)
            except (tk.TclError, ValueError):
                pass
            self.kayit_zamanlayici = None
        secili = self.agac.secili if getattr(self, "agac", None) else None
        try:
            self.ana.destroy()                           # Sadece arayuz; gorsel pencereleri etkilenmez
        except tk.TclError:
            pass
        self.qr_foto = None
        self.son_qr = None
        self.arayuz_olustur()
        self.listeyi_yenile(sec=secili)
        self.fw_durum_guncelle()
        self.web_bilgi_guncelle()
        self.onbellek_etiketi_guncelle()
        self.ui_hazir = True
        self.root.after(100, lambda: koyu_baslik(self.root))

    def durum(self, mesaj):
        self.durum_var.set(mesaj)

    # ---------------- ayarlar ----------------
    def ayar_degisti(self, *_):
        if not getattr(self, "ui_hazir", False):
            return
        if self.kayit_zamanlayici:
            self.root.after_cancel(self.kayit_zamanlayici)
        self.kayit_zamanlayici = self.root.after(300, self.ayarlari_kaydet)

    def ayarlari_kaydet(self):
        self.kayit_zamanlayici = None
        g = self.ayar.genel

        def oku(degisken, tur, varsayilan):
            try:
                return tur(degisken.get())
            except (tk.TclError, ValueError):
                return varsayilan

        g["sure"] = max(0.5, oku(self.v_sure, float, g["sure"]))
        g["boyut_orani"] = max(0.1, min(1.0, oku(self.v_boyut, int, 100) / 100))
        g["fade_in"] = max(0.0, oku(self.v_fade, float, g["fade_in"]))
        g["orani_boz"] = oku(self.v_boz, bool, g["orani_boz"])
        g["tiklama_gecir"] = oku(self.v_tikla, bool, g["tiklama_gecir"])
        g["ses_aktif"] = oku(self.v_ses, bool, g["ses_aktif"])
        g["ses_seviyesi"] = max(0, min(100, oku(self.v_seviye, int, g["ses_seviyesi"])))
        g["ses_gecikme"] = max(0, min(500, oku(self.v_gecikme, int, g["ses_gecikme"])))
        eski_onb = (g["boyut_orani"], g["orani_boz"], g["max_buyutme"], g["fps_siniri"], g["onbellek_mb"], g["on_yukle"])
        g["max_buyutme"] = max(1.0, min(4.0, oku(self.v_maxb, float, g["max_buyutme"])))
        g["fps_siniri"] = max(10, min(60, oku(self.v_fps, int, g["fps_siniri"])))
        g["bos_sure_dk"] = max(0, min(30, oku(self.v_bos, int, g["bos_sure_dk"])))
        g["on_yukle"] = oku(self.v_onyukle, bool, g["on_yukle"])
        g["onbellek_mb"] = max(200, min(3000, oku(self.v_onbellek, int, g["onbellek_mb"])))
        g["ses_gif_ile_kesil"] = oku(self.v_kes, bool, g["ses_gif_ile_kesil"])
        g["dinleme"] = oku(self.v_dinle, bool, g["dinleme"])
        g["gizli_basla"] = oku(self.v_gizli, bool, g["gizli_basla"])
        eski_web = (g["web_aktif"], g["web_port"])
        g["web_aktif"] = oku(self.v_web, bool, g["web_aktif"])
        port = oku(self.v_port, int, g["web_port"])
        g["web_port"] = port if 1024 <= port <= 65535 else g["web_port"]
        g["web_pin"] = oku(self.v_pin, str, "").strip()[:12]
        g["web_yukleme"] = oku(self.v_yukle, bool, g.get("web_yukleme", True))
        g["gif_sonuna_kadar"] = oku(self.v_sonuna, bool, g["gif_sonuna_kadar"])
        g["fw_otomatik"] = oku(self.v_fwoto, bool, g["fw_otomatik"])
        self.ayar.kaydet()
        self.durum(L("Ayarlar kaydedildi."))
        if eski_web != (g["web_aktif"], g["web_port"]):
            self.web_yeniden_baslat()
        else:
            self.web_bilgi_guncelle()
        yeni_onb = (g["boyut_orani"], g["orani_boz"], g["max_buyutme"], g["fps_siniri"], g["onbellek_mb"], g["on_yukle"])
        if yeni_onb != eski_onb:                         # Boyut/limit degisti: onbellegi yeni ayara gore yenile
            self.bellek_bosaltildi = False
            if yeni_onb[:4] != eski_onb[:4]:
                self.oynatici.temizle()                  # Eski boyuttaki kareler artik ise yaramaz
            self.oynatici.limit_degisti()
            self.root.after(600, self.onyukle_baslat)
            self.onbellek_etiketi_guncelle()

    def oynatildi(self, yol):
        """Bir gorsel ekrana gelince 'Son oynatilanlar'a ekler (tus, telefon, test: hepsi)."""
        self.deck.son_ekle(f"{os.path.basename(os.path.dirname(yol))}/{os.path.basename(yol)}")

    # ---------------- telefon kumandasi ----------------
    def web_yetki(self, ip, verilen):
        """(Web thread'inden cagrilir) PIN kontrolu + yanlis denemede gecici kilit."""
        pin = str(self.ayar.genel.get("web_pin") or "")
        if not pin:
            return True, None, 200
        simdi = time.time()
        with self.web_kilit:
            sayac, kilit_bitis = self.web_hatalar.get(ip, (0, 0.0))
            if simdi < kilit_bitis:
                return False, L("Çok fazla yanlış deneme. 1 dakika bekle."), 429
            if hmac.compare_digest(str(verilen or "").encode("utf-8"), pin.encode("utf-8")):
                self.web_hatalar.pop(ip, None)
                return True, None, 200
            sayac += 1
            kilit_bitis = 0.0
            if sayac >= 5:                               # 5 yanlis -> 60 sn kilit
                sayac, kilit_bitis = 0, simdi + 60
            self.web_hatalar[ip] = (sayac, kilit_bitis)
        return False, L("PIN yanlış"), 401

    def web_klasor_listesi(self):
        liste = []
        for ad, tus in list(self.ayar.tuslar.items()):
            gorseller, sesler = klasor_dosyalari(ad)
            liste.append({"ad": ad, "tus": tus.upper() if tus else "",
                          "gorsel": len(gorseller), "ses": len(sesler)})
        return liste

    def _klasor_ogeleri(self, ad):
        """Bir klasorun Deck ogeleri: (dosya_adi, tam_yol, tur). tur: 'gorsel' ya da 'ses'.
        Sesler: sadece hicbir gorselle ayni isimde olmayanlar soundboard tusu olur."""
        if ad not in self.ayar.tuslar:
            return []
        gorseller, sesler = klasor_dosyalari(ad)
        tabanlar = {os.path.splitext(g)[0].lower() for g in gorseller}
        sonuc = [(os.path.basename(g), g, "gorsel") for g in gorseller]
        sonuc += [(os.path.basename(x), x, "ses") for x in sesler
                  if os.path.splitext(x)[0].lower() not in tabanlar]
        return sonuc

    def web_gorsel_listesi(self):
        """Deck sekmesi icin: klasorler (kayitli siraya gore), favoriler ve son oynatilanlar."""
        favoriler, sira, son = self.deck.anlik()
        klasorler, mevcut = [], set()
        for ad, tus in list(self.ayar.tuslar.items()):
            ogeler = []
            for dosya, yol, _tur in self._klasor_ogeleri(ad):
                try:
                    surum = int(os.path.getmtime(yol))
                except OSError:
                    continue
                ogeler.append({"d": dosya, "t": os.path.splitext(dosya)[1][1:].lower(), "v": surum})
                mevcut.add(f"{ad}/{dosya}")
            sira_k = {n: i for i, n in enumerate(sira.get(ad, []))}
            ogeler.sort(key=lambda o: (sira_k.get(o["d"], 10 ** 9), o["d"].lower()))
            klasorler.append({"ad": ad, "tus": tus.upper() if tus else "", "ogeler": ogeler})
        return {"klasorler": klasorler,
                "favoriler": [i for i in favoriler if i in mevcut],
                "son": [i for i in son if i in mevcut]}

    def web_oge(self, ad, dosya):
        """(yol, tur) dondurur. Sadece klasorde gercekten var olan dosyalar kabul edilir."""
        for d, yol, tur in self._klasor_ogeleri(ad):
            if d == dosya:
                return yol, tur
        return None, None

    def web_dosya_yolu(self, ad, dosya):
        """Onizleme icin: sadece gorseller."""
        yol, tur = self.web_oge(ad, dosya)
        return yol if tur == "gorsel" else None

    def web_deck_islem(self, veri):
        """(Web thread'inden) favori ekleme/cikarma ve siralama kaydi."""
        islem = str(veri.get("islem", ""))
        if islem == "favori":
            kimlik = str(veri.get("id", ""))
            deger = bool(veri.get("deger"))
            if deger:
                klasor, _, dosya = kimlik.partition("/")
                if not self.web_oge(klasor, dosya)[0]:
                    return False, L("Öğe bulunamadı")
            self.deck.favori_ayarla(kimlik, deger)
            return True, ""
        if islem == "favori_sira":
            idler = veri.get("idler")
            if not isinstance(idler, list):
                return False, L("Geçersiz liste")
            self.deck.favori_sirala([str(i) for i in idler])
            return True, ""
        if islem == "sira":
            ad = str(veri.get("klasor", ""))
            dosyalar = veri.get("dosyalar")
            if ad not in self.ayar.tuslar or not isinstance(dosyalar, list):
                return False, L("Geçersiz istek")
            var = {d for d, _, _ in self._klasor_ogeleri(ad)}
            self.deck.sira_ayarla(ad, [str(d) for d in dosyalar if str(d) in var])
            return True, ""
        return False, L("Bilinmeyen işlem")

    def web_yukle(self, klasor, dosya, yeni, okuyucu, uzunluk):
        """(Web thread'inden) Yuklenen dosyayi hedef klasore kaydeder. Dondurur: (ok, dosya_adi|hata, klasor)."""
        if not self.ayar.genel.get("web_yukleme", True):
            return False, L("Telefondan dosya yükleme kapalı"), klasor
        if uzunluk <= 0 or uzunluk > YUKLEME_LIMIT_MB * 1048576:
            return False, L("Dosya çok büyük (en fazla {0} MB)", YUKLEME_LIMIT_MB), klasor
        klasor = (klasor or "").strip()
        esles = [k for k in klasor_listesi() if k.lower() == klasor.lower()]
        if esles:
            klasor = esles[0]
        else:                                            # Yeni klasor: sadece 'yeni' istenmisse ve ad gecerliyse
            if not yeni:
                return False, L("Klasör bulunamadı"), klasor
            if (not klasor or GECERSIZ_AD.search(klasor) or klasor.startswith((".", "_")) or klasor.endswith(".")):
                return False, L("Geçersiz ad"), klasor
            try:
                os.makedirs(os.path.join(ANA_KLASOR, klasor))
            except OSError:
                return False, L("Yükleme başarısız"), klasor
        ok, sonuc = yukleme_kaydet(os.path.join(ANA_KLASOR, klasor), dosya, okuyucu, uzunluk)
        if ok:
            self.kuyruk.put(("yuklendi", klasor, sonuc))   # Arayuz listesi / on yukleme ana thread'de yenilenir
        return ok, sonuc, klasor

    def web_tetik(self, ad, dosya=None):
        """(Web thread'inden cagrilir) Telefondan gelen tetigi dogrular, ana thread'e iletir."""
        simdi = time.time()
        yol = None
        if dosya:
            yol, tur = self.web_oge(ad, dosya)
            if not yol:
                return False, L("Dosya bulunamadı")
            if tur == "ses":                             # Soundboard: ekranda gorsel yok, sadece ses
                if simdi - self.web_son_ses < 0.2:
                    return False, L("Biraz yavaş :)")
                self.web_son_ses = simdi
                self.kuyruk.put(("web_ses", ad, yol))
                return True, dosya
        if simdi - self.web_son < 0.4:                   # Cok hizli ust uste basmayi engelle
            return False, L("Biraz yavaş :)")
        kes = bool(dosya)                                # Deck'ten secilen dosya, oynayani keser
        if self.oynatici.mesgul and not (kes and self.oynatici.oturum is not None):
            return False, L("Şu an bir GIF oynuyor")
        if dosya:
            sonuc = dosya
        elif ad == "__rastgele__":
            tum = []
            for a in list(self.ayar.tuslar):
                tum.extend(klasor_dosyalari(a)[0])
            if not tum:
                return False, L("Hiçbir klasörde görsel yok")
            yol = RASTGELE.sec("__hepsi__", tum)         # Tum gorseller arasinda akilli rastgele
            ad = os.path.basename(os.path.dirname(yol))
            sonuc = ad
        elif ad not in self.ayar.tuslar:                 # Sadece bilinen klasor adlari kabul edilir
            return False, L("Klasör bulunamadı")
        elif not klasor_dosyalari(ad)[0]:
            return False, L("'{0}' klasörü boş", ad)
        else:
            sonuc = ad
        self.web_son = simdi
        self.kuyruk.put(("web_tetik", ad, yol, kes))
        return True, sonuc

    def web_durdur(self):
        """(Web thread'inden) Telefondaki Durdur butonu."""
        self.kuyruk.put(("durdur", None))
        return True

    def web_yeniden_baslat(self):
        g = self.ayar.genel
        self.web.durdur()
        basladi = False
        if g["web_aktif"]:
            basladi = self.web.baslat(g["web_port"])
            if not basladi:
                self.durum(L("Telefon kumandası başlatılamadı: port kullanımda olabilir, başka port dene."))
        self.web_bilgi_guncelle()
        if basladi:
            self.root.after(1500, self.fw_kontrol_et)    # Guvenlik duvari izni var mi?

    def web_bilgi_guncelle(self):
        g = self.ayar.genel
        if not g["web_aktif"]:
            self.cmb_url.configure(values=[])
            self.v_url.set(L("Kapalı"))
            self.qr_goster(None)
            return
        if self.web.sunucu is None:
            self.cmb_url.configure(values=[])
            self.v_url.set(L("Başlatılamadı (port kullanımda?)"))
            self.qr_goster(None)
            return
        urller = [f"http://{ip}:{g['web_port']}" for ip in yerel_ip_listesi()]
        if self.secili_url not in urller:
            self.secili_url = urller[0]
        url = self.secili_url
        self.cmb_url.configure(values=urller)
        self.v_url.set(url)
        pin = str(g.get("web_pin") or "")
        self.qr_goster(url + ("/?pin=" + quote(pin) if pin else ""))   # QR'da PIN de var: tek okutmayla giris

    def web_adres_secildi(self, _olay=None):
        self.secili_url = self.v_url.get()
        self.web_bilgi_guncelle()

    # ---------------- guvenlik duvari ----------------
    def fw_kural_adi(self):
        return f"PopDeck {int(self.ayar.genel['web_port'])}"

    def fw_kural_var(self):
        """True: izin kurali var | False: yok | None: kontrol edilemedi. (Yonetici gerekmez)"""
        if os.name != "nt":
            return None
        try:
            r = subprocess.run(["netsh", "advfirewall", "firewall", "show", "rule", f"name={self.fw_kural_adi()}"],
                               capture_output=True, timeout=10, creationflags=0x08000000)
            return r.returncode == 0
        except Exception:
            return None

    def fw_durum_guncelle(self):
        self.fw_var = self.fw_kural_var()
        try:
            if self.fw_var is True:
                self.lbl_fw.configure(text=L("Güvenlik duvarı: izin var ✓"), fg=RENK["yesil"])
            elif self.fw_var is False:
                self.lbl_fw.configure(text=L("Güvenlik duvarı: izin YOK ✗"), fg=RENK["kirmizi"])
            else:
                self.lbl_fw.configure(text=L("Güvenlik duvarı: kontrol edilemedi"), fg=RENK["soluk"])
        except (tk.TclError, AttributeError):
            pass

    def fw_kontrol_et(self):
        """Acilista ve port degisince: izin eksikse kullaniciya sorup otomatik ekler."""
        if self.kapaniyor or os.name != "nt":
            return
        self.fw_durum_guncelle()
        g = self.ayar.genel
        if (self.fw_var is False and g["web_aktif"] and self.web.sunucu is not None and g["fw_otomatik"]
                and not self.fw_soruluyor and self.root.state() != "withdrawn"):
            self.fw_soruluyor = True
            try:
                evet = messagebox.askyesno(
                    L("Güvenlik duvarı izni"),
                    L("Telefon kumandasının çalışması için Windows güvenlik duvarında izin gerekiyor.\n\n"
                      "Şimdi otomatik eklensin mi?\n"
                      "(Evet'e basınca Windows bir onay penceresi gösterir, orada da Evet de.)"),
                    parent=self.root)
            finally:
                self.fw_soruluyor = False
            if evet:
                self.guvenlik_duvari_ac()
            else:
                self.v_fwoto.set(False)                  # Bir daha sorma (dugme her zaman var)

    def guvenlik_duvari_ac(self):
        """Portu tum ag turleri icin acar + bu programa ait engelleme kurallarini temizler."""
        if os.name != "nt":
            messagebox.showinfo(L("Güvenlik duvarı"), L("Bu özellik sadece Windows'ta çalışır."), parent=self.root)
            return
        port = int(self.ayar.genel["web_port"])
        ad = self.fw_kural_adi()
        exe = sys.executable
        komutlar = [
            ["netsh", "advfirewall", "firewall", "delete", "rule", f"name={ad}"],
            # Windows'un ilk calistirmada "Iptal"e basinca kendiliginden actigi ENGEL kurallari izni gecersiz kilar:
            ["netsh", "advfirewall", "firewall", "delete", "rule", "name=all", "dir=in", f"program={exe}"],
            ["netsh", "advfirewall", "firewall", "add", "rule", f"name={ad}", "dir=in", "action=allow",
             "protocol=TCP", f"localport={port}", "profile=any"],
        ]
        baslatildi = False
        try:
            if ctypes.windll.shell32.IsUserAnAdmin():    # Zaten yonetici: dogrudan calistir
                for k in komutlar:
                    subprocess.run(k, capture_output=True, timeout=30, creationflags=0x08000000)
                baslatildi = True
            else:                                        # Degilse Windows onay penceresi (UAC) ile calistir
                kacis = exe.replace("'", "''")
                betik = (f"$p = '{kacis}'\n"
                         f"netsh advfirewall firewall delete rule \"name={ad}\" | Out-Null\n"
                         f"netsh advfirewall firewall delete rule name=all dir=in \"program=$p\" | Out-Null\n"
                         f"netsh advfirewall firewall add rule \"name={ad}\" dir=in action=allow protocol=TCP "
                         f"localport={port} profile=any | Out-Null\n")
                kodlu = base64.b64encode(betik.encode("utf-16-le")).decode("ascii")
                sonuc = ctypes.windll.shell32.ShellExecuteW(
                    None, "runas", "powershell.exe",
                    f"-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -EncodedCommand {kodlu}", None, 0)
                baslatildi = sonuc > 32
        except Exception as hata:
            print("Guvenlik duvari hatasi:", hata)
        if not baslatildi:
            messagebox.showwarning(
                L("Güvenlik duvarı"),
                L("İzin eklenemedi (onay penceresine Evet demedin olabilir).\n\n"
                  "Alternatif: firewall_ac.bat dosyasına sağ tıkla → 'Yönetici olarak çalıştır'."),
                parent=self.root)
            self.fw_durum_guncelle()
            return
        self.durum(L("Güvenlik duvarı izni ekleniyor..."))
        self.root.after(1200, lambda: self.fw_dogrula(0, port))

    def fw_dogrula(self, deneme, port):
        """Kural eklendi mi diye 1'er saniye arayla bakar (UAC onayi biraz zaman alir)."""
        if self.kapaniyor:
            return
        if self.fw_kural_var():
            self.fw_durum_guncelle()
            self.durum(L("Güvenlik duvarı izni verildi ✓"))
            messagebox.showinfo(L("Güvenlik duvarı"),
                                L("{0} portu için izin eklendi.\nŞimdi telefondan sayfayı yenile.", port),
                                parent=self.root)
        elif deneme < 25:
            self.root.after(1000, lambda: self.fw_dogrula(deneme + 1, port))
        else:
            self.fw_durum_guncelle()
            self.durum(L("Güvenlik duvarı izni doğrulanamadı (onay verilmedi olabilir)."))

    def qr_goster(self, metin):
        if metin == self.son_qr:
            return
        self.son_qr = metin
        if not metin:
            self.lbl_qr.configure(image="", text="")
            self.qr_foto = None
            return
        try:
            import qrcode
            qr = qrcode.QRCode(border=2, box_size=6)
            qr.add_data(metin)
            qr.make(fit=True)
            resim = qr.make_image(fill_color="black", back_color="white")
            resim = resim.get_image() if hasattr(resim, "get_image") else resim
            resim = resim.convert("RGB").resize((200, 200), Image.NEAREST)
            self.qr_foto = ImageTk.PhotoImage(resim, master=self.root)
            self.lbl_qr.configure(image=self.qr_foto, text="")
        except Exception:
            self.qr_foto = None
            self.lbl_qr.configure(image="", text=L("QR yok (qrcode kurulu değil)"))

    def url_kopyala(self):
        self.root.clipboard_clear()
        self.root.clipboard_append(self.v_url.get())
        self.durum(L("Adres kopyalandı."))

    # ---------------- klasor listesi ----------------
    def tus_yazisi(self, tus):
        return tus.upper() if tus else L("— atanmadı —")

    def icerik_yazisi(self, ad):
        gorseller, sesler = klasor_dosyalari(ad)
        return L("{0} görsel, {1} ses", len(gorseller), len(sesler))

    def listeyi_yenile(self, sec=None):
        if self.ayar.klasor_esitle():
            self.ayar.kaydet()
            if getattr(self, "ui_hazir", False):
                self.tuslari_kaydet()
        satirlar = [(ad, self.tus_yazisi(tus), bool(tus), self.icerik_yazisi(ad))
                    for ad, tus in self.ayar.tuslar.items()]
        self.agac.doldur(satirlar, sec)

    def otomatik_yenile(self):
        if self.kapaniyor:
            return
        try:
            if self.root.state() != "withdrawn" and not self.yakalama:
                self.listeyi_yenile()
                self.onbellek_etiketi_guncelle()
        except tk.TclError:
            return
        dk = int(self.ayar.genel.get("bos_sure_dk", 0) or 0)
        if dk > 0 and self.oynatici.bosta_mi(dk):        # Uzun suredir kullanilmadi: RAM'i bosalt
            self.oynatici.bosalt()
            SES.hepsini_kapat()
            self.bellek_bosaltildi = True
            bellek_kirp()
            self.durum(L("Bellek boşaltıldı (boşta)."))
        self.onyukle_baslat()                            # Yeni/degisen dosyalari da hazirla (gizliyken de)
        self.root.after(3000, self.otomatik_yenile)

    # ---------------- on yukleme / onbellek ----------------
    def tum_dosyalar(self):
        gorseller, sesler = [], []
        for ad in list(self.ayar.tuslar):
            g, s = klasor_dosyalari(ad)
            gorseller += g
            sesler += s
        return gorseller, sesler

    def onyukle_baslat(self):
        """GIF/PNG kareleri ve sesler arka planda/bos zamanda hazirlanir; silinenler onbellekten atilir."""
        if self.kapaniyor:
            return
        gorseller, sesler = self.tum_dosyalar()
        self.oynatici.eskileri_at(set(gorseller))
        if self.ayar.genel.get("on_yukle", True) and not self.bellek_bosaltildi:
            self.oynatici.onyukle_tetikle(gorseller)
            self.ses_kuyrugu = [y for y in sesler if y not in SES.acik][:max(0, SES_ONBELLEK_ADET - len(SES.acik))]
            if self.ses_kuyrugu and not self.ses_isitiliyor:
                self.ses_isitiliyor = True
                self.root.after(80, self.ses_isit_adimi)

    def ses_isit_adimi(self):
        if self.kapaniyor:
            return
        if self.ses_kuyrugu and not self.oynatici.mesgul:
            SES.isit(self.ses_kuyrugu.pop(0))
        if self.ses_kuyrugu:
            self.root.after(80, self.ses_isit_adimi)
        else:
            self.ses_isitiliyor = False

    def onbellek_etiketi_guncelle(self):
        try:
            gorseller, _ = self.tum_dosyalar()
            mb, limit, hazir, toplam = self.oynatici.ozet(gorseller)
            ram = islem_bellegi_mb()
            self.lbl_onbellek.configure(
                text=L("Önbellek: {0} MB / {1} MB • {2}/{3} görsel hazır", mb, limit, hazir, toplam)
                + ((" • " + L("Program belleği: {0} MB", round(ram))) if ram else ""))
        except (tk.TclError, AttributeError):
            pass

    def onbellegi_temizle(self):
        self.oynatici.temizle()
        SES.hepsini_kapat()
        self.bellek_bosaltildi = False
        bellek_kirp()
        self.durum(L("Önbellek temizlendi."))
        self.onbellek_etiketi_guncelle()
        self.root.after(800, self.onyukle_baslat)

    def secili(self):
        sec = self.agac.selection()
        if not sec:
            messagebox.showinfo(L("Klasör seç"), L("Önce listeden bir klasör seç."), parent=self.root)
            return None
        return sec[0]

    # ---------------- klasor islemleri ----------------
    def ad_dogrula(self, ad, eski=None):
        ad = (ad or "").strip()
        if not ad:
            return None
        if GECERSIZ_AD.search(ad) or ad.startswith((".", "_")) or ad.endswith("."):
            messagebox.showwarning(
                L("Geçersiz ad"),
                L("Klasör adı geçersiz. Şu karakterler olamaz: \\ / : * ? < > | ve ad '.' ya da '_' ile başlayamaz."),
                parent=self.root)
            return None
        if ad.lower() != (eski or "").lower() and any(
                k.lower() == ad.lower() for k in self.ayar.tuslar):
            messagebox.showwarning(L("Zaten var"), L("'{0}' adlı bir klasör zaten var.", ad), parent=self.root)
            return None
        return ad

    def yeni_klasor(self):
        ad = self.metin_sor(L("Yeni Klasör"), L("Klasör adı (örn: Komik, Alkış, Korku):"))
        ad = self.ad_dogrula(ad)
        if not ad:
            return
        try:
            os.makedirs(os.path.join(ANA_KLASOR, ad))
        except OSError as hata:
            messagebox.showerror(L("Hata"), L("Klasör oluşturulamadı:\n{0}", hata), parent=self.root)
            return
        self.ayar.tuslar[ad] = ""
        self.ayar.kaydet()
        self.listeyi_yenile(sec=ad)
        self.durum(L("'{0}' oluşturuldu.", ad))
        if messagebox.askyesno(L("Tuş ata"), L("'{0}' için şimdi tuş atansın mı?", ad), parent=self.root):
            self.tus_ata()

    def yeniden_adlandir(self):
        eski = self.secili()
        if not eski:
            return
        yeni = self.metin_sor(L("Adını Değiştir"), L("Yeni ad:"), eski)
        yeni = self.ad_dogrula(yeni, eski=eski)
        if not yeni or yeni == eski:
            return
        try:
            os.rename(os.path.join(ANA_KLASOR, eski), os.path.join(ANA_KLASOR, yeni))
        except OSError as hata:
            messagebox.showerror(L("Hata"), L("Ad değiştirilemedi:\n{0}", hata), parent=self.root)
            return
        self.deck.klasor_adi_degisti(eski, yeni)
        yeni_sozluk = collections.OrderedDict()
        for ad, tus in self.ayar.tuslar.items():
            yeni_sozluk[yeni if ad == eski else ad] = tus
        self.ayar.tuslar = yeni_sozluk
        self.ayar.kaydet()
        self.listeyi_yenile(sec=yeni)
        self.tuslari_kaydet()

    def klasor_sil(self):
        ad = self.secili()
        if not ad:
            return
        gorseller, sesler = klasor_dosyalari(ad)
        if not messagebox.askyesno(
                L("Klasörü sil"),
                L("'{0}' klasörü ve içindeki TÜM dosyalar ({1} görsel, {2} ses) kalıcı olarak silinecek.\n\n"
                  "Emin misin?", ad, len(gorseller), len(sesler)), icon="warning", parent=self.root):
            return
        try:
            shutil.rmtree(os.path.join(ANA_KLASOR, ad))
        except OSError as hata:
            messagebox.showerror(L("Hata"), L("Silinemedi:\n{0}", hata), parent=self.root)
            return
        self.ayar.tuslar.pop(ad, None)
        self.ayar.kaydet()
        self.listeyi_yenile()
        self.tuslari_kaydet()
        self.durum(L("'{0}' silindi.", ad))

    def klasor_ac(self):
        ad = self.secili()
        if not ad:
            return
        try:
            os.startfile(os.path.join(ANA_KLASOR, ad))
        except Exception as hata:
            messagebox.showerror(L("Hata"), L("Klasör açılamadı:\n{0}", hata), parent=self.root)

    def dosya_ekle(self):
        ad = self.secili()
        if not ad:
            return
        dosyalar = filedialog.askopenfilenames(
            parent=self.root, title=L("'{0}' klasörüne eklenecek dosyaları seç", ad),
            filetypes=[(L("Görsel ve ses dosyaları"), " ".join("*" + e for e in GORSEL_UZANTILARI + SES_UZANTILARI)),
                       (L("Tüm dosyalar"), "*.*")])
        sayac = 0
        for d in dosyalar:
            hedef = os.path.join(ANA_KLASOR, ad, os.path.basename(d))
            if os.path.exists(hedef):
                taban, uz = os.path.splitext(hedef)
                i = 2
                while os.path.exists(f"{taban}_{i}{uz}"):
                    i += 1
                hedef = f"{taban}_{i}{uz}"
            try:
                shutil.copy2(d, hedef)
                sayac += 1
            except OSError as hata:
                print("Kopyalanamadi:", d, hata)
        self.listeyi_yenile()
        self.durum(L("{0} dosya '{1}' klasörüne eklendi.", sayac, ad))

    def test_et(self):
        ad = self.secili()
        if ad:
            self.oynatici.tetikle(ad)

    # ---------------- tus atama ----------------
    def tus_yakala(self, mesaj, sonuc_fn):
        """'Bir tusa bas' penceresi acar, basilan tus/kombinasyonu sonuc_fn'e verir."""
        self.yakalama = True
        pen = tk.Toplevel(self.root)
        pen.title(L("Tuş ata"))
        pen.configure(bg=RENK["bg"])
        pen.transient(self.root)
        pen.resizable(False, False)
        pen.attributes("-topmost", True)
        tk.Label(pen, text=mesaj + "\n\n" + L("Şimdi atamak istediğin tuşa (ya da CTRL+G gibi bir kombinasyona) bas.\n"
                 "ESC = iptal"), justify="center", bg=RENK["bg"], fg=RENK["yazi"], font=(FONT, 10), padx=28,
                 pady=24).pack()
        durum = {"bitti": False}
        sonuc_q = queue.Queue()

        def oku():
            try:
                sonuc_q.put(keyboard.read_hotkey(suppress=False))
            except Exception:
                sonuc_q.put(None)

        def bitir(tus):
            if durum["bitti"]:
                return
            durum["bitti"] = True
            self.yakalama = False
            try:
                pen.destroy()
            except tk.TclError:
                pass
            if tus and tus != "esc":
                sonuc_fn(tus)

        def bekle():
            if durum["bitti"]:
                return
            try:
                bitir(sonuc_q.get_nowait())
            except queue.Empty:
                self.root.after(50, bekle)

        Dugme(pen, L("İptal"), lambda: bitir(None), "normal", genislik=130, arka=RENK["bg"]).pack(pady=(0, 20))
        pen.protocol("WM_DELETE_WINDOW", lambda: bitir(None))
        pen.update_idletasks()
        pen.geometry("+%d+%d" % (self.root.winfo_rootx() + 150, self.root.winfo_rooty() + 120))
        koyu_baslik(pen)
        threading.Thread(target=oku, daemon=True).start()
        bekle()

    def tus_cakisma(self, tus, haric):
        """Tus baska bir yerde kullaniliyorsa kullanan yerin adini dondurur."""
        tus = tus.lower()
        kullanilanlar = {("klasor", ad): tuslu for ad, tuslu in self.ayar.tuslar.items()}
        kullanilanlar[("ozel", "arayuz_tusu")] = self.ayar.genel["arayuz_tusu"]
        kullanilanlar[("ozel", "cikis_tusu")] = self.ayar.genel["cikis_tusu"]
        kullanilanlar[("ozel", "kapat_tusu")] = self.ayar.genel["kapat_tusu"]
        for anahtar, tuslu in kullanilanlar.items():
            if anahtar != haric and tuslu and tuslu.lower() == tus:
                return L("'{0}' klasörü", anahtar[1]) if anahtar[0] == "klasor" else L("özel tuşlar")
        return None

    def tus_ata(self):
        ad = self.secili()
        if not ad:
            return

        def sonuc(tus):
            kullanan = self.tus_cakisma(tus, ("klasor", ad))
            if kullanan:
                messagebox.showwarning(L("Tuş kullanımda"),
                                       L("'{0}' zaten {1} için atanmış.", tus.upper(), kullanan), parent=self.root)
                return
            self.ayar.tuslar[ad] = tus
            self.ayar.kaydet()
            self.listeyi_yenile(sec=ad)
            self.tuslari_kaydet()
            self.durum(f"'{ad}' → {tus.upper()}")

        self.tus_yakala(L("'{0}' klasörü için tuş seç", ad), sonuc)

    def tus_kaldir(self):
        ad = self.secili()
        if not ad:
            return
        self.ayar.tuslar[ad] = ""
        self.ayar.kaydet()
        self.listeyi_yenile(sec=ad)
        self.tuslari_kaydet()

    def ozel_tus_ata(self, anahtar):
        def sonuc(tus):
            kullanan = self.tus_cakisma(tus, ("ozel", anahtar))
            if kullanan:
                messagebox.showwarning(L("Tuş kullanımda"),
                                       L("'{0}' zaten {1} için atanmış.", tus.upper(), kullanan), parent=self.root)
                return
            self.ayar.genel[anahtar] = tus
            self.ayar.kaydet()
            self.ozel_tus_yazilarini_guncelle()
            self.tuslari_kaydet()
        self.tus_yakala(L("Yeni tuşu seç"), sonuc)

    def ozel_tus_yazilarini_guncelle(self):
        deg = "  " + L("(değiştir)")
        self.btn_arayuz.configure(text=self.ayar.genel["arayuz_tusu"].upper() + deg)
        self.btn_cikis.configure(text=self.ayar.genel["cikis_tusu"].upper() + deg)
        self.btn_kapat.configure(text=self.ayar.genel["kapat_tusu"].upper() + deg)

    # ---------------- global hotkey ----------------
    def tuslari_kaydet(self):
        """Tum hotkey'leri silip ayarlara gore yeniden kaydeder."""
        for h in self.kayitli:
            try:
                keyboard.remove_hotkey(h)
            except Exception:
                pass
        self.kayitli = []
        hatalar = []

        def ekle(tus, komut):
            if not tus:
                return
            try:
                self.kayitli.append(keyboard.add_hotkey(tus, lambda: self.kuyruk.put(komut)))
            except Exception:
                hatalar.append(tus)

        for ad, tus in self.ayar.tuslar.items():
            ekle(tus, ("tetik", ad))               # suppress yok: tus oyuna da gider
        ekle(self.ayar.genel["arayuz_tusu"], ("arayuz", None))
        ekle(self.ayar.genel["cikis_tusu"], ("cik", None))
        ekle(self.ayar.genel["kapat_tusu"], ("durdur", None))
        if hatalar:
            self.durum(L("Bu tuşlar kaydedilemedi: {0}", ", ".join(hatalar)))

    def kontrol(self):
        """Kuyrugu kisa dilimlerle (~25 ms) bosaltir: kare hazirlama sirasinda arayuz/tuslar takilmaz."""
        t0 = time.perf_counter()
        try:
            while time.perf_counter() - t0 < 0.025:
                komut = self.kuyruk.get_nowait()
                self.komut_isle(komut)
                if self.kapaniyor:
                    return
        except queue.Empty:
            pass
        self.root.after(5 if not self.kuyruk.empty() else 30, self.kontrol)

    def komut_isle(self, komut):
        tur = komut[0]
        if tur == "tetik":
            if not self.yakalama and self.ayar.genel["dinleme"]:
                self.oynatici.tetikle(komut[1])
        elif tur == "web_tetik":
            if not self.yakalama:
                self.durum(L("Telefondan tetiklendi: {0}", komut[1]))
                self.oynatici.tetikle(komut[1], komut[2], komut[3])
        elif tur == "durdur":                            # Erken kapatma (tus ya da telefon)
            if not self.yakalama:
                self.oynatici.durdur()
                SES.durdur(SES.sb)
                self.durum(L("Durduruldu."))
        elif tur == "yuklendi":                          # Telefondan/tarayicidan dosya geldi
            self.durum(L("Yüklendi: {0} → {1}", komut[2], komut[1]))
            self.listeyi_yenile()
            self.onyukle_baslat()
        elif tur == "web_ses":
            if not self.yakalama:
                SES.soundboard(komut[2], self.ayar.genel["ses_seviyesi"])
                self.deck.son_ekle(f"{komut[1]}/{os.path.basename(komut[2])}")
                self.durum(L("Soundboard: {0}", os.path.basename(komut[2])))
        elif tur == "arayuz":
            if self.root.state() == "withdrawn":
                self.goster()
            else:
                self.gizle()
        elif tur == "cik":
            self.tamamen_kapat()
        elif tur == "kare":
            self.oynatici.kare_geldi(komut)
        elif tur == "is_bitti":
            self.oynatici.is_bitti(komut)

    # ---------------- pencere yonetimi ----------------
    def goster(self):
        self.listeyi_yenile()
        self.root.deiconify()
        self.root.lift()
        self.root.focus_force()

    def gizle(self):
        self.root.withdraw()

    def pencere_kapatilirken(self):
        cevap = messagebox.askyesnocancel(
            L("Kapat"),
            L("Arka planda çalışmaya devam etsin mi?\n\n"
              "Evet: Arayüz gizlenir, tuşlar çalışmaya devam eder.\n"
              "(Geri açmak için: {0})\n"
              "Hayır: Program tamamen kapanır.", self.ayar.genel["arayuz_tusu"].upper()), parent=self.root)
        if cevap is True:
            self.gizle()
        elif cevap is False:
            self.tamamen_kapat()

    def tamamen_kapat(self):
        if self.kapaniyor:
            return
        self.kapaniyor = True
        try:
            if self.kayit_zamanlayici:
                self.ayarlari_kaydet()
            keyboard.unhook_all()
        except Exception:
            pass
        self.web.durdur()
        SES.hepsini_kapat()
        try:
            self.root.destroy()
        except tk.TclError:
            pass

    def calistir(self):
        self.root.mainloop()


_TEK_KOPYA = None


def tek_kopya_mi():
    """Program iki kez acilirsa (port cakismasi olmasin) ikincisini kapatir."""
    global _TEK_KOPYA
    try:
        k32 = ctypes.WinDLL("kernel32", use_last_error=True)
        k32.CreateMutexW.restype = ctypes.c_void_p
        _TEK_KOPYA = k32.CreateMutexW(None, False, "Local\\PopDeckTekKopya")
        if ctypes.get_last_error() == 183:               # ERROR_ALREADY_EXISTS
            ctypes.windll.user32.MessageBoxW(
                0, L("PopDeck zaten çalışıyor.\n\nArayüz gizliyse CTRL+SHIFT+F11 (ya da ayarladığın tuş) ile açabilirsin."),
                "PopDeck", 0x40)
            return False
    except Exception:
        pass
    return True


if __name__ == "__main__":
    dil_yukle_erken()
    if tek_kopya_mi():
        Uygulama().calistir()
