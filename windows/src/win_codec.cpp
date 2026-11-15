// WIC tabanli gorsel kodegi: PNG/JPEG/BMP/TIFF/ICO/WEBP(+HEIF/AVIF eklentileri varsa) acar, PNG/JPEG yazar
#include "winhdr.hpp"
#include <wincodec.h>
#include "image.hpp"
#include <cstring>

namespace {
const GUID kClsidFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
const GUID kIidFactory   = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
const GUID kPfBgra32     = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f}};
const GUID kPfBgr24      = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0c}};
const GUID kFmtPng       = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
const GUID kFmtJpeg      = {0x19e4a5aa, 0x5662, 0x4fc5, {0xa0, 0xc0, 0x17, 0x58, 0x02, 0x8e, 0x10, 0x57}};

struct Com {                                  // her thread'de COM hazir olsun
    bool ok;
    Com() { HRESULT h = CoInitializeEx(nullptr, COINIT_MULTITHREADED); ok = (h == S_OK || h == S_FALSE); }
    ~Com() { if (ok) CoUninitialize(); }
};
template <class T> struct Rel { T* p = nullptr; ~Rel() { if (p) p->Release(); } T** operator&() { return &p; } T* operator->() { return p; } operator bool() const { return p != nullptr; } };

IWICImagingFactory* make_factory() {
    IWICImagingFactory* f = nullptr;
    if (FAILED(CoCreateInstance(kClsidFactory, nullptr, CLSCTX_INPROC_SERVER, kIidFactory, (void**)&f))) return nullptr;
    return f;
}

std::string magic_format(const std::string& path) {
    FILE* f = _wfopen(W(path).c_str(), L"rb");
    if (!f) return "";
    unsigned char b[16] = {0};
    size_t n = fread(b, 1, 16, f);
    fclose(f);
    if (n >= 6 && memcmp(b, "GIF8", 4) == 0) return "GIF";
    if (n >= 8 && b[0] == 0x89 && memcmp(b + 1, "PNG", 3) == 0) return "PNG";
    if (n >= 12 && memcmp(b, "RIFF", 4) == 0 && memcmp(b + 8, "WEBP", 4) == 0) return "WEBP";
    if (n >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF) return "JPEG";
    return "OTHER";
}

int read_orientation(IWICBitmapFrameDecode* fr) {
    Rel<IWICMetadataQueryReader> mq;
    if (FAILED(fr->GetMetadataQueryReader(&mq)) || !mq) return 1;
    static const wchar_t* names[] = {L"/app1/ifd/{ushort=274}", L"/ifd/{ushort=274}", L"System.Photo.Orientation"};
    for (auto nm : names) {
        PROPVARIANT pv; PropVariantInit(&pv);
        if (SUCCEEDED(mq->GetMetadataByName(nm, &pv))) {
            int v = (pv.vt == VT_UI2) ? pv.uiVal : (pv.vt == VT_UI1 ? pv.bVal : (pv.vt == VT_I2 ? pv.iVal : (pv.vt == VT_UI4 ? (int)pv.ulVal : 0)));
            PropVariantClear(&pv);
            if (v >= 1 && v <= 8) return v;
        }
    }
    return 1;
}

bool open_frame(IWICImagingFactory* f, const std::string& path, IWICBitmapDecoder** dec, IWICBitmapFrameDecode** fr) {
    if (FAILED(f->CreateDecoderFromFilename(W(path).c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, dec))) return false;
    if (FAILED((*dec)->GetFrame(0, fr))) return false;
    return true;
}

