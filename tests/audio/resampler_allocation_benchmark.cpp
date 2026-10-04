// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone allocation witness; compare headers with the include search path.
#include <AudioStreamResampler.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <utility>

static size_t allocations = 0;
void *operator new(size_t bytes) { ++allocations; if (auto *p = std::malloc(bytes)) return p; throw std::bad_alloc(); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }

int main() {
    using fntech3::audio::CubicStreamResampler;
    for (int channels : {2, 6}) {
        CubicStreamResampler resampler;
        if (!resampler.Configure(44100, 48000, channels)) return 1;
        std::vector<float> input(size_t(8192) * channels, 1234);
        size_t samples = 0;
        allocations = 0;
        auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; ++i) {
            auto next = resampler;
            std::vector<float> output;
            if (!next.Push(input.data(), 8192, output)) return 2;
            samples += output.size();
            resampler = std::move(next);
        }
        std::vector<float> tail;
        if (!resampler.Finish(tail)) return 3;
        samples += tail.size();
        if (samples != size_t((int64_t(8192) * 1000 * 48000 + 44100 - 1) / 44100) * channels) return 4;
        auto count = allocations;
        auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
        std::printf("channels=%d samples=%zu allocations=%zu elapsed_ms=%.3f\n", channels, samples, count, elapsed);
    }
}
