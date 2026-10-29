// Keir's Chain — live key and chord detection on the audio passing through the plugin.
// Peak-picked chroma with overtone suppression, tuning estimate, Krumhansl-Kessler key profiles, chord templates.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

namespace kc {

class KeyDetector {
public:
    struct Key { bool valid = false; int tonic = 0; bool minor = false; double r = 0, margin = 0; std::string name, camelot, altName, altCamelot; };
    struct Chord { bool valid = false; std::string name; double score = 0; };

    explicit KeyDetector(double sampleRate = 44100, int N = 16384) : sr(sampleRate), N(N) {
        win.resize(N); for (int i = 0; i < N; i++) { const double x = 2 * PI * i / (N - 1);
            win[i] = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2 * x) - 0.01168 * std::cos(3 * x); }
        buf.resize(N); mag.resize(N / 2); reset();
    }
    void reset() { keyAcc.fill(0); chordAcc.fill(0); bassAcc.fill(0); tx = ty = 0; heard = 0; key = Key(); chord = Chord(); tuning = 0; silent = true; }

    /* x: the latest N mono samples; dt: seconds since the previous call */
    void push(const float* x, double dt) {
        double e = 0;
        for (int i = 0; i < N; i++) { buf[i] = std::complex<double>(x[i] * win[i], 0); e += (double)x[i] * x[i]; }
        if (std::sqrt(e / N) < 0.0005) { silent = true; chord = Chord(); return; }   // below -66 dBFS: hold the key
        silent = false;
        fft(buf);
        for (int kk = 0; kk < N / 2; kk++) mag[kk] = std::abs(buf[kk]);
        auto pk = peaks();
        const double kT = std::max(1 - std::exp(-dt / 30), dt / (heard + dt));
        double x2 = 0, y2 = 0; for (auto& q : pk) { const double midi = 69 + 12 * std::log2(q.f / 440), ang = 2 * PI * midi, w = std::sqrt(q.a); x2 += w * std::cos(ang); y2 += w * std::sin(ang); }
        tx += (x2 - tx) * kT; ty += (y2 - ty) * kT;
        tuning = (tx == 0 && ty == 0) ? 0 : std::atan2(ty, tx) / (2 * PI);
        std::array<double, 12> c{}, bass{};
        for (auto& q : pk) {
            const double midi = 69 + 12 * std::log2(q.f / 440) - tuning, r = std::floor(midi + 0.5), dev = midi - r;
            const double w = std::pow(std::cos(PI * dev), 2);
            const int pc = (((int)r % 12) + 12) % 12; const double v = std::sqrt(q.a) * w;
            c[pc] += v; if (q.f < 250) bass[pc] += v;
        }
        double s = 0; for (double v : c) s += v;
        if (s <= 0) return;
        heard += dt;
        const double kK = std::max(1 - std::exp(-dt / 20), dt / heard), kC = 1 - std::exp(-dt / 0.3);
        for (int i = 0; i < 12; i++) {
            keyAcc[i] += (c[i] / s - keyAcc[i]) * kK;
            chordAcc[i] += (c[i] / s - chordAcc[i]) * kC;
            bassAcc[i] += (bass[i] / s - bassAcc[i]) * kC;
        }
        key = findKey(keyAcc);
        chord = findChord(chordAcc, bassAcc);
    }

    Key key; Chord chord; double tuning = 0, heard = 0; bool silent = true;

private:
    static constexpr double PI = 3.14159265358979323846;
    struct Peak { double f, a; };
    double sr; int N;
    std::vector<double> win, mag; std::vector<std::complex<double>> buf;
    std::array<double, 12> keyAcc{}, chordAcc{}, bassAcc{}; double tx = 0, ty = 0;

    static const char* majN(int i) { static const char* n[] = { "C", "D\xE2\x99\xAD", "D", "E\xE2\x99\xAD", "E", "F", "F\xE2\x99\xAF", "G", "A\xE2\x99\xAD", "A", "B\xE2\x99\xAD", "B" }; return n[i]; }
    static const char* minN(int i) { static const char* n[] = { "C", "C\xE2\x99\xAF", "D", "E\xE2\x99\xAD", "E", "F", "F\xE2\x99\xAF", "G", "G\xE2\x99\xAF", "A", "B\xE2\x99\xAD", "B" }; return n[i]; }

