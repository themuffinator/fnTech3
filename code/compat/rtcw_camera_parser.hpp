/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_CAMERA_PARSER_HPP
#define FNTECH3_RTCW_CAMERA_PARSER_HPP
// Included by rtcw_camera.hpp after the owned camera types.
#include <cstdlib>
#include <cerrno>

namespace fntech3 { namespace rtcw { namespace camera {

class Parser {
	const char *data;
	std::size_t size, cursor = 0, tokens = 0, sampleCount = 0;
	[[noreturn]] void Fail(const char *message) const { throw Failure{message, cursor}; }
	static bool Digit(char c) { return c >= '0' && c <= '9'; }
	static bool Letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
	char At(std::size_t ahead = 0) const { return ahead < size - cursor ? data[cursor + ahead] : 0; }
	void Space() {
		for (;;) {
			while (cursor < size && static_cast<unsigned char>(At()) <= ' ') ++cursor;
			if (At() != '/') return;
			if (At(1) == '/') {
				cursor += 2; while (cursor < size && At() != '\n') ++cursor;
			} else if (At(1) == '*') {
				cursor += 2;
				while (cursor < size && !(At() == '*' && At(1) == '/')) ++cursor;
				if (cursor == size) Fail("unterminated camera comment");
				cursor += 2;
			} else return;
		}
	}
	std::string Next() {
		Space();
		if (cursor == size) Fail("unexpected end of camera file");
		if (++tokens > 262144) Fail("too many camera tokens");
		std::string result;
		const auto add = [&] {
			if (result.size() == 1023) Fail("camera token exceeds 1023 bytes");
			result += At(); ++cursor;
		};
		if (At() == '"') {
			++cursor;
			while (cursor < size && At() != '"') {
				// Original quoted-token handling retains the backslash when
				// consuming an escaped quote. Camera names use that spelling.
				if (At() == '\\' && At(1) == '"') { add(); ++cursor; }
				else add();
			}
			if (cursor == size) Fail("unterminated camera string");
			++cursor;
		} else if (Digit(At()) || (At() == '-' && Digit(At(1))) || (At() == '.' && Digit(At(1)))) {
			add(); while (Digit(At()) || At() == '.') add();
			if (At() == 'e' || At() == 'E') {
				add(); if (At() == '+' || At() == '-') add();
				if (!Digit(At())) Fail("invalid camera exponent");
				while (Digit(At())) add();
			}
		} else if (Letter(At()) || At() == '_' || At() == '/' || At() == '\\') {
			add();
			while (Letter(At()) || Digit(At()) || At() == '_' || At() == '/' || At() == '\\' || At() == ':' || At() == '.') add();
		} else add();
		return result;
	}
	void Expect(const char *token) { if (Next() != token) Fail("unexpected camera punctuation"); }
	double Number(const std::string &text) {
		if (text.empty()) Fail("missing camera number");
		char *end = nullptr;
		errno = 0;
		const double value = std::strtod(text.c_str(), &end);
		if (end != text.c_str() + text.size() || !std::isfinite(value) || errno == ERANGE)
			Fail("invalid or overflowing camera number");
		return value;
	}
	float Narrow(double value) {
		if (std::fabs(value) > (std::numeric_limits<float>::max)()) Fail("camera number exceeds binary32");
		return Finite(static_cast<float>(value));
	}
	float Float() { return Narrow(Number(Next())); }
	std::int64_t Integer() {
		const double value = Number(Next());
		if (value < INT32_MIN || value > INT32_MAX) Fail("camera integer exceeds signed 32 bits");
		return static_cast<std::int32_t>(value);
	}
	Vector Point() {
		Expect("("); Vector value{};
		for (auto &coordinate : value) coordinate = Float();
		Expect(")"); return value;
	}
	void Spline(Path &path) {
		Expect("{");
		float granularity = 0.025f;
		std::vector<Vector> controls;
		for (;;) {
			const auto saved = cursor;
			const auto key = Next();
			if (key == "}") break;
			if (key == "(") {
				if (controls.size() == MaxPoints) Fail("too many spline control points");
				cursor = saved; controls.push_back(Point());
			} else if (SameName(key, "granularity")) granularity = Float();
			else Next(); // Name and unknown spline metadata consume one value.
		}
		if (!(granularity > 0) || controls.size() < 4) Fail("spline requires positive granularity and four control points");
		for (std::size_t end = 3; end < controls.size(); ++end) {
			for (float t = 0; t < 1.001f;) {
				if (sampleCount == MaxSamples) Fail("too many sampled camera points");
				++sampleCount;
				const double cube = std::pow(static_cast<double>(t), 3), square = std::pow(static_cast<double>(t), 2);
				const std::array<float, 4> weight = {
					static_cast<float>(std::pow(static_cast<double>(1.0f - t), 3) / 6.0),
					static_cast<float>((3 * cube - 6 * square + 4) / 6.0),
					static_cast<float>((-3 * cube + 3 * square + 3.0f * t + 1) / 6.0),
					static_cast<float>(cube / 6.0)};
				Vector sample{};
				for (int j = 0; j < 4; ++j) for (int axis = 0; axis < 3; ++axis)
					sample[axis] = Finite(sample[axis] + controls[end - 3 + j][axis] * weight[j]);
				path.samples.push_back(sample);
				const float next = t + granularity;
				if (!(next > t)) Fail("spline granularity does not advance");
				t = next;
			}
		}
		for (std::size_t i = 1; i < path.samples.size(); ++i) {
			const float length = Distance(path.samples[i - 1], path.samples[i]);
			path.lengths.push_back(length); path.distance = Finite(path.distance + length);
		}
	}
	Path Position(Shape shape) {
		Path path; path.shape = shape;
		Expect("{");
		bool splineSeen = false;
		for (;;) {
			const auto key = Next();
			if (key == "}") break;
			if (SameName(key, "time")) path.duration = Integer();
			else if (SameName(key, "name")) path.name = Next();
			else if (SameName(key, "baseVelocity")) path.speed = Float();
			else if (SameName(key, "type")) Integer(); // Constructor, not serialized editor type, owns dispatch.
			else if (SameName(key, "velocity")) {
				const auto offset = Integer(), duration = Integer();
				path.AddVelocity(offset, duration, Float());
			} else if ((shape == Shape::Fixed && SameName(key, "pos")) ||
				(shape == Shape::Interpolated && SameName(key, "startPos"))) path.first = Point();
			else if (shape == Shape::Interpolated && SameName(key, "endPos")) path.last = Point();
			else if (shape == Shape::Spline && SameName(key, "target")) {
				if (splineSeen) Fail("duplicate spline control list");
				splineSeen = true; Spline(path);
			} else if (key == "{" || key == "(" || key == ")" || key.empty()) Fail("unexpected camera position token");
			// Original unknown position keys do not consume their following
			// value. This recovers pos after an unquoted empty name line.
		}
		if (shape == Shape::Spline && !splineSeen) Fail("camera spline has no control list");
		if (shape == Shape::Interpolated) path.distance = Distance(path.first, path.last);
		return path;
	}
	void Parameters(Event &event) {
		if (event.type != 1 && event.type != 6 && event.type != 11 && event.type != 12) return;
		std::vector<std::string> values;
		std::size_t start = 0;
		while (start < event.parameter.size()) {
			start = event.parameter.find_first_not_of(" \t,", start);
			if (start == std::string::npos) break;
			const auto end = event.parameter.find_first_of(" \t,", start);
			values.push_back(event.parameter.substr(start, end == std::string::npos ? end : end - start));
			if (end == std::string::npos) break;
			start = end;
		}
		if (event.type == 6) {
			event.value = values.empty() ? 90 : Narrow(Number(values[0]));
			event.seconds = values.size() < 2 ? 0 : Narrow(Number(values[1]));
			if (event.value <= 0 || event.value >= 180) Fail("camera FOV must be between 0 and 180 degrees");
		} else event.seconds = values.empty() ? 0 : Narrow(Number(values[0]));
		if (event.seconds < 0) Fail("negative camera event duration");
		Milliseconds(event.seconds);
	}
	Event CameraEvent() {
		Event event; Expect("{");
		for (;;) {
			const auto key = Next(); if (key == "}") break;
			if (SameName(key, "type")) event.type = static_cast<int>(Integer());
			else if (SameName(key, "time")) event.offset = Integer();
			else if (SameName(key, "param")) event.parameter = Next();
			else Next();
		}
		if (event.type < 0 || event.type > 13) Fail("unknown camera event type");
		Parameters(event); return event;
	}
	void Fov(FieldOfView &fov) {
		Expect("{");
		for (;;) {
			const auto key = Next(); if (key == "}") break;
			if (SameName(key, "fov")) fov.value = Float();
			else if (SameName(key, "startFOV")) fov.from = Float();
			else if (SameName(key, "endFOV")) fov.to = Float();
			else if (SameName(key, "time")) Integer(); // Editor metadata, not the transition duration.
			else Next();
		}
		if (fov.value <= 0 || fov.value >= 180) Fail("camera FOV must be between 0 and 180 degrees");
	}
public:
	Parser(const char *text, std::size_t bytes) : data(text), size(bytes) {
		if (!text || !bytes || bytes > MaxFileBytes) Fail("invalid camera file size");
		if (std::memchr(text, 0, bytes)) Fail("embedded null in camera file");
	}
	void Read(Definition &definition) {
		const auto header = Next();
		if (SameName(header, "cameraPathDef")) Expect("{");
		else if (header != "{") Fail("expected cameraPathDef");
		bool positionSeen = false;
		for (;;) {
			const auto key = Next(); if (key == "}") break;
			if (SameName(key, "time")) {
				definition.seconds = Float();
				if (definition.seconds < 0) Fail("negative camera duration");
				Milliseconds(definition.seconds);
			} else if (SameName(key, "event")) {
				if (definition.events.size() == MaxEvents) Fail("too many camera events");
				definition.events.push_back(CameraEvent());
			} else if (SameName(key, "fov")) Fov(definition.fov);
			else {
				bool target = false; Shape shape;
				if (SameName(key, "camera_fixed")) shape = Shape::Fixed;
				else if (SameName(key, "camera_interpolated")) shape = Shape::Interpolated;
				else if (SameName(key, "camera_spline")) shape = Shape::Spline;
				else if (SameName(key, "target_fixed")) { target = true; shape = Shape::Fixed; }
				else if (SameName(key, "target_interpolated")) { target = true; shape = Shape::Interpolated; }
				else if (SameName(key, "target_spline")) { target = true; shape = Shape::Spline; }
				else Fail("unknown camera definition field");
				if (target) {
					if (definition.targets.size() == MaxTargets) Fail("too many camera targets");
					definition.targets.push_back(Position(shape));
				} else {
					if (positionSeen) Fail("duplicate camera position");
					positionSeen = true; definition.position = Position(shape);
				}
			}
		}
		Space(); if (cursor != size) Fail("trailing camera file content");
		// The original loader supplies a fixed zero position when omitted.
	}
};

inline void Parse(const char *text, std::size_t bytes, Definition &definition) {
	Parser(text, bytes).Read(definition);
}
}}}
#endif
