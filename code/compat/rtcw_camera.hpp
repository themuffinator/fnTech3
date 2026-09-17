/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_CAMERA_HPP
#define FNTECH3_RTCW_CAMERA_HPP

// Engine-owned camera state. RTCW SP and MP share the format, with timing
// policy selected at load; no reference classes or module pointers cross this API.
// Independently implemented from the observations credited in CREDITS.md.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace fntech3 { namespace rtcw { namespace camera {

constexpr int SlotCount = 64;
constexpr std::size_t MaxFileBytes = 1024 * 1024;
constexpr std::size_t MaxTargets = 128, MaxEvents = 2048;
constexpr std::size_t MaxPoints = 4096, MaxSamples = 65536, MaxVelocities = 8192;
constexpr float MaxSeconds = 86400.0f;
using Vector = std::array<float, 3>;

struct Failure { const char *message; std::size_t offset = 0; };
struct Diagnostic { const char *message = nullptr; std::size_t offset = 0; };
inline float Finite(float value) {
	// Materialize binary32 before checking it: x87 excess precision must not
	// admit an overflowing float expression that SSE/ARM would reject.
	const volatile float rounded = value;
	if (!std::isfinite(rounded)) throw Failure{"non-finite camera calculation"};
	return rounded;
}
inline bool SameName(const std::string &a, const std::string &b) {
	if (a.size() != b.size()) return false;
	for (std::size_t i = 0; i < a.size(); ++i) {
		const auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
		if (lower(a[i]) != lower(b[i])) return false;
	}
	return true;
}
inline float Distance(const Vector &a, const Vector &b) {
	const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
	return Finite(static_cast<float>(std::sqrt(static_cast<double>(Finite(x*x + y*y + z*z)))));
}
inline Vector Blend(const Vector &a, const Vector &b, float aWeight, float bWeight) {
	Vector out{};
	for (int i = 0; i < 3; ++i) out[i] = Finite(a[i] * aWeight + b[i] * bWeight);
	return out;
}
inline std::int64_t Milliseconds(float seconds) {
	if (!std::isfinite(seconds) || std::fabs(seconds) > MaxSeconds)
		throw Failure{"camera duration exceeds one day"};
	return static_cast<std::int64_t>(seconds * 1000.0f);
}

enum class Shape { Fixed, Interpolated, Spline };
enum class Mode { SinglePlayer, MultiPlayer };
struct Velocity { std::int64_t offset, duration; float speed; };
struct Path {
	Shape shape = Shape::Fixed;
	std::string name = "position";
	Vector first{}, last{};
	std::vector<Vector> samples;
	std::vector<float> lengths;
	std::vector<Velocity> velocities;
	std::int64_t duration = 0, startedAt = 0, sampledAt = 0;
	float speed = 0, distance = 0, travelled = 0;
	bool started = false;

	void Start(std::int64_t now) {
		startedAt = sampledAt = now; travelled = 0; started = true;
		if (shape != Shape::Fixed && duration)
			speed = Finite(distance / (static_cast<float>(duration) / 1000.0f));
	}
	void AddVelocity(std::int64_t offset, std::int64_t durationMs, float value) {
		if (velocities.size() == MaxVelocities) throw Failure{"too many camera velocity intervals"};
		velocities.push_back({offset, durationMs, Finite(value)});
	}
	Vector Position(std::int64_t now) {
		if (shape == Shape::Fixed) return first;
		if (!started) throw Failure{"moving camera target queried before its start event"};
		float velocity = speed;
		for (const auto &entry : velocities) {
			if (now - startedAt >= entry.offset && now - startedAt <= entry.offset + entry.duration) {
				velocity = entry.speed; break; // First inclusive interval wins.
			}
		}
		const float seconds = static_cast<float>(now - sampledAt) / 1000.0f;
		travelled = Finite(travelled + seconds * velocity);
		sampledAt = now;
		if (shape == Shape::Interpolated) {
			const float fraction = distance ? (std::clamp)(travelled / distance, 0.0f, 1.0f) : 0;
			return Blend(first, last, 1.0f - fraction, fraction);
		}
		if (samples.empty()) throw Failure{"spline camera has no sampled points"};
		if (samples.size() == 1 || !distance) return samples.front();
		// The original distance integration is query-cadence dependent. Keep
		// the float distance and double segment walk, including its final
		// segment snap, instead of substituting a time-parameterized spline.
		const double wanted = (static_cast<double>(travelled) / distance) * distance;
		double walked = 0, previous = 0;
		std::size_t i = 1;
		for (; i < samples.size(); ++i) {
			previous = walked; walked += lengths[i - 1];
			if (walked >= wanted) break;
		}
		if (i >= samples.size() - 1) return samples[i - 1];
		if (walked == previous) return samples[i - 1];
		const double fraction = (walked - wanted) / (walked - previous);
		return Blend(samples[i - 1], samples[i], static_cast<float>(fraction),
			static_cast<float>(1.0 - fraction));
	}
};

