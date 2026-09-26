#include "protocol_contract.hpp"
#include "protocol_contract_profile.hpp"
#include "q3_demo_protocol.hpp"
#include "q3_pak_contract.h"
#include <cassert>
#include <string>
#include <vector>

using namespace fnql::protocol;

static_assert( Find( 91 ) == &QuakeLive );
static_assert( Find( 68 ) == &LegacyQuake3 );
static_assert( Find( 66 ) == &Quake3_66 && Find( 67 ) == &Quake3_67 );
static_assert( Find( 66, true ) == &Quake3_66 && Find( 67, true ) == &Quake3_67 );
static_assert( TryForWireProfile( NETCHAN_WIRE_Q3_66 ) == &Quake3_66 );
static_assert( TryForWireProfile( NETCHAN_WIRE_Q3_67 ) == &Quake3_67 );
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
static_assert( Find( 24 ) == &STVEFHolomatch && Find( 24, true ) == &STVEFHolomatch );
static_assert( Find( 40 ) == &STVEFSinglePlayer );
static_assert( Find( 26 ) == &IoEF && Find( 26, true ) == &IoEF );
static_assert( TryForWireProfile( NETCHAN_WIRE_IOEF ) == &IoEF );
static_assert( IoEF.gameMessageLayout == GameMessageLayout::IoEF );
static_assert( IoEF.Has( Capability::SequenceChecksum ) && IoEF.Has( Capability::ClientQport ) );
static_assert( !IoEF.Has( Capability::PacketScramble ) && !IoEF.Has( Capability::RawMessages ) &&
	!IoEF.Has( Capability::ReliableXor ) && !IoEF.Has( Capability::CompressedConnect ) );
static_assert( IoEF.limits.downloadWindow == 48 && IoEF.limits.downloadBlockBytes == 1024 &&
	IoEF.limits.messageBytes == 16384 && IoEF.limits.reliableCommands == 64 );
static_assert( TryForWireProfile( NETCHAN_WIRE_STVEF_HM ) == &STVEFHolomatch );
static_assert( TryForWireProfile( NETCHAN_WIRE_STVEF_SP ) == &STVEFSinglePlayer );
static_assert( STVEFHolomatch.gameMessageLayout == GameMessageLayout::STVEFHolomatch );
static_assert( STVEFHolomatch.Has( Capability::PacketScramble ) &&
	STVEFHolomatch.Has( Capability::RawMessages ) );
static_assert( !STVEFHolomatch.Has( Capability::ReliableXor ) &&
	!STVEFHolomatch.Has( Capability::SequenceChecksum ) &&
	!STVEFHolomatch.Has( Capability::CompressedConnect ) );
static_assert( STVEFHolomatch.limits.downloadWindow == 8 &&
	STVEFHolomatch.limits.downloadBlockBytes == 2048 );
static_assert( STVEFHolomatch.limits.reliableCommands == 64 &&
	STVEFHolomatch.limits.messageBytes == 16384 );
static_assert( !STVEFSinglePlayer.Has( Capability::PacketScramble ) );

// American McGee's Alice. Protocol 1 was unclaimed, so it resolves the same
// way with and without the legacy-compatibility fallback.
static_assert( Find( 1 ) == &Alice && Find( 1, true ) == &Alice );
static_assert( TryForWireProfile( NETCHAN_WIRE_ALICE ) == &Alice );
static_assert( Alice.protocol == 1 && Alice.demoProtocol == 1 );
static_assert( Alice.gameMessageLayout == GameMessageLayout::Alice );
static_assert( Alice.Has( Capability::PacketScramble ) &&
	Alice.Has( Capability::RawMessages ) &&
	Alice.Has( Capability::ClientQport ) );
static_assert( !Alice.Has( Capability::ReliableXor ) &&
	!Alice.Has( Capability::SequenceChecksum ) &&
	!Alice.Has( Capability::CompressedConnect ) &&
	!Alice.Has( Capability::RetailClientsMayJoin ) );
static_assert( Alice.capabilities ==
	( STVEFHolomatch.capabilities & ~Bit( Capability::RetailClientsMayJoin ) ) );
static_assert( Alice.limits.messageBytes == 49152 &&
	Alice.limits.reliableCommands == 128 );
