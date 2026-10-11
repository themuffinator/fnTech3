/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef RTX_RT_FRAME_STATS_H
#define RTX_RT_FRAME_STATS_H

#define RT_FRAME_STATS_SAMPLES 256
typedef struct {
	float samples[RT_FRAME_STATS_SAMPLES];
	unsigned count, cursor;
} rtFrameStats_t;

static void RT_FrameStatsAdd( rtFrameStats_t *stats, float milliseconds )
{
	if ( !( milliseconds >= 0.0f && milliseconds < 1e6f ) ) return;
	stats->samples[stats->cursor] = milliseconds;
	stats->cursor = ( stats->cursor + 1 ) % RT_FRAME_STATS_SAMPLES;
	if ( stats->count < RT_FRAME_STATS_SAMPLES ) stats->count++;
}

static int RT_FrameStatsCompare( const void *a, const void *b )
{
	float x = *(const float *)a, y = *(const float *)b;
	return x < y ? -1 : x > y ? 1 : 0;
}

static void RT_FrameStatsPercentiles( const rtFrameStats_t *stats, float *p50, float *p95, float *p99 )
{
	float sorted[RT_FRAME_STATS_SAMPLES];
	unsigned n = stats->count;
	if ( !n ) { *p50 = *p95 = *p99 = 0.0f; return; }
	memcpy( sorted, stats->samples, n * sizeof( float ) );
	qsort( sorted, n, sizeof( float ), RT_FrameStatsCompare );
	*p50 = sorted[( n * 50 + 99 ) / 100 - 1];
	*p95 = sorted[( n * 95 + 99 ) / 100 - 1];
	*p99 = sorted[( n * 99 + 99 ) / 100 - 1];
}

#endif