struct Event {
	int type = 0;
	std::int64_t offset = 0;
	std::string parameter;
	float value = 0, seconds = 0;
	bool triggered = false;
};
struct FieldOfView {
	float value = 90, from = 90, to = 90, duration = 0;
	std::int64_t startedAt = 0;
	float At(std::int64_t now) {
		if (duration) {
			const float fraction = (std::clamp)(static_cast<float>(now - startedAt) / duration, 0.0f, 1.0f);
			value = Finite(from + (to - from) * fraction);
			if (fraction == 1.0f) duration = 0;
		}
		return value;
	}
	void Reset(float begin, float end, std::int64_t now, float seconds) {
		from = begin; to = end; startedAt = now; duration = seconds * 1000.0f;
		// A zero-length reset retains the stored FOV in both original modes.
	}
};
struct Fade { int alpha; float seconds; };
struct Frame {
	Vector origin{};
	float pitch = 0, yaw = 0, fov = 90;
	std::array<Fade, MaxEvents> fades{};
	std::size_t fadeCount = 0;
};
enum class Result { Frame, Ended, Invalid };

struct Definition {
	Mode mode = Mode::SinglePlayer;
	Path position;
	std::vector<Path> targets;
	std::vector<Event> events;
	FieldOfView fov;
	float seconds = 30, totalSeconds = 30;
	std::int64_t startedAt = 0;
	std::size_t activeTarget = 0;
	bool loaded = false, started = false;

	Path &SelectTarget(const std::string &name) {
		for (std::size_t i = 0; i < targets.size(); ++i) {
			if (SameName(targets[i].name, name)) { activeTarget = i; break; }
		}
		if (targets.empty()) { targets.emplace_back(); targets[0].name.clear(); activeTarget = 0; }
		return targets[activeTarget];
	}
	void Start(std::int64_t now) {
		if (!loaded) throw Failure{"camera has not been loaded"};
		if (mode == Mode::SinglePlayer) {
			position.velocities.clear();
			// SP calculates speed before the timeline changes the duration.
			position.Start(now);
		}
		position.duration = Milliseconds(seconds);
		totalSeconds = seconds;
		for (auto &event : events) {
			event.triggered = false;
			if (event.type == 13 && mode == Mode::SinglePlayer) {
				const float step = position.speed / 100.0f;
				float velocity = 0;
				for (int i = 0; i <= 100; ++i) {
					position.AddVelocity(i * 10, 10, velocity);
					velocity = (std::min)(velocity + step, position.speed);
				}
				velocity = position.speed;
				const auto endStart = Milliseconds(seconds) - 1000;
				for (int i = 0; i < 100; ++i) {
					velocity = (std::max)(velocity - step, 0.0f);
					position.AddVelocity(endStart + i * 10, 10, velocity);
				}
			} else if (event.type == 1) {
				totalSeconds = Finite(totalSeconds + event.seconds);
				const auto begin = (std::max)(std::int64_t{0}, event.offset - 1000);
				const auto steps = (event.offset - begin) / 10;
				// Original files with a wait before 10 ms divide by zero.
				if (steps <= 0) throw Failure{"camera wait requires at least 10 ms of deceleration"};
				const float step = position.speed / static_cast<float>(steps);
				float velocity = position.speed;
				for (auto t = begin; t <= event.offset - 10; t += 10) {
					position.AddVelocity(t, 10, velocity); velocity -= step;
				}
				const auto wait = Milliseconds(event.seconds);
				position.AddVelocity(event.offset, wait, 0);
				velocity = 0;
				for (int i = 0; i <= 100; ++i) {
					position.AddVelocity(event.offset + wait + i * 10, 10, velocity); velocity += step;
				}
			}
		}
		const auto totalMs = Milliseconds(totalSeconds);
		std::int64_t assigned = 0;
		for (std::size_t i = 0; i < events.size(); ++i) if (events[i].type == 4) {
			auto duration = totalMs - assigned;
			for (std::size_t j = i + 1; j < events.size(); ++j) if (events[j].type == 4) {
				duration = events[j].offset; break;
			}
			SelectTarget(events[i].parameter).duration = duration;
			assigned += duration;
		}
		// Original MP keeps parsed/prior intervals and calculates speed only
		// after building the timeline. Its event 13 has no feather action.
		if (mode == Mode::MultiPlayer) position.Start(now);
		fov.Reset(90, 90, now, 0);
		startedAt = now; started = true;
	}
	Result Sample(std::int64_t now, Frame &out) {
		if (!loaded || !started) throw Failure{"camera queried before load/start"};
		if ((now - startedAt) / 1000 > totalSeconds) return Result::Ended;
		for (auto &event : events) if (!event.triggered && now >= startedAt + event.offset) {
			event.triggered = true;
			switch (event.type) {
			case 4: SelectTarget(event.parameter).Start(startedAt + event.offset); break;
			case 6: fov.Reset(fov.At(now), event.value, now, event.seconds); break;
			case 11: case 12:
				out.fades[out.fadeCount++] = {event.type == 11 ? 255 : 0, event.seconds}; break;
			case 9: return Result::Ended;
			case 10:
				// Original EVENT_CAMERA returns uninitialized direction and can
				// replace its own object mid-evaluation. Do not publish that as
				// a usable frame. No installed campaign file uses this event.
				throw Failure{"RTCW chained camera events are not implemented"};
			default: break; // The remaining original event types have no query action.
			}
		}
		out.origin = position.Position(now);
		Vector direction{};
		if (!targets.empty()) {
			const Vector target = targets[activeTarget].Position(now);
			const float distance = Distance(target, out.origin);
			if (distance) for (int i = 0; i < 3; ++i) direction[i] = (target[i] - out.origin[i]) * (1.0f / distance);
		}
		out.pitch = Finite(static_cast<float>(std::asin(static_cast<double>((std::clamp)(direction[2], -1.0f, 1.0f))) * 180 / 3.14159));
		out.yaw = Finite(static_cast<float>(std::atan2(static_cast<double>(direction[1]), static_cast<double>(direction[0])) * 180 / 3.14159));
		out.fov = fov.At(now);
		return Result::Frame;
	}
};