static_assert( Alice.limits.packetBytes == 1400 &&
	Alice.limits.packetBackup == 32 );
// No download path exists in the engine to size: its client dispatches
// svc_download to the illegible-message arm.
static_assert( Alice.limits.downloadWindow == 0 &&
	Alice.limits.downloadBlockBytes == 0 );
static_assert( Alice.commands.download.empty() &&
	Alice.commands.nextDownload.empty() &&
	Alice.commands.stopDownload.empty() &&
	Alice.commands.doneDownload.empty() );
// Its server answers two client commands and neither is a pure exchange.
static_assert( Alice.commands.pureChecksums.empty() &&
	Alice.commands.pureReset.empty() );
static_assert( Alice.commands.userinfo == "userinfo" &&
	Alice.commands.disconnect == "disconnect" );
static_assert( Alice.commands.getChallenge == "getchallenge" &&
	Alice.commands.connectResponse == "connectResponse" &&
	Alice.commands.statusResponse == "statusResponse" );
// A systeminfo that predates the pak-name triplet.
static_assert( Alice.infoKeys.paks == "sv_paks" &&
	Alice.infoKeys.serverId == "sv_serverid" );
static_assert( Alice.infoKeys.pakNames.empty() &&
	Alice.infoKeys.referencedPaks.empty() &&
	Alice.infoKeys.referencedPakNames.empty() );
static_assert( Find( 999 ) == nullptr );
static_assert( Find( 71, true ) == &Ioquake3 );
static_assert( Find( 999, true ) == &LegacyQuake3 );
static_assert( &ForWireProfile( NETCHAN_WIRE_QL_RETAIL ) == &QuakeLive );
static_assert( &ForWireProfile( NETCHAN_WIRE_IOQ3 ) == &Ioquake3 );
static_assert( TryForWireProfile( NETCHAN_WIRE_LEGACY_Q3 ) == &LegacyQuake3 );
static_assert( TryForWireProfile( static_cast<netchanWireProfile_t>( 99 ) ) == nullptr );

// A number several games speak finds one game's contract, which serves only
// that game's profiles: 26 is ioEF's, not Jedi Academy's; 40 Elite Force
// single player's, not the Jedi titles' or Q3Test 1.08's; and the legacy
// fallback that takes 15 and 33 is Quake III's, not Jedi Outcast's or IHV's.
static_assert( ServesProfileFamilies( *Find( 26 ), COM_PROTOCOL_FAMILY_IOEF ) );
static_assert( !ServesProfileFamilies( *Find( 26, true ), COM_PROTOCOL_FAMILY_JKA_MP ) );
static_assert( ServesProfileFamilies( *Find( 40 ), COM_PROTOCOL_FAMILY_STVEF_SP ) );
static_assert( !ServesProfileFamilies( *Find( 40, true ), COM_PROTOCOL_FAMILY_JKA_SP
	| COM_PROTOCOL_FAMILY_JK2_SP | COM_PROTOCOL_FAMILY_Q3TEST ) );
static_assert( !ServesProfileFamilies( *Find( 15, true ), COM_PROTOCOL_FAMILY_JK2_MP ) );
static_assert( !ServesProfileFamilies( *Find( 33, true ), COM_PROTOCOL_FAMILY_QUAKE3_IHV ) );
static_assert( ServesProfileFamilies( *Find( 24 ), COM_PROTOCOL_FAMILY_STVEF_HM )
	&& !ServesProfileFamilies( *Find( 24 ), COM_PROTOCOL_FAMILY_IOEF ) );
static_assert( ServesProfileFamilies( *Find( 66 ), COM_PROTOCOL_FAMILY_Q3_POINT_RELEASES )
	&& ServesProfileFamilies( *Find( 68 ), COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 )
	&& ServesProfileFamilies( *Find( 71 ), COM_PROTOCOL_FAMILY_IOQUAKE3 )
	&& ServesProfileFamilies( *Find( 91 ), COM_PROTOCOL_FAMILY_QUAKELIVE ) );
static_assert( !ServesProfileFamilies( *Find( 68 ), COM_PROTOCOL_FAMILY_QUAKELIVE
	| COM_PROTOCOL_FAMILY_RTCW_MP | COM_PROTOCOL_FAMILY_WOLFET ) );
