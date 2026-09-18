/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_SND_RTCW_H
#define FNTECH3_SND_RTCW_H
#include "shared/RTCWMusic.h"
#include "shared/RTCWLoops.h"
#include "shared/RTCWOneShots.h"
#include "shared/RTCWSpeech.h"
#include "shared/RTCWDefaultSound.h"

// The selected sound backend shares one process-lifetime SP music contract.
// Sound initialization/shutdown reset it; a mere stop retains the music queue.
fntech3::audio::rtcw::Music &S_RTCWMusic();
fntech3::audio::rtcw::MasterFade &S_RTCWMasterFade();
fntech3::audio::rtcw::MasterFade &S_WolfETMasterFade();
#endif
