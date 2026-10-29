// Keir's Chain — signal processing, framework-free so it can be tested on its own.
// Port of the browser prototype (Desk to 2000XL). Every constant comes from the schematics noted beside it.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace kc {

constexpr double PI = 3.14159265358979323846;
inline double jsRound(double x) { return std::floor(x + 0.5); }     // JavaScript Math.round
inline double clip1(double x) { return x > 1 ? 1 : (x < -1 ? -1 : x); }
inline double clipF(double x) { const double a = std::abs(x); return a <= 1 ? 0.5 * x * x : a - 0.5; }
/* hard clip with first-order antiderivative anti-aliasing, only while clipping */
inline double adaaClip(double u, double& prev) {
    double c = u;
    if (std::abs(u) > 1 || std::abs(prev) > 1) {
        const double d = u - prev;
        c = std::abs(d) < 1e-6 ? clip1(0.5 * (u + prev)) : (clipF(u) - clipF(prev)) / d;
    }
    prev = u;
    return c;
}

/* ---------------- constants ---------------- */
namespace XL {   // MPC2000XL analog board (L6044801) + owner's manual specs; W 492 high shelf
constexpr double ADC_FS_DBU = 0.5, DRIVER = 0.22, C_GAIN = 110e-6, GBW = 10e6, C_FB_POLE = 241e3, EN = 7e-9, SRC_DBU = 10;
constexpr double IV_R = 4990, IV_C = 2200e-12, SK_R1 = 1910, SK_R2 = 5760, SK_C1 = 10e-9, SK_C2 = 470e-12;
constexpr double EQ_C = 28.8e-9, EQ_R46 = 3300, EQ_R47 = 100e3, EQ_R44 = 620, EQ_R45 = 620, EQ_PT4 = 100;
constexpr double RAIL = 3.75;
}
namespace MK {   // Mackie CR1604-VLZ line input, schematics 990-219 / 990-220
constexpr double PAD = 1500.0 / (5620.0 + 1500.0), RFB = 10220, RMIN = 3.32, TRIM = 10000, C1 = 1000e-6, DIFF = 5110.0 / 10000.0;
constexpr double VCLIP = 14.5, EN = 6.4e-9, EN_OUT = 20e-9;
const double VPK0 = 0.775 * std::sqrt(2.0) * std::pow(10.0, 10.0 / 20.0);
}
constexpr int OS = 4, OST = 289;          // Mackie oversampling filter: 4x, linear phase, 72-sample delay

/* ---------------- small helpers ---------------- */
struct Rng {   // xorshift32, identical sequence to the prototype
    uint32_t s;
    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 1) {}
    double next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s / 4294967296.0 - 0.5; }
};

inline void fftInPlace(std::vector<std::complex<double>>& a, bool inverse) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = (inverse ? 2 : -2) * PI / (double)len;
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1, 0);
            for (size_t k = 0; k < len / 2; k++) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
    if (inverse) for (auto& x : a) x /= (double)n;
}

/* FIR whose response matches a continuous-time response up to Nyquist, with a fixed delay;
   only the tail after the delay is tapered. */
template <typename Resp>
std::vector<double> firFromResponse(double fs, Resp resp, int taps, int delay, bool normaliseDC) {
    const int M = 1024;
    std::vector<std::complex<double>> X(M);
    for (int k = 0; k <= M / 2; k++) {
        const std::complex<double> r = resp(k * fs / M);
        const double ph = -2 * PI * k * delay / M;
        X[k] = r * std::complex<double>(std::cos(ph), std::sin(ph));
        if (k > 0 && k < M / 2) X[M - k] = std::conj(X[k]);
    }
    X[M / 2] = std::complex<double>(X[M / 2].real(), 0);
    fftInPlace(X, true);
    std::vector<double> h(taps);
    for (int n = 0; n < taps; n++) {
        const double w = n < delay ? 1 : 0.5 + 0.5 * std::cos(PI * (n - delay) / (double)(taps - delay));
        h[n] = X[n].real() * w;
    }
    if (normaliseDC) { double dc = 0; for (double v : h) dc += v; for (double& v : h) v /= dc; }
    return h;
}

