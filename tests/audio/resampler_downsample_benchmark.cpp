// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone throughput witness; reports wall time without asserting a deadline.
#include <AudioStreamResampler.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <utility>

int main() {
    using fntech3::audio::CubicStreamResampler;
    for (int rate : {8000, 22050, 44100, 96000, 48000}) for (int channels : {2, 6}) {
        const int outputRate = rate == 48000 ? 44100 : 48000;
        const auto beginInit = std::chrono::steady_clock::now();
        CubicStreamResampler resampler;
        if (!resampler.Configure(rate, outputRate, channels)) return 1;
        const auto init = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - beginInit).count();
        constexpr int frames = 8192, chunks = 100;
        std::vector<float> input(size_t(frames) * channels);
        for (size_t i = 0; i < input.size(); ++i) input[i] = float(8192 * std::sin(i * .043));
        size_t samples = 0;
        const auto begin = std::chrono::steady_clock::now();
        for (int chunk = 0; chunk < chunks; ++chunk) {
            auto next = resampler;
            std::vector<float> output;
            if (!next.Push(input.data(), frames, output)) return 2;
            samples += output.size();
            resampler = std::move(next);
        }
        std::vector<float> tail;
        if (!resampler.Finish(tail)) return 3;
        samples += tail.size();
        if (samples != size_t((int64_t(frames) * chunks * outputRate + rate - 1) / rate) * channels) return 4;
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
        const double duration = double(frames) * chunks * 1000 / rate;
        std::printf("input=%d output=%d channels=%d init_ms=%.3f elapsed_ms=%.3f audio_ms=%.3f elapsed_ratio_percent=%.3f samples=%zu\n",
            rate, outputRate, channels, init, elapsed, duration, 100 * elapsed / duration, samples);
    }
}