static_assert( ServesProfileFamilies( *Find( 49 ), COM_PROTOCOL_FAMILY_RTCW_SP )
	&& ServesProfileFamilies( *Find( 60 ), COM_PROTOCOL_FAMILY_RTCW_MP )
	&& ServesProfileFamilies( *Find( 84 ), COM_PROTOCOL_FAMILY_WOLFET )
	&& ServesProfileFamilies( *Find( 6 ), COM_PROTOCOL_FAMILY_MOHAA ) );
// F.A.K.K.2 crosses Alice's wire with its own records.
static_assert( ServesProfileFamilies( *Find( 1 ), COM_PROTOCOL_FAMILY_ALICE )
	&& ServesProfileFamilies( *Find( 1 ), COM_PROTOCOL_FAMILY_FAKK2 ) );

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
	using namespace fntech3::q3;
	assert( Com_Q3RequiredPakMask( 66 ) == 0x7f );
	assert( Com_Q3RequiredPakMask( 67 ) == 0xff );
	assert( Com_Q3RequiredPakMask( 68 ) == 0x1ff );
	assert( Com_Q3RequiredPakMask( 71 ) == 0x1ff );
	for ( const unsigned checksum : { 1913835400u, 1324612836u } ) {
		assert( Com_Q3PointPakChecksum( 66, 6, checksum ) );
		for ( int protocol : { 43, 45, 46, 48, 67, 68, 71, 91 } )
			assert( !Com_Q3PointPakChecksum( protocol, 6, checksum ) );
		for ( int pak : { -1, 0, 1, 2, 3, 4, 5, 7, 8, 9 } )
			assert( !Com_Q3PointPakChecksum( 66, pak, checksum ) );
	}
	assert( !Com_Q3PointPakChecksum( 66, 6, 0 ) );
	assert( !Com_Q3PointPakChecksum( 66, 6, 1913835401u ) );
	for ( int protocol : { 43, 45, 46, 48 } ) {
		std::vector<std::uint8_t> message( protocol >= 46 ? 4 : 0, 0 );
		const std::uint8_t start[] = { 2, 1, 0, 0, 0, 3, 0, 0 };
		message.insert( message.end(), std::begin( start ), std::end( start ) );
		const auto info = std::string( "\\hostname\\Q3 fixture\\protocol\\" ) + std::to_string( protocol );
		message.insert( message.end(), info.begin(), info.end() );
		message.push_back( 0 );
		assert( DetectRawDemoProtocol( message.data(), message.size() ) == protocol );
		if ( protocol >= 46 ) {
			// Ack 2 and command sequence 3 also resemble the older header.
			// A failed candidate must not hide the real acknowledged format.
			message[0] = 2;
			message[5] = 3;
			assert( DetectRawDemoProtocol( message.data(), message.size() ) == protocol );
		}
		for ( std::size_t cut = 0; cut < message.size(); ++cut )
			assert( DetectRawDemoProtocol( message.data(), cut ) == 0 );
		// A protocol-looking substring outside the serverinfo key is not evidence.
		message[ ( protocol >= 46 ? 4 : 0 ) + 6 ] = 1;
		assert( DetectRawDemoProtocol( message.data(), message.size() ) == 0 );
	}
	for ( const char *info : { "\\hostname\\protocol\\43", "\\xprotocol\\43", "\\protocol\\431",
		"\\protocol\\40", "\\protocol\\-1", "\\protocol\\43junk", "\\protocol\\43\\protocol\\45",
		"\\protocol\\46", "\\protocol\\43\\dangling" } ) {
		std::vector<std::uint8_t> message{ 2, 0, 0, 0, 0, 3, 0, 0 };
		const std::string value( info );
		message.insert( message.end(), value.begin(), value.end() );
		message.push_back( 0 );
		assert( DetectRawDemoProtocol( message.data(), message.size() ) == 0 );
	}
	assert( DetectRawDemoProtocol( nullptr, 100 ) == 0 );
	assert( RawDemoContainer( 48, 46 ) && RawDemoContainer( 0, 45 ) );
	assert( !RawDemoContainer( 46, 48 ) && !RawDemoContainer( 48, 43 ) );
	assert( !RawDemoContainer( 0, 40 ) && !RawDemoContainer( 0, 47 ) );
	return 0;
}