inline double besselI0(double x) { double s = 1, t = 1; for (int k = 1; k < 30; k++) { t *= (x / 2 / k) * (x / 2 / k); s += t; } return s; }
inline std::vector<double> kaiserLP(int taps, double fc, double fs, double beta) {
    std::vector<double> h(taps); const double M = (taps - 1) / 2.0; double sum = 0;
    for (int n = 0; n < taps; n++) {
        const double m = n - M, x = 2 * fc / fs;
        const double sinc = m == 0 ? x : std::sin(PI * x * m) / (PI * m);
        h[n] = sinc * besselI0(beta * std::sqrt(1 - (m / M) * (m / M))) / besselI0(beta); sum += h[n];
    }
    for (double& v : h) v /= sum;
    return h;
}

/* 2000XL D/A: PCM69 sample hold, IC800 I/V pole, IC900 Sallen-Key */
inline std::complex<double> xlDAResponse(double f, double fs) {
    using namespace XL;
    const double w = 2 * PI * f, x = f / fs, zoh = x == 0 ? 1 : std::sin(PI * x) / (PI * x);
    const std::complex<double> iv = 1.0 / std::complex<double>(1, w * IV_R * IV_C);
    const double w0 = 1 / std::sqrt(SK_R1 * SK_R2 * SK_C1 * SK_C2), Q = std::sqrt(SK_R1 * SK_R2 * SK_C1 * SK_C2) / (SK_C2 * (SK_R1 + SK_R2));
    const std::complex<double> sk = (w0 * w0) / std::complex<double>(w0 * w0 - w * w, w * w0 / Q);
    return iv * sk * zoh;
}

/* W 492 high shelf (U7/U8): H(s) = (s(1 - a k) + wc) / (s(1 - (1 - a) k) + wc) */
struct W492 { double k, a, wc; };
inline W492 w492Params(double gainDb, double freqHz) {
    using namespace XL;
    const double k = (2 * EQ_R44) / (EQ_R44 + EQ_R45 + EQ_PT4), m = 1 - k, G15 = std::pow(10.0, 15.0 / 20);
    const double u15 = std::sqrt((G15 * G15 - 1) / (1 - G15 * G15 * m * m));
    const double wc = 2 * PI * freqHz / u15, target = std::pow(10.0, std::abs(gainDb) / 20), w = 2 * PI * freqHz;
    auto gainAt = [&](double a) { return std::hypot(wc, w * (1 - a * k)) / std::hypot(wc, w * (1 - (1 - a) * k)); };
    double lo = 0, hi = 0.5;
    for (int i = 0; i < 60; i++) { const double mid = (lo + hi) / 2; if (gainAt(mid) > target) lo = mid; else hi = mid; }
    const double aBoost = (lo + hi) / 2;
    return { k, gainDb >= 0 ? aBoost : 1 - aBoost, wc };
}
inline std::complex<double> w492Response(double f, const W492& P) {
    const double w = 2 * PI * f;
    return std::complex<double>(P.wc, w * (1 - P.a * P.k)) / std::complex<double>(P.wc, w * (1 - (1 - P.a) * P.k));
}

/* direct-form FIR on a doubled ring buffer */
struct Fir {
    std::vector<double> h, b; int T = 0, i = 0;
    void setTaps(std::vector<double> taps) { h = std::move(taps); T = (int)h.size(); b.assign(T * 2, 0.0); i = 0; }
    inline void push(double x) { i = (i + T - 1) % T; b[i] = x; b[i + T] = x; }
    inline double dot(const std::vector<double>& k) const { double acc = 0; const double* p = &b[i]; for (int n = 0; n < T; n++) acc += k[n] * p[n]; return acc; }
    inline double run(double x) { push(x); return dot(h); }
};

/* ---------------- MPC2000XL Timing Correct on audio ---------------- */
enum class Grid { E8 = 0, E8T, S16, S16T, T32, T32T };
inline int gridTicks(Grid g) { static const int t[] = { 48, 32, 24, 16, 12, 8 }; return t[(int)g]; }