    static void fft(std::vector<std::complex<double>>& a) {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; i++) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
        for (size_t len = 2; len <= n; len <<= 1) {
            const double ang = -2 * PI / (double)len; const std::complex<double> wl(std::cos(ang), std::sin(ang));
            for (size_t i = 0; i < n; i += len) { std::complex<double> w(1, 0);
                for (size_t k = 0; k < len / 2; k++) { const auto u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; } }
        }
    }
    std::vector<Peak> peaks() const {
        std::vector<Peak> out;
        const int k0 = std::max(2, (int)std::floor(55.0 * N / sr)), k1 = std::min((int)mag.size() - 2, (int)std::ceil(5000.0 * N / sr));
        double mx = 0; for (int k = k0; k <= k1; k++) mx = std::max(mx, mag[k]);
        if (mx <= 0) return out;
        const double floorV = mx * 0.003;
        for (int k = k0; k <= k1; k++) {
            const double m = mag[k];
            if (m > floorV && m > mag[k - 1] && m >= mag[k + 1]) {
                const double a = std::log(mag[k - 1] + 1e-12), b = std::log(m + 1e-12), c = std::log(mag[k + 1] + 1e-12);
                const double d = a - 2 * b + c, p = d != 0 ? 0.5 * (a - c) / d : 0;
                out.push_back({ (k + p) * sr / N, m });
            }
        }
        // overtone suppression: a peak on the 2nd–5th harmonic of a lower peak loses that harmonic's share
        std::vector<double> raw(out.size()); for (size_t i = 0; i < out.size(); i++) raw[i] = out[i].a;
        for (size_t i = 0; i < out.size(); i++) {
            double sub = 0;
            for (size_t j = 0; j < i; j++) { const double h = out[i].f / out[j].f, hr = std::floor(h + 0.5);
                if (hr >= 2 && hr <= 5 && std::abs(h - hr) / hr < 0.012) sub = std::max(sub, raw[j] * std::pow(0.55, hr - 1)); }
            out[i].a = std::max(0.0, raw[i] - sub);
        }
        out.erase(std::remove_if(out.begin(), out.end(), [](const Peak& q) { return q.a <= 0; }), out.end());
        return out;
    }
    static double pearson(const std::array<double, 12>& a, const double* b, int rot) {
        double ma = 0, mb = 0; for (int i = 0; i < 12; i++) { ma += a[i]; mb += b[i]; } ma /= 12; mb /= 12;
        double num = 0, da = 0, db = 0;
        for (int i = 0; i < 12; i++) { const double x = a[(i + rot) % 12] - ma, y = b[i] - mb; num += x * y; da += x * x; db += y * y; }
        return da > 0 && db > 0 ? num / std::sqrt(da * db) : 0;
    }
    static Key findKey(const std::array<double, 12>& c) {
        static const double PMAJ[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double PMIN[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        static const int CAMMAJ[] = { 8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1 }, CAMMIN[] = { 5, 12, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10 };
        double tot = 0; for (double v : c) tot += v;
        Key k; if (tot <= 0) return k;
        struct S { int t; bool minor; double r; }; std::vector<S> sc;
        for (int t = 0; t < 12; t++) { sc.push_back({ t, false, pearson(c, PMAJ, t) }); sc.push_back({ t, true, pearson(c, PMIN, t) }); }
        std::stable_sort(sc.begin(), sc.end(), [](const S& a, const S& b) { return a.r > b.r; });
        auto nm = [](const S& s) { return std::string(s.minor ? minN(s.t) : majN(s.t)) + (s.minor ? " minor" : " major"); };
        auto cam = [&](const S& s) { return std::to_string(s.minor ? CAMMIN[s.t] : CAMMAJ[s.t]) + (s.minor ? "A" : "B"); };
        k.valid = true; k.tonic = sc[0].t; k.minor = sc[0].minor; k.r = sc[0].r; k.margin = sc[0].r - sc[1].r;
        k.name = nm(sc[0]); k.camelot = cam(sc[0]); k.altName = nm(sc[1]); k.altCamelot = cam(sc[1]);
        return k;
    }
    static Chord findChord(const std::array<double, 12>& c, const std::array<double, 12>& bass) {
        struct T { const char* s; std::vector<int> n; };
        static const std::vector<T> CH = { { "", { 0, 4, 7 } }, { "m", { 0, 3, 7 } }, { "7", { 0, 4, 7, 10 } }, { "maj7", { 0, 4, 7, 11 } },
            { "m7", { 0, 3, 7, 10 } }, { "sus4", { 0, 5, 7 } }, { "sus2", { 0, 2, 7 } }, { "dim", { 0, 3, 6 } }, { "5", { 0, 7 } } };
        double n2 = 0, tot = 0; for (double v : c) { n2 += v * v; tot += v; }
        Chord ch; if (tot <= 0) return ch;
        const double norm = std::sqrt(n2);
        double bsum = 0, bTopV = 0; int bTop = -1;
        for (int i = 0; i < 12; i++) { bsum += bass[i]; if (bass[i] > bTopV) { bTopV = bass[i]; bTop = i; } }
        const int bassRoot = bsum > 0.1 * tot && bTopV > 0.5 * bsum ? bTop : -1;
        int bestR = -1; const T* bestT = nullptr; double best = -1e9;
        for (int r = 0; r < 12; r++) for (const auto& t : CH) {
            double dot = 0; for (int iv : t.n) dot += c[(r + iv) % 12];
            const double score = dot / (norm * std::sqrt((double)t.n.size())) - 0.035 * std::max(0, (int)t.n.size() - 3) - (t.n.size() == 2 ? 0.06 : 0) + (r == bassRoot ? 0.03 : 0);
            if (!bestT || score > best) { best = score; bestR = r; bestT = &t; }
        }
        ch.valid = true; ch.score = best;
        if (best < 0.62) { ch.name = "N.C."; return ch; }
        const std::string s = bestT->s;
        const bool minorish = (s.rfind("m", 0) == 0 && s.rfind("maj", 0) != 0) || s == "dim";
        ch.name = std::string(minorish ? minN(bestR) : majN(bestR)) + s;
        double bmax = 0; int bpc = -1; double btot = 0;
        for (int i = 0; i < 12; i++) { btot += bass[i]; if (bass[i] > bmax) { bmax = bass[i]; bpc = i; } }
        bool inChord = false; for (int iv : bestT->n) if ((bestR + iv) % 12 == bpc) inChord = true;
        if (bpc >= 0 && btot > 0.15 * tot && bmax > 0.6 * btot && bpc != bestR && inChord) ch.name += std::string("/") + majN(bpc);
        return ch;
    }
};

} // namespace kc
