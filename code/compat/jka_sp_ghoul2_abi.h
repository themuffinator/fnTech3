/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_COMPAT_JKA_SP_GHOUL2_ABI_H
#define FNTECH3_COMPAT_JKA_SP_GHOUL2_ABI_H
#include <stddef.h>
#include <stdint.h>

/* Retail 1.01 facts, independently checked in jasp.exe and jagamex86.dll.
 * OpenJK ghoul2_shared.h at 1a6a6434 names the corresponding fields.
 * These are external records, never std::vector or the engine's MP handles.
 * Retail vector construction at 0x4d0f00 writes +4/+8/+12; array creation
 * uses stride 16. +0 belongs to the old allocator object/padding, and must
 * not be interpreted as an element pointer. Model copies use stride 204
 * at 0x4d0f6a and copy its individual fields at 0x4d11a0..0x4d12ce. */
enum {
	FNTECH3_JKA_SP_GHOUL2_ARRAY_SLOTS = 5,
	FNTECH3_JKA_SP_GHOUL2_HANDLE_SLOTS = 512,
	FNTECH3_JKA_SP_GHOUL2_INFO_BYTES = 204
};
typedef struct {
	uint32_t allocator_reserved, begin_address, end_address, capacity_address;
} fntech3_jka_sp_ghoul2_vector_t;
typedef struct { int32_t handle; } fntech3_jka_sp_ghoul2_handle_t;
typedef struct {
	fntech3_jka_sp_ghoul2_vector_t surfaces, bolts, bones;
	int32_t modelindex, animModelIndexOffset, customShader, customSkin;
	int32_t modelBoltLink, surfaceRoot, lodBias, newOrigin, goreSetTag, model;
	char fileName[64];
	int32_t animFrameDefault, skelFrameNum, meshFrameNum, flags;
	uint32_t transformedVertsArray_address, boneCache_address;
	int32_t skin;
	uint8_t valid, valid_padding[3];
	uint32_t currentModel_address;
	int32_t currentModelSize;
	uint32_t animModel_address;
	int32_t currentAnimModelSize;
	uint32_t animHeader_address;
} fntech3_jka_sp_ghoul2_info_t;
/* Retail import46 at jasp0x4ce910; seven x86 __cdecl words, no modern MP
 * instance pointer or boolean/out-pointer result. The third and sixth words
 * are ignored by the retail initializer. */
typedef int32_t (*fntech3_jka_sp_G2API_InitGhoul2Model_t)(fntech3_jka_sp_ghoul2_handle_t *,
	const char *,int32_t,int32_t,int32_t,int32_t,int32_t);

#ifdef __cplusplus
static_assert(sizeof(fntech3_jka_sp_ghoul2_vector_t) == 16);
static_assert(offsetof(fntech3_jka_sp_ghoul2_vector_t, begin_address) == 4);
static_assert(offsetof(fntech3_jka_sp_ghoul2_vector_t, end_address) == 8);
static_assert(offsetof(fntech3_jka_sp_ghoul2_vector_t, capacity_address) == 12);
static_assert(sizeof(fntech3_jka_sp_ghoul2_handle_t) == 4);
static_assert(sizeof(fntech3_jka_sp_ghoul2_info_t) == FNTECH3_JKA_SP_GHOUL2_INFO_BYTES);
static_assert(offsetof(fntech3_jka_sp_ghoul2_info_t, modelindex) == 48);
static_assert(offsetof(fntech3_jka_sp_ghoul2_info_t, fileName) == 88);
static_assert(offsetof(fntech3_jka_sp_ghoul2_info_t, boneCache_address) == 172);
static_assert(offsetof(fntech3_jka_sp_ghoul2_info_t, valid) == 180);
static_assert(offsetof(fntech3_jka_sp_ghoul2_info_t, currentModel_address) == 184);

/* The getter is __cdecl; its five methods are MSVC x86 __thiscall.
 * ECX carries this, one-argument methods end in ret 4, and IsValid returns
 * a one-byte bool in AL. There is no virtual destructor slot. The retail
 * vtable at 0x4f142c is followed immediately by string data, not slot six.
 * MSVC reverses the two overloaded Get declarations: module resize at
 * 0x201327e3 calls New, then uses Get at +16 (0x20132806) and mutates that
 * vector at 0x2013280b. Const Get is therefore slot 3, mutable Get slot 4.
 * An explicit table avoids exposing the host compiler's class/STL ABI. */
#if defined(_MSC_VER)
#define FNTECH3_JKA_SP_THISCALL __thiscall
#elif defined(__i386__) && defined(__GNUC__)
#define FNTECH3_JKA_SP_THISCALL __attribute__((thiscall))
#else
#define FNTECH3_JKA_SP_THISCALL
#endif
struct fntech3_jka_sp_ghoul2_array_vtable_t {
	int32_t (FNTECH3_JKA_SP_THISCALL *New)(void *);
	void (FNTECH3_JKA_SP_THISCALL *Delete)(void *, int32_t);
	bool (FNTECH3_JKA_SP_THISCALL *IsValid)(const void *, int32_t);
	const fntech3_jka_sp_ghoul2_vector_t *(FNTECH3_JKA_SP_THISCALL *GetConst)(const void *, int32_t);
	fntech3_jka_sp_ghoul2_vector_t *(FNTECH3_JKA_SP_THISCALL *Get)(void *, int32_t);
};
struct fntech3_jka_sp_ghoul2_array_t {
	const fntech3_jka_sp_ghoul2_array_vtable_t *vtable;
};
#if UINTPTR_MAX == UINT32_MAX
static_assert(sizeof(fntech3_jka_sp_ghoul2_array_vtable_t) == 20);
static_assert(sizeof(fntech3_jka_sp_ghoul2_array_t) == 4);
#endif
#endif
#endif