class MPCTiming {
public:
    struct Params { bool on = true; double bpm = 124; Grid grid = Grid::S16; double swing = 50; bool earlier = false; double shiftAmt = 0; };
    void prepare(double sampleRate) {
        fs = sampleRate; bufL.assign(SIZE, 0.f); bufR.assign(SIZE, 0.f); bufP.assign(SIZE, 0.0); w = 0;
        fade = jsRound(0.004 * fs); set(p);
    }
    void set(const Params& q) {
        p = q; const int G = gridTicks(p.grid);
        spt = fs * 60.0 / (std::max(20.0, p.bpm) * 96.0);
        swingOn = (p.grid == Grid::E8 || p.grid == Grid::S16) && p.swing > 50;
        const double sw = std::clamp(p.swing, 50.0, 75.0);
        evenTicks = swingOn ? (int)jsRound(2 * G * sw / 100.0) - G : 0;
        const int amt = std::clamp((int)jsRound(p.shiftAmt), 0, G / 2);
        shiftTicks = p.earlier ? -amt : amt; lookTicks = p.earlier ? amt : 0;
        eOdd = (shiftTicks + lookTicks) * spt; eEven = (shiftTicks + lookTicks + evenTicks) * spt; gS = G * spt;
    }
    int latencySamples() const { return (int)jsRound(lookTicks * spt); }
    int evenTicksNow() const { return evenTicks; }
    /* pos0: transport position in samples (from the host's beat position) of the first sample */
    void process(const float* inL, const float* inR, float* outL, float* outR, int n, double pos0) {
        const int64_t m = SIZE - 1;
        for (int i = 0; i < n; i++) {
            const double pos = pos0 + i;
            const int64_t wi = w; bufL[wi & m] = inL[i]; bufR[wi & m] = inR[i]; bufP[wi & m] = pos; w = wi + 1;
            if (!p.on) { outL[i] = inL[i]; outR[i] = inR[i]; continue; }
            double l = 0, r = 0;
            const double aO = (double)wi - eOdd;
            if (aO >= 0) { const double wo = 1 - evenWeight(bufP[(int64_t)jsRound(aO) & m]); if (wo > 0) { l += wo * read(bufL, aO); r += wo * read(bufR, aO); } }
            if (swingOn) {
                const double aE = (double)wi - eEven;
                if (aE >= 0) { const double we = evenWeight(bufP[(int64_t)jsRound(aE) & m]); if (we > 0) { l += we * read(bufL, aE); r += we * read(bufR, aE); } }
            }
            outL[i] = (float)l; outR[i] = (float)r;
        }
    }
private:
    static constexpr int64_t SIZE = 1 << 17;
    double evenWeight(double pos) const {
        if (!swingOn) return 0;
        const double g = gS, F = fade, r = std::fmod(std::fmod(pos, 2 * g) + 2 * g, 2 * g);
        if (r < g - F) return 0;
        if (r < g) return (r - (g - F)) / F;
        if (r < 2 * g - F) return 1;
        return 1 - (r - (2 * g - F)) / F;
    }
    double read(const std::vector<float>& buf, double at) const {   // 4-point Hermite
        const int64_t i = (int64_t)std::floor(at), m = SIZE - 1; const double f = at - (double)i;
        const double y0 = buf[(i - 1) & m], y1 = buf[i & m], y2 = buf[(i + 1) & m], y3 = buf[(i + 2) & m];
        const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    Params p; double fs = 44100, spt = 1, eOdd = 0, eEven = 0, gS = 1, fade = 176; bool swingOn = false;
    int evenTicks = 0, shiftTicks = 0, lookTicks = 0; int64_t w = 0;
    std::vector<float> bufL, bufR; std::vector<double> bufP;
};

/* ---------------- the chain: Mackie → (W492) → 2000XL A/D → D/A → (W492) ---------------- */
class Chain {
public:
    struct Params {
        MPCTiming::Params timing;
        bool mkOn = true; double drive = 0;                     // Mackie trim, dB from U
        bool eqOn = true; double bright = 0, brightFreq = 10000; bool eqPost = false;
        bool xlOn = true; bool truncate = true;
        double outDb = 0; bool bypass = false;
    };
    struct Meters { float xlL = 0, xlR = 0, mkClip = 0, out = 0; };

