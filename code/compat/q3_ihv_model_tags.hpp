/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_MODEL_TAGS_HPP
#define FNTECH3_Q3_IHV_MODEL_TAGS_HPP

#include "q3_ihv_client_abi.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace fntech3::q3ihv {

static_assert(sizeof(fntech3_q3_ihv_tag_t) == 112);
static_assert(offsetof(fntech3_q3_ihv_tag_t, origin) == 64);
static_assert(offsetof(fntech3_q3_ihv_tag_t, axis) == 76);
static_assert(sizeof(fntech3_q3_ihv_model_frame_t) == 56);
static_assert(sizeof(fntech3_q3_ihv_collision_model_t) == 112);
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, name) == 28);
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, isMD3) == 92);
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, frames) == 104);
static_assert(offsetof(fntech3_q3_ihv_collision_model_t, tags) == 108);

/* Engine-owned MD3 animation metadata for the IHV collision-model/tag API.
 * Meshes still belong to the renderer. Native modules receive the separate
 * NativeModelMetadata record below, never a pointer to this C++ object.
 * Observations: original CM_GetTag clamps high frames, compares names exactly,
 * and CM_LerpTag blends then normalizes each axis independently. Negative
 * frames/nonfinite data are refused instead of reproducing unsafe reads. */
class ModelTags {
public:
	bool Load(const void *input, std::size_t size) {
		Clear();
		if (!input || size < 108) return false;
		const auto *data = static_cast<const std::uint8_t *>(input);
		if (Word(data) != 0x33504449 || Word(data + 4) != 15) return false;
		const auto frames = Word(data + 76), tags = Word(data + 80);
		const auto frameOffset = Word(data + 92);
		auto tagOffset = Word(data + 96);
		const auto end = Word(data + 104);
		if (!frames || frames > 0x7fffffffu || tags > 0x7fffffffu || end < 108 || end > size
			|| frameOffset < 108 || frameOffset > end || frames > (end - frameOffset) / 56
			|| tagOffset < 108 || tagOffset > end) return false;
		const std::uint64_t count = std::uint64_t(frames) * tags;
		if (count > (end - tagOffset) / 112) return false;
		// Reject overlapping frame/tag tables. Zero-tag models are valid.
		if (count && tagOffset < std::uint64_t(frameOffset) + std::uint64_t(frames) * 56
			&& frameOffset < std::uint64_t(tagOffset) + count * 112) return false;
		std::vector<fntech3_q3_ihv_model_frame_t> parsedFrames(frames);
		for (std::size_t n = 0; n < frames; ++n) {
			const auto *record = data + frameOffset + n * 56;
			auto &frame = parsedFrames[n];
			for (unsigned i = 0; i < 10; ++i) {
				const auto bits = Word(record + i * 4);
				if ((bits & 0x7f800000u) == 0x7f800000u) return false;
				float value; std::memcpy(&value, &bits, 4);
				if (i < 3) frame.mins[i] = value;
				else if (i < 6) frame.maxs[i - 3] = value;
				else if (i < 9) frame.localOrigin[i - 6] = value;
				else frame.radius = value;
			}
			std::memcpy(frame.name, record + 40, 16);
		}
		std::vector<fntech3_q3_ihv_tag_t> parsed(static_cast<std::size_t>(count));
		for (auto &tag : parsed) {
			const auto *record = data + tagOffset;
			if (!std::memchr(record, 0, 64)) return false;
			std::memcpy(tag.name, record, 64);
			for (unsigned i = 0; i < 12; ++i) {
				const auto bits = Word(record + 64 + i * 4);
				if ((bits & 0x7f800000u) == 0x7f800000u) return false;
				float value; std::memcpy(&value, &bits, 4);
				if (i < 3) tag.origin[i] = value;
				else tag.axis[(i - 3) / 3][(i - 3) % 3] = value;
			}
			tagOffset += 112;
		}
		frameData_ = std::move(parsedFrames); tags_ = std::move(parsed); frames_ = frames; perFrame_ = tags;
		return true;
	}
	void Clear() { frameData_.clear(); tags_.clear(); frames_ = perFrame_ = 0; }
	std::uint32_t Frames() const { return frames_; }
	std::uint32_t TagsPerFrame() const { return perFrame_; }
	const fntech3_q3_ihv_model_frame_t *FrameData() const { return frameData_.empty() ? nullptr : frameData_.data(); }
	const fntech3_q3_ihv_tag_t *TagData() const { return tags_.empty() ? nullptr : tags_.data(); }
	const fntech3_q3_ihv_tag_t *Find(int frame, std::string_view name) const {
		if (!frames_ || frame < 0) return nullptr;
		const auto selected = static_cast<std::uint32_t>(frame) >= frames_ ? frames_ - 1 : static_cast<std::uint32_t>(frame);
		const auto first = std::size_t(selected) * perFrame_;
		for (std::size_t i = 0; i < perFrame_; ++i)
			if (name == tags_[first + i].name) return &tags_[first + i];
		return nullptr;
	}
	bool Lerp(int startFrame, int endFrame, const float &fraction, std::string_view name,
		fntech3_q3_ihv_tag_t &output) const {
		// Read stored bits before floating arithmetic; a by-value parameter can
		// acquire a no-NaN assumption under Clang's finite-math optimization.
		std::uint32_t fractionBits; std::memcpy(&fractionBits, &fraction, 4);
		if ((fractionBits & 0x7f800000u) == 0x7f800000u) return false;
		const auto *start = Find(startFrame, name), *end = Find(endFrame, name);
		if (!start || !end) return false;
		fntech3_q3_ihv_tag_t next{};
		std::memcpy(next.name, output.name, 64); // Preserve only the name; old pose may be uninitialized.
		const double weight = fraction;
		for (unsigned i = 0; i < 3; ++i) {
			if (!Blend(start->origin[i], end->origin[i], weight, next.origin[i])) return false;
			for (unsigned j = 0; j < 3; ++j)
				if (!Blend(start->axis[i][j], end->axis[i][j], weight, next.axis[i][j])) return false;
		}
		for (auto &axis : next.axis) {
			const double x = axis[0], y = axis[1], z = axis[2];
			const double length = std::sqrt(x * x + y * y + z * z);
			if (length != 0.0) for (auto &component : axis) component = static_cast<float>(component * (1.0 / length));
		}
		// Finite inputs can still overflow during extrapolation; publish nothing.
		for (unsigned i = 0; i < 3; ++i) {
			if (!Finite(next.origin[i])) return false;
			for (float component : next.axis[i]) if (!Finite(component)) return false;
		}
		output = next; return true;
	}
private:
	static bool Blend(float start, float end, double weight, float &result) {
		const double value = (1.0 - weight) * start + weight * end;
		const double limit = (std::numeric_limits<float>::max)();
		if (value < -limit || value > limit) return false;
		result = static_cast<float>(value); return true;
	}
	static std::uint32_t Word(const std::uint8_t *p) {
		return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
	}
	static bool Finite(const float &value) {
		std::uint32_t bits; std::memcpy(&bits, &value, 4); return (bits & 0x7f800000u) != 0x7f800000u;
	}
	std::vector<fntech3_q3_ihv_tag_t> tags_;
	std::vector<fntech3_q3_ihv_model_frame_t> frameData_;
	std::uint32_t frames_ = 0, perFrame_ = 0;
};

