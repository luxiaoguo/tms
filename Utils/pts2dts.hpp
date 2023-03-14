/*****************************************************************************
 * live555_dtsgen.h : DTS rebuilder for pts only streams
 *****************************************************************************
 * Copyright (C) 2018 VideoLabs, VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <cstdio>
#include <cstdlib>
#include <cstring>
typedef long int64_t;
typedef unsigned long uint64_t;
typedef int64_t mtime_t;

/*
 *  https://blog.csdn.net/u012459903/article/details/100274939  from there
 */

// #define VLC_TS_INVALID INT64_C(0)
// #define VLC_TS_0 INT64_C(1)

// #define CLOCK_FREQ INT64_C(1000000)

#define VLC_TS_INVALID (0)
#define VLC_TS_0 (1)

#define CLOCK_FREQ (1000000)

/* __MAX and __MIN: self explanatory */
#ifndef __MAX
#define __MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef __MIN
#define __MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define DTSGEN_REORDER_MAX 4 /* should be enough */
#define DTSGEN_HISTORY_COUNT (DTSGEN_REORDER_MAX + 2)
// #define DTSGEN_DEBUG

struct dtsgen_t
{
    mtime_t history[DTSGEN_HISTORY_COUNT];
    mtime_t ordereddts[DTSGEN_HISTORY_COUNT];
    mtime_t i_startingdts;
    mtime_t i_startingdiff;
    unsigned reorderdepth = 0;
    unsigned count = 0;
};

int cmpvlctickp(const void *p1, const void *p2);
void dtsgen_Init(struct dtsgen_t *d);
void dtsgen_Resync(struct dtsgen_t *d);

#define dtsgen_Clean(d)

/*
 * RTSP sends in decode order, but only provides PTS as timestamp
 * P0 P2 P3 P1 P5 P7 P8 P6
 * D0 D2 D3 D1 D5 D7 D8 D6 <- wrong !
 * creating a non monotonical sequence when used as DTS, then PCR
 *
 * We need to have a suitable DTS for proper PCR and transcoding
 * with the usual requirements DTS0 < DTS1 and DTSN < PTSN
 *
 * So we want to find the closest DTS matching those conditions
 *  P0 P2 P3[P1]P5 P7 P8 P6
 * [D0]D1 D2 D3 D4 D5 D6 D7
 *
 * Which means that within a reorder window,
 * we want the PTS time index after reorder as DTS
 * [P0 P2 P3 P1]P5 P7 P8 P6
 * [P0 P1 P2 P3] reordered
 * [D0 D1 D2 D3]D4 D5 D6 D7
 * we need to pick then N frames before in reordered order (== reorder depth)
 *  P0 P2 P3[P1]P5 P7 P8 P6
 * [D0]D1 D2 D3 D4 D5 D6 D7
 * so D0 < P1 (we can also pick D1 if we want DTSN <= PTSN)
 *
 * Since it would create big delays with low fps streams we need
 * - to avoid buffering packets
 * - to detect real reorder depth (low fps usually have no reorder)
 *
 * We need then to:
 * - Detect reorder depth
 * - Keep track of last of N past timestamps, > maximum possible reorder
 * - Make sure a suitable dts is still created while detecting reorder depth
 *
 * While receiving the N first packets (N>max reorder):
 * - check if it needs reorder, or increase depth
 * - create slow increments in DTS while taking any frame as a start,
 *   substracting the total difference between first and last packet,
 *   and removing the possible offset after reorder,
 *   divided by max possible frames.
 *
 * Once reorder depth is fully known,
 * - use N previous frames reordered PTS as DTS for current PTS.
 *  (with mandatory gap/increase in DTS caused by previous step)
 */

void dtsgen_AddNextPTS(struct dtsgen_t *d, mtime_t i_pts);
mtime_t dtsgen_GetDTS(struct dtsgen_t *d);

#ifdef DTSGEN_DEBUG
static void dtsgen_Debug(vlc_object_t *p_demux, struct dtsgen_t *d, mtime_t dts, mtime_t pts)
{
    if (pts == VLC_TS_INVALID)
        return;
    msg_Dbg(p_demux,
            "dtsgen %" PRId64 " / pts %" PRId64 " diff %" PRId64 ", "
            "pkt count %u, reorder %u",
            dts % (10 * CLOCK_FREQ), pts % (10 * CLOCK_FREQ), (pts - dts) % (10 * CLOCK_FREQ), d->count,
            d->reorderdepth);
}
#else

// int main()
// {
// 	struct dtsgen_t dtsgen;
	
// 	dtsgen_Init( &dtsgen );
// 	dtsgen_Resync( &dtsgen );
 
// 	int64_t pts[8]={0,2,3,1,5,7,8,6};
// 	//int64_t pts[10]={1,2,3,4,5,7,8,6,9,10};
// 	int i =0 ;
// 	for(i = 0; i<sizeof(pts)/sizeof(pts[0]); i++)
// 	{
// 		dtsgen_AddNextPTS( &dtsgen, pts[i] );
// 		printf("get dts :%ld\n", dtsgen_GetDTS( &dtsgen ));
// 	}	
 
 
// 	printf("wang hello world  [%d%s]\n",__LINE__,__FUNCTION__);
// }


#define dtsgen_Debug(a, b, c, d)
#endif