    void prepare(double sampleRate, int maxBlock) {
        fs = sampleRate;
        timing.prepare(fs);
        const auto hAK = kaiserLP(95, 21000, fs, 7);
        const auto hDA = firFromResponse(fs, [&](double f) { return xlDAResponse(f, fs); }, 192, 24, true);
        const auto hOS = kaiserLP(OST, std::min(21300.0, 0.483 * fs), fs * OS, 8);
        const uint32_t seeds[2] = { 2654435761u, (uint32_t)(2ull * 2654435761ull) };
        for (int c = 0; c < 2; c++) {
            auto& L = xl[c]; L = XLLane(); L.rng = Rng(seeds[c]);
            L.ak.setTaps(hAK); L.daf.setTaps(hDA); L.eqf.setTaps(std::vector<double>(EQ_TAPS, 0.0)); L.dly.assign(D, 0.0);
            auto& M = mk[c]; M = MKLane(); M.rng = Rng(c == 0 ? 12345u : 67890u);
            M.h = hOS; M.inb.assign(128, 0.0); M.ob.assign(OST * 2, 0.0); M.dly.assign(MKD, 0.0);
        }
        kBx = 1 - std::exp(-1 / (0.02 * fs));
        tL.assign(std::max(1, maxBlock), 0.f); tR = tL;
        dryL.assign(DRY_SIZE, 0.f); dryR = dryL; dw = 0;
        eqKey = -1e9; eqFadeLeft = 0; mkSmoothInit = false;
        set(p, true);
    }
    void set(const Params& q, bool force = false) {
        p = q;
        timing.set(p.timing);
        // 2000XL REC GAIN fixed so the DAW's 0 dBFS meets converter full scale (+10 dBu at RECORD IN)
        const double sens = 10.0;
        G = std::pow(10.0, ((XL::ADC_FS_DBU - sens) - 20 * std::log10(XL::DRIVER)) / 20);
        const double Rg = G > 1.0001 ? 60000 / (G - 1) : 1e12, hpFc = 1 / (2 * PI * Rg * XL::C_GAIN);
        hpA = std::exp(-2 * PI * hpFc / fs);
        const double bwFc = std::min(XL::GBW / G, XL::C_FB_POLE);
        bwOn = bwFc < 60000; bwA = 1 - std::exp(-2 * PI * bwFc / fs);
        k = std::pow(10.0, (XL::SRC_DBU - sens) / 20);
        nSig = XL::EN * std::sqrt(fs / 2) / (0.775 * std::pow(10.0, sens / 20) * std::sqrt(2.0));
        outG = p.xlOn ? 1 / k : 1;
        masterG = std::pow(10.0, p.outDb / 20);
        // Mackie trim → gain-set resistance
        const double G1min = 1 + MK::RFB / (MK::TRIM + MK::RMIN), G1max = 1 + MK::RFB / MK::RMIN;
        mkG1Target = std::clamp(std::pow(10.0, p.drive / 20) / (MK::PAD * MK::DIFF), G1min, G1max);
        if (!mkSmoothInit || force) { mkG1 = mkG1Target; mkSmoothInit = true; updateMackieDerived(); }
        mkK = MK::VPK0 * MK::PAD / MK::VCLIP;
        // W 492: redesign only when the setting really moves, then crossfade old → new taps
        const double eqB = p.eqOn ? p.bright : 0.0;
        const double key = std::round(eqB * 10) * 100000 + std::round(p.brightFreq);
        if (key != eqKey) {
            const W492 P = w492Params(eqB, p.brightFreq);
            auto h = firFromResponse(fs, [&](double f) { return w492Response(f, P); }, EQ_TAPS, EQD, false);
            if (eqKey < -1e8 || force) { eqH = h; eqFadeLeft = 0; }
            else { eqHOld = eqH; eqH = h; eqFadeLeft = EQ_FADE; }
            eqKey = key;
        }
    }
    int latency() const { return timing.latencySamples() + MKD + EQD + (p.xlOn ? AKD : 0) + D; }