/* Stable x86 mirror of the original CM_RegisterModel result. A native module
 * may follow frames/tags, so an opaque pointer to a C++ metadata object is not
 * a compatible registration result. Copy/move would invalidate its identity. */
class NativeModelMetadata {
public:
	NativeModelMetadata() = default;
	NativeModelMetadata(const NativeModelMetadata &) = delete;
	NativeModelMetadata &operator=(const NativeModelMetadata &) = delete;
	bool Load(std::string_view name, const void *data, std::size_t size) {
		if (sizeof(void *) != 4 || name.empty() || name.size() >= sizeof(record_.name)
			|| name.find('\0') != std::string_view::npos) { Clear(); return false; }
		char savedName[64]{}; std::memcpy(savedName, name.data(), name.size());
		Clear();
		if (!metadata_.Load(data, size)) return false;
		std::memcpy(record_.name, savedName, sizeof(savedName));
		record_.isMD3 = 1;
		record_.frameCount = static_cast<std::int32_t>(metadata_.Frames());
		record_.tagCount = static_cast<std::int32_t>(metadata_.TagsPerFrame());
		record_.frames = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(metadata_.FrameData()));
		record_.tags = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(metadata_.TagData()));
		return true;
	}
	void Clear() { record_ = {}; metadata_.Clear(); }
	const ModelTags &Metadata() const { return metadata_; }
	const fntech3_q3_ihv_collision_model_t *Record() const { return record_.isMD3 ? &record_ : nullptr; }
private:
	fntech3_q3_ihv_collision_model_t record_{};
	ModelTags metadata_;
};

} // namespace fntech3::q3ihv
#endif
