#version 430 core

// T & H modes of ETC2

// #include "/media/matias/Datos/SyntaxHighlightingMisc.h"

#include "CrossPlatformSettings_piece_all.glsl"
#include "UavCrossPlatform_piece_all.glsl"

uniform sampler2D srcTex;

layout( rg32ui, binding = 0 ) uniform restrict writeonly uimage2DArray dstTexture;

layout( local_size_x = 120,  //
		local_size_y = 4,    //
		local_size_z = 2 ) in;

#define FLT_MAX 340282346638528859811704183484516925440.0f

/*
kLocalInvocationToPixIdx table generated with:
	int main()
	{
		for( int pix1 = 0; pix1 < 15; pix1++ )
		{
			for( int pix2 = pix1 + 1; pix2 < 16; pix2++ )
				printf( "uint2( %iu, %iu ), ", pix1, pix2 );
		}
		printf( "\n" );
		return 0;
	}
*/
const uint2 kLocalInvocationToPixIdx[120] = {
	uint2( 0u, 1u ),   uint2( 0u, 2u ),   uint2( 0u, 3u ),   uint2( 0u, 4u ),   uint2( 0u, 5u ),
	uint2( 0u, 6u ),   uint2( 0u, 7u ),   uint2( 0u, 8u ),   uint2( 0u, 9u ),   uint2( 0u, 10u ),
	uint2( 0u, 11u ),  uint2( 0u, 12u ),  uint2( 0u, 13u ),  uint2( 0u, 14u ),  uint2( 0u, 15u ),
	uint2( 1u, 2u ),   uint2( 1u, 3u ),   uint2( 1u, 4u ),   uint2( 1u, 5u ),   uint2( 1u, 6u ),
	uint2( 1u, 7u ),   uint2( 1u, 8u ),   uint2( 1u, 9u ),   uint2( 1u, 10u ),  uint2( 1u, 11u ),
	uint2( 1u, 12u ),  uint2( 1u, 13u ),  uint2( 1u, 14u ),  uint2( 1u, 15u ),  uint2( 2u, 3u ),
	uint2( 2u, 4u ),   uint2( 2u, 5u ),   uint2( 2u, 6u ),   uint2( 2u, 7u ),   uint2( 2u, 8u ),
	uint2( 2u, 9u ),   uint2( 2u, 10u ),  uint2( 2u, 11u ),  uint2( 2u, 12u ),  uint2( 2u, 13u ),
	uint2( 2u, 14u ),  uint2( 2u, 15u ),  uint2( 3u, 4u ),   uint2( 3u, 5u ),   uint2( 3u, 6u ),
	uint2( 3u, 7u ),   uint2( 3u, 8u ),   uint2( 3u, 9u ),   uint2( 3u, 10u ),  uint2( 3u, 11u ),
	uint2( 3u, 12u ),  uint2( 3u, 13u ),  uint2( 3u, 14u ),  uint2( 3u, 15u ),  uint2( 4u, 5u ),
	uint2( 4u, 6u ),   uint2( 4u, 7u ),   uint2( 4u, 8u ),   uint2( 4u, 9u ),   uint2( 4u, 10u ),
	uint2( 4u, 11u ),  uint2( 4u, 12u ),  uint2( 4u, 13u ),  uint2( 4u, 14u ),  uint2( 4u, 15u ),
	uint2( 5u, 6u ),   uint2( 5u, 7u ),   uint2( 5u, 8u ),   uint2( 5u, 9u ),   uint2( 5u, 10u ),
	uint2( 5u, 11u ),  uint2( 5u, 12u ),  uint2( 5u, 13u ),  uint2( 5u, 14u ),  uint2( 5u, 15u ),
	uint2( 6u, 7u ),   uint2( 6u, 8u ),   uint2( 6u, 9u ),   uint2( 6u, 10u ),  uint2( 6u, 11u ),
	uint2( 6u, 12u ),  uint2( 6u, 13u ),  uint2( 6u, 14u ),  uint2( 6u, 15u ),  uint2( 7u, 8u ),
	uint2( 7u, 9u ),   uint2( 7u, 10u ),  uint2( 7u, 11u ),  uint2( 7u, 12u ),  uint2( 7u, 13u ),
	uint2( 7u, 14u ),  uint2( 7u, 15u ),  uint2( 8u, 9u ),   uint2( 8u, 10u ),  uint2( 8u, 11u ),
	uint2( 8u, 12u ),  uint2( 8u, 13u ),  uint2( 8u, 14u ),  uint2( 8u, 15u ),  uint2( 9u, 10u ),
	uint2( 9u, 11u ),  uint2( 9u, 12u ),  uint2( 9u, 13u ),  uint2( 9u, 14u ),  uint2( 9u, 15u ),
	uint2( 10u, 11u ), uint2( 10u, 12u ), uint2( 10u, 13u ), uint2( 10u, 14u ), uint2( 10u, 15u ),
	uint2( 11u, 12u ), uint2( 11u, 13u ), uint2( 11u, 14u ), uint2( 11u, 15u ), uint2( 12u, 13u ),
	uint2( 12u, 14u ), uint2( 12u, 15u ), uint2( 13u, 14u ), uint2( 13u, 15u ), uint2( 14u, 15u )
};