    /* in/out may alias. pos0 = host transport position (samples) of the first sample. */
    void process(const float* inL, const float* inR, float* outL, float* outR, int n, double pos0) {
        if ((int)tL.size() < n) { tL.assign(n, 0.f); tR.assign(n, 0.f); }
        const int lat = latency();
        // dry copy, delayed to line up with the processed signal for bypass
        for (int i = 0; i < n; i++) { dryL[(dw + i) & (DRY_SIZE - 1)] = inL[i]; dryR[(dw + i) & (DRY_SIZE - 1)] = inR[i]; }
        timing.process(inL, inR, tL.data(), tR.data(), n, pos0);
        for (int i = 0; i < n; i++) {
            if ((i & 31) == 0) smoothMackie();
            const double tl = tL[i], tr = tR[i];
            double wl = lane(0, tl), wr = lane(1, tr);
            wl *= outG * masterG; wr *= outG * masterG;
            if (eqFadeLeft > 0) eqFadeLeft--;
            bx += kBx * ((p.bypass ? 1.0 : 0.0) - bx);
            const int64_t di = (dw + i - lat) & (DRY_SIZE - 1);
            const double dl = dryL[di], dr = dryR[di];
            double ol = wl * (1 - bx) + dl * bx, orr = wr * (1 - bx) + dr * bx;
            const double ap = std::max(std::abs(ol), std::abs(orr)); if (ap > opk) opk = ap;
            outL[i] = (float)clip1(ol); outR[i] = (float)clip1(orr);
        }
        dw += n;
    }
    Meters takeMeters() {
        Meters m; m.xlL = (float)xl[0].peak; m.xlR = (float)xl[1].peak; m.mkClip = (float)std::max(mk[0].clip, mk[1].clip); m.out = (float)opk;
        xl[0].peak = xl[1].peak = 0; mk[0].clip = mk[1].clip = 0; opk = 0; return m;
    }
    int evenTicks() const { return timing.evenTicksNow(); }

private:
    static constexpr int D = 24, AKD = 47, EQD = 24, MKD = 72, EQ_TAPS = 192, EQ_FADE = 1024;
    static constexpr int64_t DRY_SIZE = 1 << 15;
    struct XLLane { Fir ak, daf, eqf; double ex = 0, hpx = 0, hpy = 0, bw = 0, cx = 0, peak = 0; Rng rng; std::vector<double> dly; int di = 0; };
    struct MKLane { std::vector<double> h, inb, ob, dly; int ii = 0, oi = 0, di = 0; double hpx = 0, hpy = 0, cx = 0, clip = 0; Rng rng{ 12345 }; };

