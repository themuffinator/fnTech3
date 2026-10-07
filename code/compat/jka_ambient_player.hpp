/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_AMBIENT_PLAYER_HPP
#define FNTECH3_JKA_AMBIENT_PLAYER_HPP
#include <array>
#include <cstddef>
#include <cstdint>

/* Jedi Academy multiplayer's ambient-set playback, as retail jamp.exe 1.01's
 * engine plays the set the client game names each frame (client-game import
 * 48, S_UpdateAmbientSet 0x468420), measured with Capstone. Independently
 * written; it owns no sound and calls a sink for each loop and wave.
 *
 * - The set the listener is in becomes current; the one it replaces becomes
 *   old (0x468020). Each set is a set of one parse of sound/sound.txt, and a
 *   parse adds its sets to those of earlier maps under new numbers
 *   (0x467c50), so a map's set is new even when its name is not, and the
 *   previous map's set fades out on the next map.
 * - The new current set fades in from 0 and the old one out from 255, each
 *   over 1,000 ms of the client's real time (0x467ec0, cls.realtime at
 *   0x8af224, the fade time at 0x56d538), truncated to whole volumes; the old
 *   set is let go once it reaches 0.
 * - Each frame both play (0x4682c0): the looped wave at the set's volume, and,
 *   once the real time since the slot's last wave reaches a whole number of
 *   seconds drawn afresh each frame between the set's bounds, one of its
 *   subwaves at a volume drawn between its bounds scaled by the set's own
 *   volume, never above it. The two slots' wave times stay with the slots,
 *   not with the sets in them.
 * - Draws come from the engine's own generator (0x56e760, starting at
 *   0x89abcdef): x = x * 0x343fd + 0x269ec3, a number in [lo, hi] being
 *   lo + (((hi - lo + 1) * (x >> 17)) >> 15).
 */
namespace fntech3::jka::ambient {
inline constexpr int MaxSetVolume = 255;
inline constexpr int CrossFadeMilliseconds = 1000;
inline constexpr std::size_t MaxPlayWaves = 8;

/* A set as the player keeps it: the parse's numbers and sound handles, and
 * the volume it has faded to. A copy, so a set of an earlier map plays on. */
struct PlaySet {
    std::uint32_t id = 0;               // unique for every set of every parse
    int loop = 0;                       // the looped wave's handle, or 0
    std::array<int, MaxPlayWaves> waves{};
    std::size_t waveCount = 0;
    std::uint32_t minimumSeconds = 10, maximumSeconds = 25;
    int minimumVolume = MaxSetVolume, maximumVolume = MaxSetVolume;
    int masterVolume = MaxSetVolume;
    int fadeTime = 0;
};

class Random {
public:
    std::uint32_t state = 0x89abcdefu;
    std::uint32_t Next() noexcept { state = state * 0x343fdu + 0x269ec3u; return state >> 17; }
    /* A number from lo to hi, wrapping as retail's 32-bit arithmetic does. */
    int Range(int lo, int hi) noexcept {
        const std::uint32_t span = std::uint32_t(hi) - std::uint32_t(lo) + 1u;
        const std::int32_t product = std::int32_t(span * Next());
        return std::int32_t(std::uint32_t(product >> 15) + std::uint32_t(lo));
    }
};

/* Retail's x87 arithmetic at the double precision its runtime sets: two whole
 * numbers each stored as a float, their quotient stored as a float, and a
 * float times a float, truncated (_ftol 0x539584). */
inline float StoredQuotient(std::int32_t numerator, std::int32_t denominator) noexcept {
    return static_cast<float>(static_cast<double>(static_cast<float>(numerator)) /
        static_cast<double>(static_cast<float>(denominator)));
}
inline int TruncatedProduct(float value, float factor) noexcept {
    return static_cast<int>(static_cast<double>(value) * static_cast<double>(factor));
}

inline int FadedVolume(int now, int fadeTime) noexcept {
    const auto elapsed = static_cast<std::int32_t>(std::uint32_t(now) - std::uint32_t(fadeTime));
    return TruncatedProduct(StoredQuotient(elapsed, CrossFadeMilliseconds), static_cast<float>(MaxSetVolume));
}

class Player {
public:
    /* Plays `set` for this frame at `origin`. `sink.Loop(origin, handle,
     * volume)` and `sink.Wave(origin, handle, volume)` take volumes 0..255. */
    template<class Sink> void Update(const PlaySet &set, const float *origin, int now, Sink &sink) {
        Choose(set, now);
        if (hasCurrent_) Play(origin, current_, currentWave_, now, sink);
        if (hasOld_) Play(origin, old_, oldWave_, now, sink);
    }
    void Reset() noexcept { *this = Player{}; }

