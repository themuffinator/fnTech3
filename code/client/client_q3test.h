/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_CLIENT_Q3TEST_H
#define FNTECH3_CLIENT_Q3TEST_H

/* The Q3Test releases' client side (cl_q3test.cpp, cl_q3test_cgame.cpp).
 *
 * A Q3Test connection is its own session: each release's connectionless
 * handshake, one of Q3Test's two netchans, the release's message bodies
 * (code/qcommon/q3test_wire.hpp) and its native snapshot, gamestate and
 * usercmd records, which the retail client game reads exactly as its engine
 * built them. The engine's connection state, command queues, time base and
 * screen flow are shared with every other profile; the facts and addresses
 * are in docs/fntech3/Q3TEST.md. */

#include "../qcommon/q3test_wire.hpp"

// cl_main.cpp / cl_input.cpp / cl_cgame.cpp hooks
void CL_Q3TestCheckForResend( void );
void CL_Q3TestPacketEvent( const netadr_t *from, msg_t *msg );
void CL_Q3TestSendCmd( void );
void CL_Q3TestDisconnect( void );
void CL_Q3TestClearConnection( void );
netchanWireProfile_t CL_Q3TestWireProfile( void );
// cl_main.cpp: the gamestate is in; load the client game (CL_DownloadsComplete's first half)
void CL_Q3TestStartCGame( void );

// cl_main.cpp: demos (cl_q3test.cpp)
void CL_Q3TestPlayDemo_f( void );
bool CL_Q3TestReadDemoMessage( void );
void CL_Q3TestRecord_f( void );
void CL_Q3TestStopRecord_f( void );

// cl_main.cpp: the server browser's commands (cl_q3test.cpp)
void CL_Q3TestLocalServers_f( void );
void CL_Q3TestGlobalServers_f( void );
void CL_Q3TestPing_f( void );

// cl_q3test_cgame.cpp
vm_t *CL_Q3TestLoadCGame( void );
void CL_Q3TestInitCGameModule( void );
void CL_Q3TestShutdownCGame( void );
void CL_Q3TestCGameConfigstringModified( int index );
void CL_Q3TestCGameServerCommand( void );

// cl_q3test_ui.cpp: the release's menu module, or 1.05's engine menus
vm_t *CL_Q3TestLoadUI( void );
void CL_Q3TestShutdownUI( void );
void CL_Q3TestUIConnectCount( const char *text );
void CL_Q3TestUIServerMessage( const char *text );
void CL_Q3TestUIFrame( void );

// cl_q3test_menus.cpp: 1.05's menus
void CL_Q3TestMenuReset( void );
void CL_Q3TestMenuConnectText( const char *text, bool message );
intptr_t CL_Q3TestMenuEntry( int command, intptr_t a0, intptr_t a1 );

// services the client game bridges read
namespace fntech3::q3test::client {

int Protocol();
bool Legacy();
// The latest valid snapshot's number and time, false before the first.
bool CurrentSnapshot( int &messageNum, int &serverTime );
// A frame still in the ring, or null when it is too old or was invalid.
std::shared_ptr<const wire::Snapshot> Snapshot( int messageNum );
// Snapshot ping (retail: time since the packet whose command the server named).
int SnapshotPing( const wire::Snapshot &snap );
// The engine's configstrings as retail's gameState_t: 1024 offsets, 16000
// characters and the used count.
void WriteGameState( void *out, std::size_t bytes );
const char *Configstring( int index );
// 38-40: a stored server command, handled as CL_GetServerCommand does.
bool ServerCommand( int number );
int ServerCommandSequence();
int CurrentCmdNumber();
bool UserCmd( int number, void *out, std::size_t bytes );
void SetUserCmdValue( int weapon, float sensitivityScale );

// The server lists the menus read (cl_q3test.cpp).
int LocalServerCount();
std::string LocalServerInfo( int n );
std::string LocalServerAddress( int n );
int GlobalServerCount();
std::string GlobalServerInfo( int n );
std::string GlobalServerAddress( int n );
int PingQueueCount();
void ClearPing( int n );
void GetPing( int n, char *address, int size, int *time );
void LocalServersScan();

// Collision, records and 2D the menus share with the client game (cl_q3test_cgame.cpp).
clipHandle_t CollisionModel( const char *name );
void LerpTag( orientation_t *tag, clipHandle_t model, int startFrame, int endFrame, float frac, const char *name );
void WriteGlconfig( void *native );
void AddRefEntity( const void *native );
void RenderScene( const void *native );
void AdjustFrom640( float *x, float *y, float *w, float *h );
void FillRect( float x, float y, float w, float h, const float *color );
void DrawPic( float x, float y, float w, float h, qhandle_t shader );
void DrawBigChar( float x, float y, int ch );
void DrawBigString( int x, int y, const char *s, const float *color, bool forceColor );
int BigStringWidth( const char *s );
// Q3Test's 0x1B colour escape as this engine's.
std::string Text( const char *text );
// A module's cvar flags as this engine's, by release.
int CvarFlags( int flags );

} // namespace fntech3::q3test::client

#endif