bool encode(const Rgba& im, bool jpeg, int quality, std::vector<u8>& out) {
    Com com;
    Rel<IWICImagingFactory> f; f.p = make_factory();
    if (!f || im.w <= 0 || im.h <= 0) return false;
    Rel<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) return false;
    Rel<IWICBitmapEncoder> enc;
    if (FAILED(f->CreateEncoder(jpeg ? kFmtJpeg : kFmtPng, nullptr, &enc))) return false;
    if (FAILED(enc->Initialize(stream.p, WICBitmapEncoderNoCache))) return false;
    Rel<IWICBitmapFrameEncode> fr;
    Rel<IPropertyBag2> bag;
    if (FAILED(enc->CreateNewFrame(&fr, &bag))) return false;
    if (jpeg && bag) {
        PROPBAG2 o; memset(&o, 0, sizeof o);
        o.pstrName = (LPOLESTR)L"ImageQuality";
        VARIANT v; VariantInit(&v); v.vt = VT_R4; v.fltVal = quality / 100.0f;
        bag->Write(1, &o, &v);
    }
    if (FAILED(fr->Initialize(bag.p))) return false;
    if (FAILED(fr->SetSize((UINT)im.w, (UINT)im.h))) return false;
    GUID pf = jpeg ? kPfBgr24 : kPfBgra32;
    GUID want = pf;
    if (FAILED(fr->SetPixelFormat(&pf))) return false;
    if (memcmp(&pf, &want, sizeof pf) != 0) return false;
    std::vector<u8> buf;
    UINT stride;
    if (jpeg) {
        stride = (UINT)im.w * 3;
        buf.resize((size_t)stride * im.h);
        for (size_t i = 0, n = (size_t)im.w * im.h; i < n; i++) { buf[i * 3] = im.px[i * 4 + 2]; buf[i * 3 + 1] = im.px[i * 4 + 1]; buf[i * 3 + 2] = im.px[i * 4]; }
    } else {
        stride = (UINT)im.w * 4;
        buf.resize((size_t)stride * im.h);
        for (size_t i = 0, n = (size_t)im.w * im.h; i < n; i++) { buf[i * 4] = im.px[i * 4 + 2]; buf[i * 4 + 1] = im.px[i * 4 + 1]; buf[i * 4 + 2] = im.px[i * 4]; buf[i * 4 + 3] = im.px[i * 4 + 3]; }
    }
    if (FAILED(fr->WritePixels((UINT)im.h, stride, (UINT)buf.size(), buf.data()))) return false;
    if (FAILED(fr->Commit()) || FAILED(enc->Commit())) return false;
    STATSTG st; memset(&st, 0, sizeof st);
    if (FAILED(stream->Stat(&st, STATFLAG_NONAME))) return false;
    HGLOBAL hg = nullptr;
    if (FAILED(GetHGlobalFromStream(stream.p, &hg)) || !hg) return false;
    void* p = GlobalLock(hg);
    if (!p) return false;
    out.assign((u8*)p, (u8*)p + st.cbSize.LowPart);
    GlobalUnlock(hg);
    return !out.empty();
}

struct WicCodec : Codec {
    bool probe(const std::string& path, CodecInfo& info) override {
        Com com;
        Rel<IWICImagingFactory> f; f.p = make_factory();
        if (!f) return false;
        Rel<IWICBitmapDecoder> dec; Rel<IWICBitmapFrameDecode> fr;
        if (!open_frame(f.p, path, &dec, &fr)) return false;
        UINT w = 0, h = 0;
        if (FAILED(fr->GetSize(&w, &h)) || !w || !h) return false;
        info.w = (int)w; info.h = (int)h;
        info.format = magic_format(path);
        info.orient = info.format == "JPEG" ? read_orientation(fr.p) : 1;
        return true;
    }
    bool decode(const std::string& path, Rgba& out, int& orient) override {
        Com com;
        Rel<IWICImagingFactory> f; f.p = make_factory();
        if (!f) return false;
        Rel<IWICBitmapDecoder> dec; Rel<IWICBitmapFrameDecode> fr;
        if (!open_frame(f.p, path, &dec, &fr)) return false;
        UINT w = 0, h = 0;
        if (FAILED(fr->GetSize(&w, &h)) || !w || !h || (u64)w * h > 120000000ull) return false;
        orient = read_orientation(fr.p);
        Rel<IWICFormatConverter> conv;
        if (FAILED(f->CreateFormatConverter(&conv))) return false;
        if (FAILED(conv->Initialize(fr.p, kPfBgra32, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return false;
        std::vector<u8> bgra((size_t)w * h * 4);
        if (FAILED(conv->CopyPixels(nullptr, w * 4, (UINT)bgra.size(), bgra.data()))) return false;
        out.w = (int)w; out.h = (int)h; out.px.resize(bgra.size());
        for (size_t i = 0, n = (size_t)w * h; i < n; i++) {
            out.px[i * 4] = bgra[i * 4 + 2]; out.px[i * 4 + 1] = bgra[i * 4 + 1]; out.px[i * 4 + 2] = bgra[i * 4]; out.px[i * 4 + 3] = bgra[i * 4 + 3];
        }
        return true;
    }
    bool encode_png(const Rgba& img, std::vector<u8>& out) override { return encode(img, false, 0, out); }
    bool encode_jpeg(const Rgba& img, int q, std::vector<u8>& out) override { return encode(img, true, q, out); }
};
}

Codec* win_codec_create() { static WicCodec c; return &c; }