float3 getSrcPixel( uint idx )
{
	const uint2 pixelsToLoadBase = gl_GlobalInvocationID.yz << 2u;
	uint2 pixelsToLoad = pixelsToLoadBase;
	// Note ETC2 wants the src pixels transposed!
	pixelsToLoad.x += idx >> 2u;    //+= threadId / 4
	pixelsToLoad.y += idx & 0x03u;  //+= threadId % 4
	const float3 srcPixels0 = OGRE_Load2D( srcTex, int2( pixelsToLoad ), 0 ).xyz;
	return srcPixels0;
}

/// Quantizes 'srcValue' which is originally in 888 (full range),
/// converting it to 444 and then back to 888 (quantized)
uint quant4( const uint packedRgb )
{
	float3 rgbValue = unpackUnorm4x8( packedRgb ).xyz;  // Range [0; 1]
	rgbValue = floor( rgbValue * 15.0f + 0.5f );        // Convert to 444, range [0; 15]
	// rgbValue = floor( rgbValue * 19.05f );              // Convert to 888, range [0; 255]
	rgbValue = floor( rgbValue * 17.05f );              // Convert to 888, range [0; 255]
	return packUnorm4x8( float4( rgbValue * ( 1.0f / 255.0f ), 1.0f ) );
}

uint quant4( float3 rgbValue )
{
	rgbValue = floor( rgbValue * 15.0f / 255.0f + 0.5f );  // Convert to 444
	// rgbValue = floor( rgbValue * 19.05f );                 // Convert to 888
	rgbValue = floor( rgbValue * 17.05f );                 // Convert to 888
	return packUnorm4x8( float4( rgbValue * ( 1.0f / 255.0f ), 1.0f ) );
}

float calcError( const uint colour0, const uint colour1 )
{
	float3 diff = unpackUnorm4x8( colour0 ).xyz - unpackUnorm4x8( colour1 ).xyz;
	// return dot( diff, diff ) * 65025.0f; // 65025 = 255 * 255
	return (abs(diff.r)*0.3f + abs(diff.g)*0.59f + abs(diff.b)*0.11f) * 65025.0f;
}

float calcError( const uint colour0, const float3 colour1 )
{
	float3 diff = unpackUnorm4x8( colour0 ).xyz - colour1.xyz;
	// return dot( diff, diff ) * 65025.0f;  // 65025 = 255 * 255
	return (abs(diff.r)*0.3f + abs(diff.g)*0.59f + abs(diff.b)*0.11f) * 65025.0f;
}

