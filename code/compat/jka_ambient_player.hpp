/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_JKA_AMBIENT_PLAYER_HPP
#define FNTECH3_JKA_AMBIENT_PLAYER_HPP
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>

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
 *
 * A set can also be heard around a point (client-game import 51,
 * AS_AddLocalSet 0x4684f0, AS_PlayLocalSet 0x4680d0); see PlayLocal.
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

    /* The volume a set's own record holds (+0x80), which a set heard around
     * a point plays at (0x46817a): 255 as the set was parsed (0x4672c6), or
     * whatever the fades last wrote into it, since they write the record
     * itself (0x467f5b, 0x468098, 0x4680ae). */
    int MasterVolume(std::uint32_t id) const {
        if (hasCurrent_ && current_.id == id) return current_.masterVolume;
        if (hasOld_ && old_.id == id) return old_.masterVolume;
        const auto found = faded_.find(id);
        return found == faded_.end() ? MaxSetVolume : found->second;
    }

private:
    /* 0x468020: a new set fades in, the one it replaces out. */
    void Choose(const PlaySet &set, int now) {
        if (!hasCurrent_ || set.id != current_.id) {
            if (hasOld_) faded_[old_.id] = old_.masterVolume; // its record keeps the level it reached
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
        if (old_.masterVolume <= 0) { old_.masterVolume = 0; hasOld_ = false; faded_[old_.id] = 0; }
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
    std::map<std::uint32_t, int> faded_;   // sets the fades have left
};

/* The distance from the listener to a point as AS_PlayLocalSet measures it:
 * each difference stored as a float (0x4680ef..0x46810d), and VectorLength
 * (0x455e70) summing the squares at the double precision retail's runtime
 * sets before storing the root as a float. */
inline float LocalDistance(const float *listener, const float *origin) noexcept {
    const auto difference = [&](int axis) {
        return static_cast<double>(static_cast<float>(
            static_cast<double>(origin[axis]) - static_cast<double>(listener[axis])));
    };
    const double x = difference(0), y = difference(1), z = difference(2);
    return static_cast<float>(std::sqrt(x * x + y * y + z * z));
}

/* The volume a set around a point plays its loop at (0x468111..0x468195):
 * the set's own volume within half its radius, falling in a straight line to
 * nothing at the radius, each step stored as a float, the product truncated
 * and kept to its low byte. Nothing (radius 0 at the point itself) falls
 * through both of retail's tests and truncates to nothing. */
inline int LocalVolume(float distance, int radius, int masterVolume) noexcept {
    const float full = static_cast<float>(radius);
    const float half = static_cast<float>(static_cast<double>(full) * 0.5);
    float fraction = 1.0f;
    if (!(distance < half)) {
        if (!(half != 0.0f)) return 0; // retail divides by it: -inf or nothing
        fraction = static_cast<float>((static_cast<double>(full) - static_cast<double>(distance)) /
            static_cast<double>(half));
        if (fraction > 1.0f || fraction < 0.0f) return 0;
    }
    const double product = static_cast<double>(static_cast<float>(masterVolume)) * static_cast<double>(fraction);
    if (std::isnan(product)) return 0;
    return static_cast<std::uint8_t>(static_cast<std::int64_t>(product));
}

/* Client-game import 51: a set heard around a point, as retail jamp.exe
 * 1.01's AS_AddLocalSet (0x4684f0) and AS_PlayLocalSet (0x4680d0) play it,
 * measured with Capstone and run in-process on synthetic cases.
 *
 * - The loop, if the set has one, at the point at LocalVolume (0x468197,
 *   S_AddAmbientLoopingSound 0x46b270).
 * - A whole number of seconds is drawn from the set's bounds on every call,
 *   from the generator the general sets draw from. The answer is the
 *   client's real time (cls.realtime 0x8af224) once the real time less the
 *   time the client game hands over reaches it, and that time otherwise.
 * - A subwave plays when the real time less the last subwave time reaches
 *   it. Retail takes the handed time as that last time; single player hands
 *   back each answer, but the multiplayer client game hands over its own
 *   clock, cg.time, and drops the answer (cgamex86.dll 1.01 0x30035519), so
 *   there retail's subwaves come never or on almost every frame, as the two
 *   clocks happen to lie. The engine keeps each entity's last subwave time
 *   instead (`lastWave`), as retail keeps the general sets' (0x12a4d0c), so
 *   the subwaves come at the intervals sound/sound.txt gives; passing the
 *   handed time as `lastWave` is retail's own reckoning.
 * - When one plays, a volume is drawn between the set's bounds scaled by the
 *   loop's volume, never clamped (0x4681eb), and one of the subwaves is
 *   chosen; a set without subwaves draws the volume and stops. The subwave
 *   plays at the point, on the client game's entity (S_StartAmbientSound
 *   0x46aaf0).
 *
 * `set.masterVolume` is the set record's own (Player::MasterVolume).
 * `sink.Loop(origin, handle, volume)` and `sink.EntityWave(origin, entity,
 * handle, volume)` take volumes 0..255; handles may be 0 (a "null" subwave),
 * which retail still hands over. Returns the time import 51 answers. */
template<class Sink> std::int32_t PlayLocal(const PlaySet &set, int radius, const float *listener,
    const float *origin, int entity, std::int32_t time, std::int32_t now, std::int32_t &lastWave,
    Random &random, Sink &sink) {
    const int volume = LocalVolume(LocalDistance(listener, origin), radius, set.masterVolume);
    if (set.loop) sink.Loop(origin, set.loop, volume);
    const std::int32_t threshold = std::int32_t(std::uint32_t(random.Range(
        int(set.minimumSeconds), int(set.maximumSeconds))) * 1000u);
    const auto since = [now](std::int32_t then) { return std::int32_t(std::uint32_t(now) - std::uint32_t(then)); };
    const std::int32_t answer = since(time) < threshold ? time : now;
    if (since(lastWave) < threshold) return answer;
    lastWave = now;
    const float scale = StoredQuotient(volume, MaxSetVolume);
    const int lo = TruncatedProduct(static_cast<float>(std::uint32_t(set.minimumVolume)), scale);
    const int hi = TruncatedProduct(static_cast<float>(std::uint32_t(set.maximumVolume)), scale);
    const int waveVolume = static_cast<std::uint8_t>(random.Range(lo, hi));
    const auto count = static_cast<std::uint8_t>(set.waveCount);
    if (!count) return answer;
    const std::size_t index = (std::uint32_t(count) * random.Next()) >> 15;
    if (index < set.waves.size()) sink.EntityWave(origin, entity, set.waves[index], waveVolume);
    return answer;
}
} // namespace fntech3::jka::ambient
#endif
