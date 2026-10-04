/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RAVEN_FONT_FORMAT_HPP
#define FNTECH3_RAVEN_FONT_FORMAT_HPP
#include "jk2_font_format.hpp"

/* The independent fnTech3 reader serves the shared Raven glyph schema.
 * Retail JKA assets1.pk3 contains seven records of the same 7180-byte
 * shape as JK2. Keep one bounded implementation for both titles. */
namespace fntech3::raven { namespace font = fntech3::jk2::font; }
#endif
