// Persistent cubic interpolation for independently queued PCM chunks.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace fntech3::audio {
class CubicStreamResampler {
public:
    bool Configure(int inputRate, int outputRate, int channels) {
        if (inputRate <= 0 || outputRate <= 0 || channels <= 0 || channels > 32)
            return false;
        inputRate_ = inputRate; outputRate_ = outputRate; channels_ = channels;
        next_ = 0; history_.clear();
        return true;
    }
    bool Push(const float *samples, int frames, std::vector<float> &output) {
        output.clear();
        if (channels_ == 0 || frames < 0 || (frames && !samples)) return false;
        const auto oldFrames = history_.size() / static_cast<size_t>(channels_);
        if (static_cast<size_t>(frames) > static_cast<size_t>(INT32_MAX) - oldFrames)
            return false;
        const auto total = oldFrames + static_cast<size_t>(frames);
        if (total > history_.max_size() / static_cast<size_t>(channels_) || !OutputFits(total)) return false;
        if (frames) history_.insert(history_.end(), samples,
            samples + static_cast<size_t>(frames) * static_cast<size_t>(channels_));
        Render(output, false);
        return true;
    }
    bool Finish(std::vector<float> &output) {
        output.clear();
        if (channels_ == 0) return true;
        if (!OutputFits(history_.size() / static_cast<size_t>(channels_))) return false;
        Render(output, true);
        history_.clear(); next_ = 0;
        return true;
    }
    size_t BufferedFrames() const {
        return channels_ ? history_.size() / static_cast<size_t>(channels_) : 0;
    }
private:
    bool OutputFits(size_t frames) const {
        const int64_t distance = static_cast<int64_t>(frames) * outputRate_ - next_;
        const int64_t count = distance > 0 ? (distance + inputRate_ - 1) / inputRate_ : 0;
        return count <= INT32_MAX / channels_ &&
            static_cast<uint64_t>(count) <= std::numeric_limits<size_t>::max() / sizeof(float) / channels_;
    }
    void Render(std::vector<float> &output, bool final) {
        const int64_t frames = static_cast<int64_t>(history_.size() / channels_);
        while (next_ < frames * outputRate_) {
            const int64_t base = next_ / outputRate_;
            const int64_t remainder = next_ % outputRate_;
            if (!final && remainder && base + 2 >= frames) break;
            const float f = base >= frames - 1 ? 0.0f : static_cast<float>(remainder) / outputRate_;
            for (int channel = 0; channel < channels_; ++channel) {
                auto sample = [&](int64_t frame) {
                    frame = (std::max)(int64_t{0}, (std::min)(frame, frames - 1));
                    return history_[static_cast<size_t>(frame) * channels_ + channel];
                };
                const float p1 = sample(base);
                float value = p1;
                if (f > 0.0f) {
                    const float p0 = sample(base - 1), p2 = sample(base + 1), p3 = sample(base + 2);
                    const float f2 = f * f, f3 = f2 * f;
                    value = 0.5f * ((2.0f * p1) + (-p0 + p2) * f +
                        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * f2 +
                        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * f3);
                }
                output.push_back(value);
            }
            next_ += inputRate_;
        }
        // Keep p0 and any not-yet-rendered lookahead. Integer phase is relative
        // to the retained history, so neither run length nor chunking adds drift.
        const int64_t discard = (std::max)(int64_t{0},
            (std::min)(frames - 1, next_ / outputRate_ - 1));
        if (discard) {
            history_.erase(history_.begin(), history_.begin() + static_cast<size_t>(discard) * channels_);
            next_ -= discard * outputRate_;
        }
    }
    int inputRate_ = 0, outputRate_ = 0, channels_ = 0;
    int64_t next_ = 0;
    std::vector<float> history_;
};
} // namespace fntech3::audio
