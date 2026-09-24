#include "netchan_profile.h"
#include "net_address.hpp"

static_assert( fnql::net::ParsePackedIpv4Host( "2825167223" ).valid );
static_assert( fnql::net::ParsePackedIpv4Host( "2825167223" ).value == 0xa864a177u );
static_assert( fnql::net::ParsePackedIpv4Host( "0" ).valid );
static_assert( fnql::net::ParsePackedIpv4Host( "4294967295" ).value == 0xffffffffu );
static_assert( !fnql::net::ParsePackedIpv4Host( "4294967296" ).valid );
static_assert( !fnql::net::ParsePackedIpv4Host( "168.100.161.119" ).valid );
static_assert( !fnql::net::ParsePackedIpv4Host( "-1" ).valid );
static_assert( !fnql::net::ParsePackedIpv4Host( "" ).valid );

static_assert( Netchan_SelectWireProfile( 68, 1 ) == NETCHAN_WIRE_LEGACY_Q3 );
static_assert( Netchan_SelectWireProfile( 66, 0 ) == NETCHAN_WIRE_Q3_66 );
static_assert( Netchan_SelectWireProfile( 67, 1 ) == NETCHAN_WIRE_Q3_67 );
static_assert( Netchan_WireFeatures( NETCHAN_WIRE_Q3_66 ) == Netchan_WireFeatures( NETCHAN_WIRE_LEGACY_Q3 ) );
static_assert( Netchan_WireFeatures( NETCHAN_WIRE_Q3_67 ) == Netchan_WireFeatures( NETCHAN_WIRE_LEGACY_Q3 ) );
static_assert( Netchan_MessageLimit( NETCHAN_WIRE_Q3_66 ) == 16384 );
static_assert( Netchan_ReliableCommandLimit( NETCHAN_WIRE_Q3_67 ) == 64 );
// Local Q3 startup skips the challenge exchange and initially has compat=0.
static_assert( Netchan_SelectWireProfile( 68, 0 ) == NETCHAN_WIRE_LEGACY_Q3 );
static_assert( Netchan_SelectWireProfile( 71, 0 ) == NETCHAN_WIRE_IOQ3 );
static_assert( Netchan_SelectWireProfile( 71, 1 ) == NETCHAN_WIRE_LEGACY_Q3 );
static_assert( Netchan_SelectWireProfile( 49, 0 ) == NETCHAN_WIRE_RTCW_SP );
static_assert( Netchan_SelectWireProfile( 49, 1 ) == NETCHAN_WIRE_RTCW_SP );
static_assert( Netchan_SelectWireProfile( 60, 0 ) == NETCHAN_WIRE_RTCW_MP );
static_assert( Netchan_SelectWireProfile( 60, 1 ) == NETCHAN_WIRE_RTCW_MP );
static_assert( Netchan_WireFeatures( NETCHAN_WIRE_RTCW_SP ) == NETCHAN_FEATURE_CLIENT_QPORT );
static_assert( Netchan_WireFeatures( NETCHAN_WIRE_RTCW_MP ) ==
	( NETCHAN_FEATURE_CLIENT_QPORT | NETCHAN_FEATURE_RELIABLE_XOR ) );
// Elite Force: Holomatch scrambles packets and sends raw bits with neither
// checksum nor XOR; single player stays on loopback with only the qport.
static_assert( Netchan_SelectWireProfile( 24, 0 ) == NETCHAN_WIRE_STVEF_HM );
static_assert( Netchan_SelectWireProfile( 24, 1 ) == NETCHAN_WIRE_STVEF_HM );
static_assert( Netchan_SelectWireProfile( 40, 1 ) == NETCHAN_WIRE_STVEF_SP );
static_assert( Netchan_WireFeatures( NETCHAN_WIRE_STVEF_HM ) == ( NETCHAN_FEATURE_CLIENT_QPORT |
	NETCHAN_FEATURE_PACKET_SCRAMBLE | NETCHAN_FEATURE_RAW_MESSAGES ) );
