#include "../Source/dsp/KeyDetector.h"
#include <cstdio>
#include <fstream>
#include <string>
int main() {
    const char* files[] = { "osaka_breakdown.wav", "groove_124.wav", "00_dry.wav" }; const int srs[] = { 48000, 44100, 44100 }; int bad = 0;
    for (int f = 0; f < 3; f++) {
        std::ifstream in(std::string("/tmp/kd_") + files[f] + ".f32", std::ios::binary | std::ios::ate); size_t s = in.tellg(); in.seekg(0);
        std::vector<float> x(s / 4); in.read((char*)x.data(), s);
        std::ifstream tr(std::string("/tmp/kdtrail_") + files[f] + ".txt"); std::string line; std::vector<std::string> ref; while (std::getline(tr, line)) ref.push_back(line);
        const int sr = srs[f], N = 16384, hop = (int)std::floor(sr * 0.1 + 0.5); kc::KeyDetector kd(sr, N); size_t idx = 0, match = 0; std::string last;
        for (size_t o = N; o <= x.size(); o += hop, idx++) {
            kd.push(x.data() + o - N, (double)hop / sr);
            last = (kd.key.valid ? kd.key.name + "|" + kd.key.camelot : "-") + "|" + (kd.chord.valid ? kd.chord.name : "-");
            if (idx < ref.size() && ref[idx] == last) match++;
        }
        std::printf("%-20s %zu/%zu updates identical | final %s | tuning %.3f\n", files[f], match, ref.size(), last.c_str(), kd.tuning * 100);
        if (match != ref.size()) bad++;
    }
    return bad;
}
