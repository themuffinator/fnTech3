#include "../code/rendererrtx/rtx_scene_signature.h"
#include <assert.h>
#include <stdio.h>

int main( void )
{
	assert( RTX_QuantizeSceneFloat( 1.49f, 1.0f ) == 1u );
	assert( RTX_QuantizeSceneFloat( 1.5f, 1.0f ) == 2u );
	assert( RTX_QuantizeSceneFloat( -1.5f, 1.0f ) == (uint32_t)-2 );
	assert( RTX_QuantizeSceneFloat( 0.0f, 1.0f ) == 0u );
	assert( RTX_QuantizeSceneFloat( -0.0f, 1.0f ) == 0u );
	assert( RTX_QuantizeSceneFloat( 16.0f, 4.0f ) == 4u );
	assert( RTX_QuantizeSceneFloat( 2147483648.0f, 1.0f ) == UINT32_C( 0x7fffffff ) );
	assert( RTX_QuantizeSceneFloat( -2147483648.0f, 1.0f ) == UINT32_C( 0x80000000 ) );
	assert( RTX_QuantizeSceneFloat( FLT_MAX, FLT_MIN ) == UINT32_C( 0x7fffffff ) );
	assert( RTX_QuantizeSceneFloat( -FLT_MAX, FLT_MIN ) == UINT32_C( 0x80000000 ) );
	assert( RTX_QuantizeSceneFloat( NAN, 1.0f ) == UINT32_C( 0x7fc00000 ) );
	assert( RTX_QuantizeSceneFloat( INFINITY, 1.0f ) == UINT32_C( 0x7f800000 ) );
	assert( RTX_QuantizeSceneFloat( -INFINITY, 1.0f ) == UINT32_C( 0xff800000 ) );
	assert( RTX_QuantizeSceneFloat( 2.0f, NAN ) == 2u );
	assert( RTX_QuantizeSceneFloat( 2.0f, INFINITY ) == 2u );
	assert( RTX_QuantizeSceneFloat( 2.0f, 0.0f ) == 2u );
	assert( RTX_QuantizeSceneFloat( 2.0f, -1.0f ) == 2u );
	puts( "RTX scene signature quantization tests passed" );
	return 0;
}