void block_main_colors_find( out uint outC0, out uint outC1, uint c0, uint c1 )
{
	const int kMaxIterations = 20;

	bool bestMatchFound = false;

	// k-means complexity is O(n^(d.k+1) log n)
	// In this case, n = 16, k = 2, d = 3 so 20 loops

	for( int iter = 0; iter < kMaxIterations && !bestMatchFound; ++iter )
	{
		int cluster0_cnt = 0, cluster1_cnt = 0;
		int cluster0[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		int cluster1[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		float minDist0 = FLT_MAX, maxDist0 = 0;
		float minDist1 = FLT_MAX, maxDist1 = 0;

		int minDist0_idx = 0, maxDist0_idx = 0;
		int minDist1_idx = 0, maxDist1_idx = 0;
		

		// k-means assignment step
		for( int k = 0; k < 16; ++k )
		{
			const float dist0 = calcError( c0, getSrcPixel( k ) );
			const float dist1 = calcError( c1, getSrcPixel( k ) );
			if( dist0 <= dist1 )
			{
				cluster0[cluster0_cnt++] = k;
				maxDist0 = max( dist0, maxDist0 );
				if (minDist0 > dist0) {
					minDist0 = dist0;
					minDist0_idx = k;
				}

				if (maxDist0 < dist0) {
					maxDist0 = dist0;
					maxDist0_idx = k;
				}

			}
			else
			{
				cluster1[cluster1_cnt++] = k;
				maxDist1 = max( dist1, maxDist1 );
				if (minDist1 > dist1) {
					minDist1 = dist1;
					minDist1_idx = k;
				}

				if (maxDist1 < dist1) {
					maxDist1 = dist1;
					maxDist1_idx = k;
				}
			}
		}

		// k-means failed
		if( cluster0_cnt == 0 || cluster1_cnt == 0 )
		{
			// Actually we did not find the best match. But set this flag to abort
			// the loop and keep on going with the original colours (using 'break'
			// makes compilers go crazy)
			// bestMatchFound = true;
			if (cluster0_cnt > 0) {
				float3 rgb0 = getSrcPixel(maxDist0_idx);
				float3 rgb1 = getSrcPixel(minDist0_idx);
				c0 = quant4(rgb0);
				c1 = quant4(rgb1);
			}
			else { // if cluster1_cnt > 0
				float3 rgb0 = getSrcPixel(maxDist1_idx);
				float3 rgb1 = getSrcPixel(minDist1_idx);
				c0 = quant4(rgb0);
				c1 = quant4(rgb1);
			}
		}
		else
		{
			float3 rgb0 = float3( 0, 0, 0 );
			float3 rgb1 = float3( 0, 0, 0 );

			// k-means update step
			for( int k = 0; k < cluster0_cnt; ++k )
				rgb0 += getSrcPixel( cluster0[k] );

			for( int k = 0; k < cluster1_cnt; ++k )
				rgb1 += getSrcPixel( cluster1[k] );

			rgb0 = floor( rgb0 * ( 255.0f / cluster0_cnt ) + 0.5f );
			rgb1 = floor( rgb1 * ( 255.0f / cluster1_cnt ) + 0.5f );

			const uint newC0 = quant4( rgb0 );
			const uint newC1 = quant4( rgb1 );
			if( newC0 == c0 && newC1 == c1 )
			{
				bestMatchFound = true;
			}
			else
			{
				if( newC0 != newC1 )
				{
					c0 = newC0;
					c1 = newC1;
				}
				else if( calcError( newC0, c0 ) > calcError( newC1, c1 ) )
				{
					c0 = newC0;
				}
				else
				{
					c1 = newC1;
				}
			}
		}
	}

	outC0 = c0;
	outC1 = c1;
}

// hyeon add 
// struct baseColorCandidate {
// 	float error;
// 	uint c0;
// 	uint c1;
// };

// shared baseColorCandidate g_threadBaseColorCandidate[120];

void main()
{
	const uint pix0 = kLocalInvocationToPixIdx[gl_LocalInvocationID.x].x;
	const uint pix1 = kLocalInvocationToPixIdx[gl_LocalInvocationID.x].y;

	uint c0 = quant4( getSrcPixel( pix0 ) * 255.0f );
	uint c1 = quant4( getSrcPixel( pix1 ) * 255.0f );

	// uint ori_c0 = quant4( getSrcPixel( pix0 ) * 255.0f );
	// uint ori_c1 = quant4( getSrcPixel( pix1 ) * 255.0f );

	if( c0 != c1 )
	{
		uint newC0, newC1;
		block_main_colors_find( newC0, newC1, c0, c1 );
		c0 = newC0;
		c1 = newC1;
	}
	// const uint thisThreadLocalIdx = gl_LocalInvocationID.x;
	// float difference = calcError(ori_c0, c0) + calcError(ori_c1, c1);
	// g_threadBaseColorCandidate[thisThreadLocalIdx] = baseColorCandidate(difference, c0, c1);

	// __sharedOnlyBarrier;

	// int bestIndex = 0;
	// uint best_c0 = 0;
	// uint best_c1 = 0;
	// float minimum_err = 9999999;
	const uint2 dstUV = gl_GlobalInvocationID.yz;

	// if (gl_LocalInvocationID.x == 0u && gl_LocalInvocationID.y == 0u && gl_LocalInvocationID.z == 0u ) {
	// 	for ( int i = 0; i < 120; i++ ){
	// 		if (g_threadBaseColorCandidate[i].error == 0) {
	// 			imageStore( dstTexture, int3( dstUV, i ), uint4(g_threadBaseColorCandidate[i].c0 , g_threadBaseColorCandidate.c1, 0u, 0u ) );
	// 			break;
	// 		}
	// 		if (g_threadBaseColorCandidate[i] < minimum_err) {
	// 			minimum_err = g_threadBaseColorCandidate[i].error;
	// 			bestIndex = i;
	// 			best_c0 = g_threadBaseColorCandidate[i].c0;
	// 			best_c1 = g_threadBaseColorCandidate[i].c1;
	// 		}
	// 	}
	// 	imageStore( dstTexture, int3( dstUV, bestIndex ), uint4( best_c0, best_c1, 0u, 0u ) );
	// }
	imageStore( dstTexture, int3( dstUV, gl_LocalInvocationID.x ), uint4( c0, c1, 0u, 0u ) );
}