// Parser lives separately so all loaded state remains ordinary owned data.
}}}
#include "rtcw_camera_parser.hpp"
namespace fntech3 { namespace rtcw { namespace camera {

class Manager {
	std::array<Definition, SlotCount> slots;
public:
	static bool ValidSlot(std::intptr_t index) { return index >= 0 && index < SlotCount; }
	void Clear() { for (auto &slot : slots) slot = Definition{}; }
	void Clear(int index) { if (ValidSlot(index)) slots[index] = Definition{}; }
	const Definition *Inspect(int index) const { return ValidSlot(index) ? &slots[index] : nullptr; }
	bool Load(int index, const char *text, std::size_t bytes, Diagnostic &diagnostic,
		Mode mode = Mode::SinglePlayer) {
		diagnostic = {};
		if (!ValidSlot(index)) return false;
		Clear(index);
		try {
			Definition candidate;
			candidate.mode = mode;
			Parse(text, bytes, candidate);
			candidate.loaded = true;
			slots[index] = std::move(candidate);
			return true;
		} catch (const Failure &failure) { diagnostic = {failure.message, failure.offset}; }
		catch (const std::bad_alloc &) { diagnostic = {"camera allocation failed", 0}; }
		return false;
	}
	bool Start(int index, std::int32_t time, Diagnostic &diagnostic) {
		diagnostic = {};
		if (!ValidSlot(index)) return false;
		try { slots[index].Start(time); return true; }
		catch (const Failure &failure) { diagnostic = {failure.message, failure.offset}; }
		catch (const std::bad_alloc &) { diagnostic = {"camera allocation failed", 0}; }
		slots[index].started = false;
		return false;
	}
	Result Sample(int index, std::int32_t time, Frame &frame, Diagnostic &diagnostic) {
		diagnostic = {}; frame.fadeCount = 0;
		if (!ValidSlot(index)) return Result::Ended;
		try { return slots[index].Sample(time, frame); }
		catch (const Failure &failure) { diagnostic = {failure.message, failure.offset}; }
		catch (const std::bad_alloc &) { diagnostic = {"camera allocation failed", 0}; }
		slots[index].started = false; frame.fadeCount = 0;
		return Result::Invalid;
	}
};

}}}
#endif
