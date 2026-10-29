#include "../Source/dsp/KeyDetector.h"
#include <cstdio>
#include <fstream>
#include <string>
int main() {
    std::ifstream in("/tmp/kd_osaka_breakdown.wav.f32", std::ios::binary | std::ios::ate); size_t s = in.tellg(); in.seekg(0);
    std::vector<float> x(s / 4); in.read((char*)x.data(), s);
    std::ifstream tr("/tmp/kdtrail_osaka_breakdown.wav.txt"); std::string line; std::vector<std::string> ref; while (std::getline(tr, line)) ref.push_back(line);
    const int sr = 48000, N = 16384, hop = 4800; kc::KeyDetector kd(sr, N); size_t idx = 0;
    for (size_t o = N; o <= x.size(); o += hop, idx++) {
        kd.push(x.data() + o - N, (double)hop / sr);
        std::string me = (kd.key.valid ? kd.key.name + "|" + kd.key.camelot : "-") + "|" + (kd.chord.valid ? kd.chord.name : "-");
        if (me != ref[idx]) std::printf("%zu: js=%s  c++=%s  (chord score %.6f)\n", idx, ref[idx].c_str(), me.c_str(), kd.chord.score);
    }
}
