// Persistent band-limited sample-rate conversion of PCM chunks.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace fntech3::audio {
class CubicStreamResampler {
    // This is a chunk converter, not whole-file storage. Bound both sides of
    // rate amplification before reserving memory or reading the input pointer.
    static constexpr size_t MaxChunkSamples = 4 * 1024 * 1024;
    struct PolyphaseFilter {
        static constexpr int Phases = 128;
        int radius;
        std::vector<float> coefficients;
    };
public:
    static bool ChunkSizeFits(int frames, int channels) {
        return frames >= 0 && channels > 0 && channels <= 32 &&
            static_cast<size_t>(frames) <= MaxChunkSamples / static_cast<size_t>(channels);
    }
    bool Configure(int inputRate, int outputRate, int channels) {
        if (inputRate <= 0 || outputRate <= 0 || channels <= 0 || channels > 32)
            return false;
        inputRate_ = inputRate; outputRate_ = outputRate; channels_ = channels;
        next_ = 0; history_.clear();
        filter_.reset();
        if (inputRate != outputRate) {
            auto filter = std::make_shared<PolyphaseFilter>();
            // Sixty-four lobes with a Blackman window. Scale the support with
            // the rate ratio, bounded even for malformed/extreme rates.
            filter->radius = static_cast<int>((std::min)(2048.0,
                std::ceil(64.0 * (std::max)(1.0, static_cast<double>(inputRate) / outputRate))));
            const int taps = 2 * filter->radius;
            filter->coefficients.resize(static_cast<size_t>(PolyphaseFilter::Phases + 1) * taps);
            constexpr double pi = 3.14159265358979323846;
            const double cutoff = 0.95 * (std::min)(1.0, static_cast<double>(outputRate) / inputRate);
            for (int phase = 0; phase <= PolyphaseFilter::Phases; ++phase) {
                const double fraction = static_cast<double>(phase) / PolyphaseFilter::Phases;
                auto *row = filter->coefficients.data() + static_cast<size_t>(phase) * taps;
                double sum = 0;
                for (int tap = 0; tap < taps; ++tap) {
                    const double distance = tap - filter->radius + 1 - fraction;
                    const double windowPosition = distance / filter->radius;
                    const double window = std::fabs(windowPosition) >= 1.0 ? 0.0 :
                        0.42 + 0.5 * std::cos(pi * windowPosition) + 0.08 * std::cos(2 * pi * windowPosition);
                    const double sinc = distance == 0.0 ? cutoff : std::sin(pi * cutoff * distance) / (pi * distance);
                    row[tap] = static_cast<float>(sinc * window);
                    sum += row[tap];
                }
                for (int tap = 0; tap < taps; ++tap) row[tap] = static_cast<float>(row[tap] / sum);
            }
            // Conversion transactions share this immutable table. No filter
            // construction, trigonometry or table copies occur per chunk.
            filter_ = std::move(filter);
        }
        return true;
    }
    bool Push(const float *samples, int frames, std::vector<float> &output) {
        output.clear();
        if (channels_ == 0 || frames < 0 || (frames && !samples)) return false;
        const auto oldFrames = history_.size() / static_cast<size_t>(channels_);
        if (static_cast<size_t>(frames) > static_cast<size_t>(INT32_MAX) - oldFrames)
            return false;
        const auto total = oldFrames + static_cast<size_t>(frames);
        if (total > MaxChunkSamples / static_cast<size_t>(channels_) ||
            total > history_.max_size() / static_cast<size_t>(channels_) || !OutputFits(total)) return false;
        if (frames) history_.insert(history_.end(), samples,
            samples + static_cast<size_t>(frames) * static_cast<size_t>(channels_));
        Render(output, false);
        return true;
    }
    bool Finish(std::vector<float> &output) {
        if (!Drain(output)) return false;
        history_.clear(); next_ = 0;
        return true;
    }
    // A raw stream has no EOF notification. Drain known frames with endpoint
    // padding, retaining phase/left support in case the next chunk follows.
    bool Drain(std::vector<float> &output) {
        output.clear();
        if (channels_ == 0) return true;
        if (!OutputFits(history_.size() / static_cast<size_t>(channels_))) return false;
        Render(output, true);
        return true;
    }
    size_t BufferedFrames() const {
        return channels_ ? history_.size() / static_cast<size_t>(channels_) : 0;
    }
    bool HasPendingOutput() const {
        return channels_ && next_ < static_cast<int64_t>(BufferedFrames()) * outputRate_;
    }
private:
    bool OutputFits(size_t frames) const {
        const int64_t distance = static_cast<int64_t>(frames) * outputRate_ - next_;
        const int64_t count = distance > 0 ? (distance + inputRate_ - 1) / inputRate_ : 0;
        return count <= static_cast<int64_t>(MaxChunkSamples / channels_) && count <= INT32_MAX / channels_ &&
            static_cast<uint64_t>(count) <= std::numeric_limits<size_t>::max() / sizeof(float) / channels_;
    }
    template<int FixedChannels>
    void RenderFilteredFrame(int64_t base, int64_t remainder, int64_t frames, std::vector<float> &output) const {
        const int channels = FixedChannels ? FixedChannels : channels_;
        const int taps = 2 * filter_->radius;
        const int phase = static_cast<int>(remainder * PolyphaseFilter::Phases / outputRate_);
        const float blend = static_cast<float>(remainder * PolyphaseFilter::Phases % outputRate_) / outputRate_;
        const auto *row = filter_->coefficients.data() + static_cast<size_t>(phase) * taps;
        const auto *center = history_.data() + static_cast<size_t>(base) * channels;
        std::array<double, FixedChannels ? FixedChannels : 32> values{};
        for (int channel = 0; channel < channels; ++channel) values[channel] = center[channel];
        auto accumulate = [&](const float *samples, float weight) {
            // Center on p1 to preserve DC and one-frame streams exactly,
            // independent of coefficient rounding. Route all channels through
            // each weight once, keeping interleaved PCM access contiguous.
            for (int channel = 0; channel < channels; ++channel)
                values[channel] += weight * static_cast<double>(samples[channel] - center[channel]);
        };
        const int64_t first = base - filter_->radius + 1;
        if (first >= 0 && base + filter_->radius < frames) {
            // Most frames have contiguous support. Avoid per-tap bounds
            // checks and let common channel counts unroll/vectorize.
            auto *samples = history_.data() + static_cast<size_t>(first) * channels;
            for (int tap = 0; tap < taps; ++tap, samples += channels)
                accumulate(samples, row[tap] + blend * (row[tap + taps] - row[tap]));
        } else {
            for (int tap = 0; tap < taps; ++tap) {
                const int64_t frame = (std::max)(int64_t{0}, (std::min)(frames - 1, first + tap));
                accumulate(history_.data() + static_cast<size_t>(frame) * channels,
                    row[tap] + blend * (row[tap + taps] - row[tap]));
            }
        }
        for (int channel = 0; channel < channels; ++channel) output.push_back(static_cast<float>(values[channel]));
    }
    void Render(std::vector<float> &output, bool final) {
        const int64_t frames = static_cast<int64_t>(history_.size() / channels_);
        // Push/Finish already validated this bound. Reserve once per chunk
        // instead of repeatedly reallocating and copying interleaved PCM.
        const int64_t distance = frames * outputRate_ - next_;
        const int64_t count = distance > 0 ? (distance + inputRate_ - 1) / inputRate_ : 0;
        output.reserve(static_cast<size_t>(count) * channels_);
        while (next_ < frames * outputRate_) {
            const int64_t base = next_ / outputRate_;
            const int64_t remainder = next_ % outputRate_;
            if (!final && filter_ && base + filter_->radius >= frames) break;
            if (filter_) {
                switch (channels_) {
                case 1: RenderFilteredFrame<1>(base, remainder, frames, output); break;
                case 2: RenderFilteredFrame<2>(base, remainder, frames, output); break;
                case 6: RenderFilteredFrame<6>(base, remainder, frames, output); break;
                default: RenderFilteredFrame<0>(base, remainder, frames, output); break;
                }
            } else {
                // Equal-rate PCM is copied exactly and needs no lookahead.
                const auto *frame = history_.data() + static_cast<size_t>(base) * channels_;
                output.insert(output.end(), frame, frame + channels_);
            }
            next_ += inputRate_;
        }
        // Keep left support and not-yet-rendered lookahead. Integer phase is relative
        // to the retained history, so neither run length nor chunking adds drift.
        const int64_t discard = (std::max)(int64_t{0},
            (std::min)(frames - 1, next_ / outputRate_ - (filter_ ? filter_->radius : 1)));
        if (discard) {
            history_.erase(history_.begin(), history_.begin() + static_cast<size_t>(discard) * channels_);
            next_ -= discard * outputRate_;
        }
    }
    int inputRate_ = 0, outputRate_ = 0, channels_ = 0;
    int64_t next_ = 0;
    std::vector<float> history_;
    std::shared_ptr<const PolyphaseFilter> filter_;
};
} // namespace fntech3::audio