    void updateMackieDerived() {
        const double mkRg = MK::RFB / (mkG1 - 1);
        mkHpA = std::exp(-2 * PI * (1 / (2 * PI * mkRg * MK::C1)) / (fs * OS));
        mkOutG = MK::VCLIP / (MK::VPK0 * MK::PAD * mkG1 * MK::DIFF);
        mkN = std::hypot(MK::EN * mkG1 * MK::DIFF, MK::EN_OUT) * std::sqrt(fs * OS / 2) / MK::VCLIP;
    }
    void smoothMackie() {   // glide the trim in log steps so turning it never clicks
        if (mkG1 == mkG1Target) return;
        const double r = std::log(mkG1Target / mkG1), step = 0.02;   // ~0.17 dB per 32 samples
        mkG1 = std::abs(r) <= step ? mkG1Target : mkG1 * std::exp(r > 0 ? step : -step);
        updateMackieDerived();
    }
    double mackie(int c, double x) {
        MKLane& M = mk[c];
        if (!p.mkOn) { const double y = M.dly[M.di]; M.dly[M.di] = x; M.di = (M.di + 1) % MKD; return y; }
        const double G1 = mkG1, Dd = MK::DIFF; double out = 0;
        M.ii = (M.ii + 127) % 128; M.inb[M.ii] = x;
        for (int ph = 0; ph < OS; ph++) {
            double u = 0;
            for (int kk = 0, j = ph; j < OST; kk++, j += OS) u += M.h[j] * M.inb[(M.ii + kk) % 128];
            u *= OS * mkK;
            M.hpy = mkHpA * (M.hpy + u - M.hpx); M.hpx = u;               // C1 1000 µF in the gain leg
            double v = (u + (G1 - 1) * M.hpy) * Dd;                       // preamp + U33A, in units of the clip voltage
            v += (M.rng.next() + M.rng.next() + M.rng.next()) * 2 * mkN;  // channel noise
            if (std::abs(v) > M.clip) M.clip = std::abs(v);
            const double cl = adaaClip(v, M.cx);                          // NJM2068 into the ±16 V rails
            M.oi = (M.oi + OST - 1) % OST; M.ob[M.oi] = cl; M.ob[M.oi + OST] = cl;
            if (ph == 0) { double acc = 0; const double* o = &M.ob[M.oi]; for (int nn = 0; nn < OST; nn++) acc += M.h[nn] * o[nn]; out = acc; }
        }
        return out * mkOutG;
    }
    double eq(int c, double x) {
        XLLane& L = xl[c];
        L.eqf.push(x);
        double y = L.eqf.dot(eqH);
        if (eqFadeLeft > 0) { const double t = (double)eqFadeLeft / EQ_FADE; y = y * (1 - t) + L.eqf.dot(eqHOld) * t; }
        y /= XL::RAIL;
        return adaaClip(y, L.ex) * XL::RAIL;                              // NE5532 rails, only if pushed that far
    }
    double ad(int c, double x) {
        XLLane& L = xl[c];
        const double v = x * k / G;
        L.hpy = hpA * (L.hpy + v - L.hpx); L.hpx = v;
        double u = v + (G - 1) * L.hpy;                                   // instrumentation amp
        if (bwOn) { L.bw += bwA * (u - L.bw); u = L.bw; }
        u += (L.rng.next() + L.rng.next() + L.rng.next()) * 2 * nSig;    // preamp hiss
        const double a = std::abs(u); if (a > L.peak) L.peak = a;
        const double c2 = adaaClip(u, L.cx);                              // AK5340 overload
        double out = clip1(L.ak.run(c2));                                 // decimation filter
        const double q = 32768;
        out = p.truncate ? std::floor(out * q) : jsRound(out * q);        // 16-bit storage
        return std::clamp(out, -q, q - 1) / q;
    }
    double delay(XLLane& L, double x) { const double y = L.dly[L.di]; L.dly[L.di] = x; L.di = (L.di + 1) % D; return y; }
    double lane(int c, double x) {
        double y = mackie(c, x);
        if (!p.eqPost) y = eq(c, y);
        y = p.xlOn ? ad(c, y) : y;
        y = p.xlOn ? xl[c].daf.run(y) : delay(xl[c], y);
        if (p.eqPost) y = eq(c, y);
        return y;
    }

    Params p; double fs = 44100;
    MPCTiming timing; std::array<XLLane, 2> xl; std::array<MKLane, 2> mk;
    double G = 1, hpA = 0, bwA = 0, k = 1, nSig = 0, outG = 1, masterG = 1; bool bwOn = false;
    double mkG1 = 9.29, mkG1Target = 9.29, mkHpA = 0, mkK = 0, mkOutG = 1, mkN = 0; bool mkSmoothInit = false;
    std::vector<double> eqH, eqHOld; double eqKey = -1e9; int eqFadeLeft = 0;
    double bx = 0, kBx = 0, opk = 0;
    std::vector<float> tL, tR, dryL, dryR; int64_t dw = 0;
};

} // namespace kc
