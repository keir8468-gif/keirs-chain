// Runs the C++ chain on the same input and compares with the browser reference output.
#include "../Source/dsp/ChainDSP.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <sstream>
using namespace kc;
static std::vector<float> readF32(const std::string& p) { std::ifstream f(p, std::ios::binary | std::ios::ate); size_t s = f.tellg(); f.seekg(0); std::vector<float> v(s / 4); f.read((char*)v.data(), s); return v; }
static std::string field(const std::string& obj, const std::string& k) { auto i = obj.find("\"" + k + "\":"); i += k.size() + 3; auto e = obj.find_first_of(",}", i); return obj.substr(i, e - i); }
int main() {
    auto in = readF32("in.f32"); const int n = (int)in.size() / 2; const float* inL = in.data(); const float* inR = in.data() + n;
    std::ifstream cf("cases.json"); std::stringstream ss; ss << cf.rdbuf(); std::string all = ss.str();
    size_t pos = 0; int fails = 0;
    while ((pos = all.find('{', pos)) != std::string::npos) {
        auto end = all.find('}', pos); std::string o = all.substr(pos, end - pos + 1); pos = end;
        auto name = field(o, "name"); name = name.substr(1, name.size() - 2);
        Chain::Params P; P.timing.on = field(o, "tOn") == "true"; P.timing.bpm = 124;
        auto g = field(o, "grid"); const char* gs[] = { "\"1/8\"", "\"1/8T\"", "\"1/16\"", "\"1/16T\"", "\"1/32\"", "\"1/32T\"" };
        for (int i = 0; i < 6; i++) if (g == gs[i]) P.timing.grid = (Grid)i;
        P.timing.swing = std::stod(field(o, "swing")); P.timing.earlier = field(o, "earlier") == "true"; P.timing.shiftAmt = std::stod(field(o, "shift"));
        P.mkOn = field(o, "mkOn") == "true"; P.drive = std::stod(field(o, "drive"));
        P.eqOn = field(o, "eqOn") == "true"; P.bright = std::stod(field(o, "bright")); P.brightFreq = std::stod(field(o, "brightFreq")); P.eqPost = field(o, "eqPost") == "true";
        P.xlOn = field(o, "xlOn") == "true"; P.outDb = std::stod(field(o, "out"));
        Chain ch; ch.prepare(44100, 512); ch.set(P, true);
        std::vector<float> oL(n), oR(n);
        for (int i = 0; i < n; i += 128) { const int m = std::min(128, n - i); ch.process(inL + i, inR + i, oL.data() + i, oR.data() + i, m, (double)i); }
        auto ref = readF32("ref_" + name + ".f32");
        double maxd = 0, se = 0, sr = 0; int at = -1;
        for (int i = 0; i < n; i++) for (int c = 0; c < 2; c++) {
            const double a = c ? oR[i] : oL[i], b = ref[c * n + i], d = std::abs(a - b);
            if (d > maxd) { maxd = d; at = i; } se += d * d; sr += b * b;
        }
        const double rel = 10 * std::log10(se / sr + 1e-300);
        std::printf("%-9s latency %3d | max diff %.2e (at %d) | error %.1f dB below signal %s\n", name.c_str(), ch.latency(), maxd, at, rel, maxd < 1e-4 ? "MATCH" : "DIFFERS");
        if (maxd >= 1e-4) fails++;
    }
    return fails;
}
