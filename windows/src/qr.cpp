#include "qr.hpp"
#include <cstring>

namespace {
const int ECC_L[11] = {0, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18};
const int ECC_M[11] = {0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26};
const int BLK_L[11] = {0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4};
const int BLK_M[11] = {0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5};

int raw_modules(int v) {
    int r = (16 * v + 128) * v + 64;
    if (v >= 2) { int na = v / 7 + 2; r -= (25 * na - 10) * na - 55; if (v >= 7) r -= 36; }
    return r;
}
u8 gf_mul(int x, int y) {
    int z = 0;
    for (int i = 7; i >= 0; i--) { z = (z << 1) ^ ((z >> 7) * 0x11D); z ^= ((y >> i) & 1) * x; }
    return (u8)z;
}
std::vector<u8> rs_div(int d) {
    std::vector<u8> r((size_t)d, 0);
    r[(size_t)d - 1] = 1;
    int root = 1;
    for (int i = 0; i < d; i++) {
        for (int j = 0; j < d; j++) { r[(size_t)j] = gf_mul(r[(size_t)j], root); if (j + 1 < d) r[(size_t)j] ^= r[(size_t)j + 1]; }
        root = gf_mul(root, 2);
    }
    return r;
}
}

std::vector<u8> qr_rs_remainder(const std::vector<u8>& data, int d) {
    std::vector<u8> div = rs_div(d), r((size_t)d, 0);
    for (u8 b : data) {
        u8 f = b ^ r[0];
        r.erase(r.begin()); r.push_back(0);
        for (int i = 0; i < d; i++) r[(size_t)i] ^= gf_mul(div[(size_t)i], f);
    }
    return r;
}

namespace {
struct Qr {
    int ver, size;
    std::vector<std::vector<bool>> mod, fn;
    explicit Qr(int v) : ver(v), size(v * 4 + 17), mod((size_t)size, std::vector<bool>((size_t)size, false)), fn((size_t)size, std::vector<bool>((size_t)size, false)) {}
    void setf(int x, int y, bool d) { mod[(size_t)y][(size_t)x] = d; fn[(size_t)y][(size_t)x] = true; }
    static bool bit(unsigned v, int i) { return ((v >> i) & 1) != 0; }
    void finder(int cx, int cy) {
        for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
            int d = std::max(std::abs(dx), std::abs(dy)), x = cx + dx, y = cy + dy;
            if (x >= 0 && x < size && y >= 0 && y < size) setf(x, y, d != 2 && d != 4);
        }
    }
    void align(int cx, int cy) {
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) setf(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
    }
    std::vector<int> align_pos() const {
        if (ver == 1) return {};
        int na = ver / 7 + 2;
        int step = (ver == 32) ? 26 : (ver * 4 + na * 2 + 1) / (na * 2 - 2) * 2;
        std::vector<int> r;
        for (int i = 0, pos = size - 7; i < na - 1; i++, pos -= step) r.insert(r.begin(), pos);
        r.insert(r.begin(), 6);
        return r;
    }
    void format_bits(bool medium, int mask) {
        int data = ((medium ? 0 : 1) << 3) | mask;
        int rem = data;
        for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
        unsigned bits = (unsigned)(((data << 10) | rem) ^ 0x5412);
        for (int i = 0; i <= 5; i++) setf(8, i, bit(bits, i));
        setf(8, 7, bit(bits, 6)); setf(8, 8, bit(bits, 7)); setf(7, 8, bit(bits, 8));
        for (int i = 9; i < 15; i++) setf(14 - i, 8, bit(bits, i));
        for (int i = 0; i < 8; i++) setf(size - 1 - i, 8, bit(bits, i));
        for (int i = 8; i < 15; i++) setf(8, size - 15 + i, bit(bits, i));
        setf(8, size - 8, true);
    }
    void functions() {
        for (int i = 0; i < size; i++) { setf(6, i, i % 2 == 0); setf(i, 6, i % 2 == 0); }
        finder(3, 3); finder(size - 4, 3); finder(3, size - 4);
        auto ap = align_pos();
        int n = (int)ap.size();
        for (int i = 0; i < n; i++) for (int j = 0; j < n; j++)
            if (!((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0))) align(ap[(size_t)i], ap[(size_t)j]);
        format_bits(false, 0);
        if (ver >= 7) {
            int rem = ver;
            for (int i = 0; i < 12; i++) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
            unsigned bits = ((unsigned)ver << 12) | (unsigned)rem;
            for (int i = 0; i < 18; i++) {
                bool b = bit(bits, i); int a = size - 11 + i % 3, c = i / 3;
                setf(a, c, b); setf(c, a, b);
            }
        }
    }
    void place(const std::vector<u8>& data) {
        size_t i = 0;
        for (int right = size - 1; right >= 1; right -= 2) {
            if (right == 6) right = 5;
            for (int vert = 0; vert < size; vert++) for (int j = 0; j < 2; j++) {
                int x = right - j;
                bool up = ((right + 1) & 2) == 0;
                int y = up ? size - 1 - vert : vert;
                if (!fn[(size_t)y][(size_t)x] && i < data.size() * 8) { mod[(size_t)y][(size_t)x] = bit(data[i >> 3], 7 - (int)(i & 7)); i++; }
            }
        }
    }
    void apply_mask(int m) {
        for (int y = 0; y < size; y++) for (int x = 0; x < size; x++) {
            bool inv;
            switch (m) {
                case 0: inv = (x + y) % 2 == 0; break;
                case 1: inv = y % 2 == 0; break;
                case 2: inv = x % 3 == 0; break;
                case 3: inv = (x + y) % 3 == 0; break;
                case 4: inv = (x / 3 + y / 2) % 2 == 0; break;
                case 5: inv = x * y % 2 + x * y % 3 == 0; break;
                case 6: inv = (x * y % 2 + x * y % 3) % 2 == 0; break;
                default: inv = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
            }
            if (!fn[(size_t)y][(size_t)x] && inv) mod[(size_t)y][(size_t)x] = !mod[(size_t)y][(size_t)x];
        }
    }
    long penalty() const {                                  // basitlestirilmis: ayni renk seritleri + 2x2 bloklar + koyu orani
        long p = 0;
        for (int pass = 0; pass < 2; pass++) for (int a = 0; a < size; a++) {
            int run = 1;
            for (int b = 1; b < size; b++) {
                bool c = pass ? mod[(size_t)b][(size_t)a] : mod[(size_t)a][(size_t)b];
                bool pcol = pass ? mod[(size_t)b - 1][(size_t)a] : mod[(size_t)a][(size_t)b - 1];
                if (c == pcol) { run++; if (run == 5) p += 3; else if (run > 5) p++; } else run = 1;
            }
        }
        for (int y = 0; y + 1 < size; y++) for (int x = 0; x + 1 < size; x++) {
            bool c = mod[(size_t)y][(size_t)x];
            if (c == mod[(size_t)y][(size_t)x + 1] && c == mod[(size_t)y + 1][(size_t)x] && c == mod[(size_t)y + 1][(size_t)x + 1]) p += 3;
        }
        long dark = 0;
        for (auto& r : mod) for (bool b : r) dark += b;
        long tot = (long)size * size;
        long k = (std::labs(dark * 20 - tot * 10) + tot - 1) / tot - 1;
        return p + (k > 0 ? k : 0) * 10;
    }
};
}

