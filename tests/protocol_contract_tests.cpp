#include "protocol_contract.hpp"

using namespace fnql::protocol;

static_assert( Find( 91 ) == &QuakeLive );
static_assert( Find( 68 ) == &LegacyQuake3 );
static_assert( Find( 71 ) == &Ioquake3 );
static_assert( Find( 49 ) == &RtcwSinglePlayer );
static_assert( Find( 60 ) == &RtcwMultiplayer );
static_assert( Find( 49, true ) == &RtcwSinglePlayer );
static_assert( Find( 60, true ) == &RtcwMultiplayer );
static_assert( Find( 50 ) == nullptr && Find( 61 ) == nullptr );
static_assert( TryForWireProfile( NETCHAN_WIRE_RTCW_SP ) == &RtcwSinglePlayer );
static_assert( TryForWireProfile( NETCHAN_WIRE_RTCW_MP ) == &RtcwMultiplayer );
static_assert( RtcwSinglePlayer.gameMessageLayout == GameMessageLayout::RtcwSinglePlayer );
static_assert( RtcwMultiplayer.gameMessageLayout == GameMessageLayout::RtcwMultiplayer );
static_assert( !RtcwSinglePlayer.Has( Capability::ReliableXor ) );
static_assert( !RtcwSinglePlayer.Has( Capability::CompressedConnect ) );
static_assert( RtcwMultiplayer.Has( Capability::CompressedConnect ) );
static_assert( RtcwMultiplayer.Has( Capability::ReliableXor ) );
static_assert( !RtcwSinglePlayer.Has( Capability::SequenceChecksum ) );
static_assert( !RtcwMultiplayer.Has( Capability::SequenceChecksum ) );
static_assert( RtcwLimits.reliableCommands == 256 && RtcwLimits.messageBytes == 32768 );
static_assert( RtcwLimits.downloadWindow == 8 && RtcwLimits.downloadBlockBytes == 2048 );
static_assert( Find( 999 ) == nullptr );
static_assert( Find( 71, true ) == &Ioquake3 );
static_assert( Find( 999, true ) == &LegacyQuake3 );
static_assert( &ForWireProfile( NETCHAN_WIRE_QL_RETAIL ) == &QuakeLive );
static_assert( &ForWireProfile( NETCHAN_WIRE_IOQ3 ) == &Ioquake3 );
static_assert( TryForWireProfile( NETCHAN_WIRE_LEGACY_Q3 ) == &LegacyQuake3 );
static_assert( TryForWireProfile( static_cast<netchanWireProfile_t>( 99 ) ) == nullptr );

static_assert( QuakeLive.wireProfile == NETCHAN_WIRE_QL_RETAIL );
static_assert( QuakeLive.gameMessageLayout == GameMessageLayout::QuakeLive );
static_assert( QuakeLive.Has( Capability::ReliableXor ) );
static_assert( !QuakeLive.Has( Capability::SequenceChecksum ) );
static_assert( QuakeLive.Has( Capability::PlatformAuthentication ) );
static_assert( QuakeLive.Has( Capability::WorkshopContent ) );
static_assert( QuakeLive.Has( Capability::RetailClientsMayJoin ) );
static_assert( QuakeLive.limits.downloadWindow == 8 );
static_assert( QuakeLive.limits.downloadBlockBytes == 2048 );

static_assert( !LegacyQuake3.Has( Capability::WorkshopContent ) );
static_assert( LegacyQuake3.gameMessageLayout == GameMessageLayout::Quake3 );
static_assert( Ioquake3.Has( Capability::SequenceChecksum ) );
static_assert( Ioquake3.gameMessageLayout == GameMessageLayout::Quake3 );
static_assert( !Ioquake3.Has( Capability::ReliableXor ) );
static_assert( !Ioquake3.Has( Capability::WorkshopContent ) );
static_assert( ModernizedQ3Limits.downloadWindow == 48 );
static_assert( ModernizedQ3Limits.downloadBlockBytes == 1024 );

static_assert( QuakeLive.commands.getChallenge == "getchallenge" );
static_assert( QuakeLive.commands.pureChecksums == "cp" );
static_assert( QuakeLive.infoKeys.serverId == "sv_serverid" );

int main() {
	return 0;
}
