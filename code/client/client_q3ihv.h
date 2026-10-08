/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_Q3IHV_H
#define FNTECH3_CLIENT_Q3IHV_H

/* The February 1999 IHV Test's client side (cl_q3ihv.cpp, cl_q3ihv_cgame.cpp).
 *
 * The IHV client game is pushed its data: the engine hands it each snapshot
 * as the packet carrying it is parsed, each changed configstring and each
 * server command, and asks it once a frame whether it has something to draw,
 * reading another demo record whenever it has not. Its records are the
 * build's own (code/compat/q3_ihv_client_abi.h); the engine's connection
 * state, time base and screen flow are shared with every other profile.
 * Facts and addresses: docs/fntech3/Q3_IHV.md. */

#include "../compat/q3_ihv_client_abi.h"

// cl_main.cpp / cl_input.cpp hooks (cl_q3ihv.cpp)
void CL_Q3IHVPlayDemo_f( void );
void CL_Q3IHVRecord_f( void );
void CL_Q3IHVStopRecord_f( void );
bool CL_Q3IHVReadDemoMessage( void );
void CL_Q3IHVClearConnection( void );
netchanWireProfile_t CL_Q3IHVWireProfile( void );
void CL_Q3IHVCheckForResend( void );
void CL_Q3IHVPacketEvent( const netadr_t *from, msg_t *msg );
void CL_Q3IHVSendCmd( void );
void CL_Q3IHVDisconnect( void );
// cl_cgame.cpp: CL_SetCGameTime's work for this profile (cl_q3ihv.cpp)
void CL_Q3IHVSetCGameTime( void );
// cl_ui.cpp: the engine's own screens, there being no menu module (cl_q3ihv.cpp)
vm_t *CL_Q3IHVLoadUI( void );

// cl_q3ihv_cgame.cpp
vm_t *CL_Q3IHVLoadCGame( void );
void CL_Q3IHVInitCGameModule( void );
void CL_Q3IHVShutdownCGame( void );
bool CL_Q3IHVCGameReady( void );
void CL_Q3IHVCGameSnapshot( const fntech3_q3_ihv_snapshot_t *snapshot );
void CL_Q3IHVCGameConfigstring( int index );
void CL_Q3IHVCGameServerCommand( void );
// Export 5: whether the client game has a frame to draw at this time.
bool CL_Q3IHVCGameFrame( int realtime, bool demo );
// The weapon the client game last chose (import 61).
int CL_Q3IHVCGameWeapon( void );

namespace fntech3::q3ihv::client {
class ClientData;
// The session's records the client game's data imports read (cl_q3ihv.cpp).
const ClientData *SessionData();
}

#endif