static_assert( Netchan_WireFeatures( NETCHAN_WIRE_STVEF_SP ) == NETCHAN_FEATURE_CLIENT_QPORT );
static_assert( Netchan_MessageLimit( NETCHAN_WIRE_STVEF_HM ) == 16384 );
static_assert( Netchan_ReliableCommandLimit( NETCHAN_WIRE_STVEF_HM ) == 64 );
// American McGee's Alice: Holomatch's framing choices on Ritual's engine,
// with its own message and reliable-command limits. Both compatibility
// settings resolve it, because protocol 1 is nobody else's.
static_assert( Netchan_SelectWireProfile( 1, 0 ) == NETCHAN_WIRE_ALICE );
static_assert( Netchan_SelectWireProfile( 1, 1 ) == NETCHAN_WIRE_ALICE );
static_assert( Netchan_SelectWireProfile( NETCHAN_ALICE_PROTOCOL_VERSION, 0 ) ==
	NETCHAN_WIRE_ALICE );
static_assert( Netchan_WireFeatures( NETCHAN_WIRE_ALICE ) ==
	Netchan_WireFeatures( NETCHAN_WIRE_STVEF_HM ) );
static_assert( !Netchan_WireHasFeature( NETCHAN_WIRE_ALICE,
	NETCHAN_FEATURE_RELIABLE_XOR ) );
static_assert( !Netchan_WireHasFeature( NETCHAN_WIRE_ALICE,
	NETCHAN_FEATURE_SEQUENCE_CHECKSUM ) );
static_assert( Netchan_MessageLimit( NETCHAN_WIRE_ALICE ) == 49152 );
static_assert( Netchan_ReliableCommandLimit( NETCHAN_WIRE_ALICE ) == 128 );
static_assert( Netchan_SelectWireProfile( NETCHAN_QL_RETAIL_PROTOCOL_VERSION, 0 ) ==
	NETCHAN_WIRE_QL_RETAIL );
static_assert( Netchan_SelectWireProfile( NETCHAN_QL_RETAIL_PROTOCOL_VERSION, 1 ) ==
	NETCHAN_WIRE_QL_RETAIL );

static_assert( Netchan_WireHasFeature( NETCHAN_WIRE_LEGACY_Q3,
	NETCHAN_FEATURE_CLIENT_QPORT ) );
static_assert( Netchan_WireHasFeature( NETCHAN_WIRE_IOQ3,
	NETCHAN_FEATURE_CLIENT_QPORT ) );
static_assert( Netchan_WireHasFeature( NETCHAN_WIRE_QL_RETAIL,
	NETCHAN_FEATURE_CLIENT_QPORT ) );

static_assert( !Netchan_WireHasFeature( NETCHAN_WIRE_LEGACY_Q3,
	NETCHAN_FEATURE_SEQUENCE_CHECKSUM ) );
static_assert( Netchan_WireHasFeature( NETCHAN_WIRE_IOQ3,
	NETCHAN_FEATURE_SEQUENCE_CHECKSUM ) );
static_assert( !Netchan_WireHasFeature( NETCHAN_WIRE_QL_RETAIL,
	NETCHAN_FEATURE_SEQUENCE_CHECKSUM ) );

static_assert( Netchan_WireHasFeature( NETCHAN_WIRE_LEGACY_Q3,
	NETCHAN_FEATURE_RELIABLE_XOR ) );
static_assert( !Netchan_WireHasFeature( NETCHAN_WIRE_IOQ3,
	NETCHAN_FEATURE_RELIABLE_XOR ) );
static_assert( Netchan_WireHasFeature( NETCHAN_WIRE_QL_RETAIL,
	NETCHAN_FEATURE_RELIABLE_XOR ) );

int main() {
	static_assert( Netchan_MessageLimit( NETCHAN_WIRE_LEGACY_Q3 ) == 16384 );
	static_assert( Netchan_MessageLimit( NETCHAN_WIRE_IOQ3 ) == 16384 );
	static_assert( Netchan_MessageLimit( NETCHAN_WIRE_QL_RETAIL ) == 16384 );
	static_assert( Netchan_MessageLimit( NETCHAN_WIRE_RTCW_SP ) == 32768 );
	static_assert( Netchan_MessageLimit( NETCHAN_WIRE_RTCW_MP ) == 32768 );
	static_assert( Netchan_ReliableCommandLimit( NETCHAN_WIRE_LEGACY_Q3 ) == 64 );
	static_assert( Netchan_ReliableCommandLimit( NETCHAN_WIRE_RTCW_MP ) == 256 );
	static_assert( Netchan_ReliableCommandLimit( static_cast<netchanWireProfile_t>( 99 ) ) == 0 );
	return 0;
}