bool qr_encode(const std::string& text, bool medium, QrCode& out) {
    const int* ECC = medium ? ECC_M : ECC_L;
    const int* BLK = medium ? BLK_M : BLK_L;
    int ver = 0;
    for (int v = 1; v <= 10; v++) {
        int cw = raw_modules(v) / 8 - ECC[v] * BLK[v];
        int bits = 4 + (v >= 10 ? 16 : 8) + 8 * (int)text.size();
        if (bits <= cw * 8) { ver = v; break; }
    }
    if (!ver) return false;
    const int data_cw = raw_modules(ver) / 8 - ECC[ver] * BLK[ver];
    std::vector<bool> bb;
    auto put = [&](unsigned val, int len) { for (int i = len - 1; i >= 0; i--) bb.push_back(((val >> i) & 1) != 0); };
    put(4, 4); put((unsigned)text.size(), ver >= 10 ? 16 : 8);
    for (unsigned char c : text) put(c, 8);
    size_t cap = (size_t)data_cw * 8;
    put(0, (int)std::min<size_t>(4, cap - bb.size()));
    while (bb.size() % 8) bb.push_back(false);
    std::vector<u8> data;
    for (size_t i = 0; i < bb.size(); i += 8) { u8 b = 0; for (int k = 0; k < 8; k++) b = (u8)((b << 1) | (bb[i + (size_t)k] ? 1 : 0)); data.push_back(b); }
    for (u8 pad = 0xEC; data.size() < (size_t)data_cw; pad ^= 0xEC ^ 0x11) data.push_back(pad);

    const int nb = BLK[ver], ecl = ECC[ver], raw_cw = raw_modules(ver) / 8;
    const int nshort = nb - raw_cw % nb, short_len = raw_cw / nb;
    std::vector<std::vector<u8>> blocks;
    for (int i = 0, k = 0; i < nb; i++) {
        int dl = short_len - ecl + (i < nshort ? 0 : 1);
        std::vector<u8> dat(data.begin() + k, data.begin() + k + dl);
        k += dl;
        std::vector<u8> ecc = qr_rs_remainder(dat, ecl);
        if (i < nshort) dat.push_back(0);
        dat.insert(dat.end(), ecc.begin(), ecc.end());
        blocks.push_back(dat);
    }
    std::vector<u8> all;
    for (size_t i = 0; i < blocks[0].size(); i++)
        for (size_t j = 0; j < blocks.size(); j++)
            if (i != (size_t)(short_len - ecl) || j >= (size_t)nshort) all.push_back(blocks[j][i]);

    Qr q(ver);
    q.functions();
    q.place(all);
    int best = 0; long best_p = -1;
    for (int m = 0; m < 8; m++) {
        q.apply_mask(m);
        q.format_bits(medium, m);
        long p = q.penalty();
        if (best_p < 0 || p < best_p) { best_p = p; best = m; }
        q.apply_mask(m);                                   // geri al
    }
    q.apply_mask(best);
    q.format_bits(medium, best);
    out.size = q.size;
    out.m = q.mod;
    return true;
}
