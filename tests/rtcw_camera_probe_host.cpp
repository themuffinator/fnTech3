/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
// A disposable numerical probe. Reference sources are linked only by the
// opt-in probe script, never into fnTech3. Camera bytes arrive through a pipe.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <iterator>
#ifdef ORIGINAL_CAMERA_REFERENCE
#include "q_splineshared.h"
#include "splines.h"
#undef int8_t
static std::string input;
extern "C" {
#ifdef IORTCW_CAMERA_REFERENCE
long FS_ReadFile(const char *, void **out)
#else
int FS_ReadFile(const char *, void **out)
#endif
{ *out = std::malloc(input.size() + 1); std::memcpy(*out, input.c_str(), input.size() + 1); return static_cast<int>(input.size()); }
void FS_FreeFile(void *p) { std::free(p); }
int FS_Write(const void *, int n, fileHandle_t) { return n; }
fileHandle_t FS_FOpenFileWrite(const char *) { return 0; }
void FS_FCloseFile(fileHandle_t) {}
void Cbuf_AddText(const char *p) { std::fprintf(stderr, "EVENT %s\n", p); }
void Cbuf_Execute() {}
void *Com_Allocate(int n) { return std::malloc(n); }
void Com_Dealloc(void *p) { std::free(p); }
void Com_DPrintf(const char *, ...) {}
qboolean loadCamera(int, const char *);
void startCamera(int, int);
qboolean getCameraInfo(int, int, float *, float *, float *);
}
void QDECL Com_Error(int, const char *format, ...) {
	va_list ap; va_start(ap, format); std::vfprintf(stderr, format, ap); va_end(ap); std::exit(2);
}
void QDECL Com_Printf(const char *format, ...) {
	va_list ap; va_start(ap, format); std::vfprintf(stderr, format, ap); va_end(ap);
}
#ifndef IORTCW_CAMERA_REFERENCE
decltype(qglBegin) qglBegin = nullptr;
decltype(qglEnd) qglEnd = nullptr;
decltype(qglVertex3fv) qglVertex3fv = nullptr;
decltype(qglVertex3f) qglVertex3f = nullptr;
decltype(qglColor3fv) qglColor3fv = nullptr;
decltype(qglPointSize) qglPointSize = nullptr;
decltype(qglRasterPos3fv) qglRasterPos3fv = nullptr;
decltype(qglCallLists) qglCallLists = nullptr;
#endif
extern idCameraDef camera[64];
#if defined(IORTCW_CAMERA_REFERENCE) && defined(_MSC_VER)
// This legacy spline declaration has C++ linkage under MSVC. Its engine C
// implementation is the same bounded formatting operation.
int Q_vsnprintf(char *out, size_t bytes, const char *format, va_list args) {
	return std::vsnprintf(out, bytes, format, args);
}
#endif
#else
#include "code/compat/rtcw_camera.hpp"
#endif

int main(int argc, char **argv) {
#ifdef ORIGINAL_CAMERA_REFERENCE
	input.assign(std::istreambuf_iterator<char>(std::cin), {});
	std::printf("load %d\n", loadCamera(0, "stdin.camera"));
	for (int i = 0; i < camera[0].numTargets(); ++i)
		std::printf("target %d %s\n", i, camera[0].getActiveTarget(i)->getName());
#else
	using namespace fntech3::rtcw::camera;
	std::string input(std::istreambuf_iterator<char>(std::cin), {});
	Manager manager; Diagnostic diagnostic;
	const bool loaded = manager.Load(0, input.data(), input.size(), diagnostic,
#ifdef CANDIDATE_CAMERA_MP
		Mode::MultiPlayer
#else
		Mode::SinglePlayer
#endif
	);
	std::printf("load %d\n", loaded);
	if (!loaded) { std::fprintf(stderr, "parse %zu %s\n", diagnostic.offset, diagnostic.message); return 2; }
	const auto *definition = manager.Inspect(0);
	for (std::size_t i = 0; i < definition->targets.size(); ++i)
		std::printf("target %zu %s\n", i, definition->targets[i].name.c_str());
#endif
	// Two starts exercise the original retained-duration and FOV behavior.
	for (int pass = 0; pass < 2; ++pass) {
		const int base = pass ? 100000 : 10000;
#ifdef ORIGINAL_CAMERA_REFERENCE
		startCamera(0, base);
#else
		if (!manager.Start(0, base, diagnostic)) { std::fprintf(stderr, "start %s\n", diagnostic.message); return 3; }
#endif
		for (int i = 1; i < argc; ++i) {
			const int time = std::atoi(argv[i]);
			float origin[3] = {7, 8, 9}, angles[3] = {11, 12, 13}, fov = 91;
#ifdef ORIGINAL_CAMERA_REFERENCE
			const int ok = getCameraInfo(0, base + time, origin, angles, &fov);
#else
			Frame frame;
			const auto result = manager.Sample(0, base + time, frame, diagnostic);
			if (result == Result::Invalid) { std::fprintf(stderr, "sample %d %s\n", time, diagnostic.message); return 4; }
			const int ok = result == Result::Frame;
			if (ok) { std::memcpy(origin, frame.origin.data(), sizeof(origin)); angles[0] = frame.pitch; angles[1] = frame.yaw; fov = frame.fov; }
			for (std::size_t j = 0; j < frame.fadeCount; ++j)
				std::fprintf(stderr, "EVENT fade 0 0 0 %d %f\n", frame.fades[j].alpha, frame.fades[j].seconds);
#endif
			std::printf("sample %d %d %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",
				pass, time, ok, origin[0], origin[1], origin[2], angles[0], angles[1], angles[2], fov);
		}
	}
}