    bool HasCurrent() const noexcept { return hasCurrent_; }
    bool HasOld() const noexcept { return hasOld_; }
    const PlaySet &Current() const noexcept { return current_; }
    const PlaySet &Old() const noexcept { return old_; }
    int CurrentWaveTime() const noexcept { return currentWave_; }
    int OldWaveTime() const noexcept { return oldWave_; }
    Random &Generator() noexcept { return random_; }

private:
    /* 0x468020: a new set fades in, the one it replaces out. */
    void Choose(const PlaySet &set, int now) {
        if (!hasCurrent_ || set.id != current_.id) {
            hasOld_ = hasCurrent_;
            old_ = current_;
            current_ = set;
            hasCurrent_ = true;
            if (hasOld_) { old_.masterVolume = MaxSetVolume; old_.fadeTime = now; }
            current_.masterVolume = 0;
            current_.fadeTime = now;
        }
        // 0x467ec0
        if (current_.masterVolume < MaxSetVolume) current_.masterVolume = FadedVolume(now, current_.fadeTime);
        if (current_.masterVolume > MaxSetVolume) current_.masterVolume = MaxSetVolume;
        if (!hasOld_) return;
        if (old_.masterVolume > 0) old_.masterVolume = MaxSetVolume - FadedVolume(now, old_.fadeTime);
        if (old_.masterVolume <= 0) { old_.masterVolume = 0; hasOld_ = false; }
    }

    /* 0x4682c0 */
    template<class Sink> void Play(const float *origin, const PlaySet &set, int &lastWave, int now, Sink &sink) {
        const int master = static_cast<std::uint8_t>(set.masterVolume);
        if (set.loop) sink.Loop(origin, set.loop, master);
        const std::int32_t threshold = std::int32_t(std::uint32_t(random_.Range(
            int(set.minimumSeconds), int(set.maximumSeconds))) * 1000u);
        if (std::int32_t(std::uint32_t(now) - std::uint32_t(lastWave)) < threshold) return;
        lastWave = now;
        const float scale = StoredQuotient(set.masterVolume, MaxSetVolume);
        const int lo = TruncatedProduct(static_cast<float>(std::uint32_t(set.minimumVolume)), scale);
        const int hi = TruncatedProduct(static_cast<float>(std::uint32_t(set.maximumVolume)), scale);
        int volume = static_cast<std::uint8_t>(random_.Range(lo, hi));
        if (volume > set.masterVolume) volume = static_cast<std::uint8_t>(set.masterVolume);
        const auto count = static_cast<std::uint8_t>(set.waveCount);
        if (!count) return;
        const std::size_t index = (std::uint32_t(count) * random_.Next()) >> 15;
        sink.Wave(origin, set.waves[index], volume);
    }

    PlaySet current_{}, old_{};
    bool hasCurrent_ = false, hasOld_ = false;
    int currentWave_ = 0, oldWave_ = 0;    // 0x12a4d0c, 0x12a4d10
    Random random_{};
};
} // namespace fntech3::jka::ambient
#endif
