/*
	This file is part of Warzone 2100.
	Copyright (C) 2026  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/** \file
 *  Frame timing by stage, for finding where a frame's time goes.
 *  Off unless WZ_PROFILE is set in the environment; then every 100 frames
 *  the average milliseconds per stage go to stderr. While on, the GPU is
 *  waited for before each buffer swap so its time is counted on its own.
 */

#ifndef __INCLUDED_LIB_FRAMEWORK_WZPROF_H__
#define __INCLUDED_LIB_FRAMEWORK_WZPROF_H__

#include <sys/time.h>

enum
{
	PROF_GPU,	// waiting for the GPU to finish the frame
	PROF_SWAP,	// buffer swap and clear
	PROF_LOGIC,	// game update, scripts, input
	PROF_SKY,	// skybox and fog box
	PROF_TERRAIN,	// terrain tiles: height, lighting, projection
	PROF_EFFECTS,	// effects, particles and visibility updates
	PROF_TDRAW,	// terrain tiles: building and drawing the triangles
	PROF_WATER,	// water edges and water tiles
	PROF_OBJECTS,	// structures, features, droids, effects, projectiles
	PROF_HUD,	// selection boxes, radar, console text
	PROF_UI,	// in-game interface widgets
	PROF_COUNT,
	PROF_DRAWCALL = PROF_COUNT,	// inside glDrawArrays/glDrawElements; overlaps the stages above
	PROF_TSUBMIT,	// the terrain's draw call; overlaps the above
	PROF_ALL
};

extern int wzProfOn;
extern double wzProfTotal[PROF_ALL];
extern double wzProfStart[PROF_ALL];
extern unsigned int wzProfDrawCalls;

static inline double wzProfNow(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

#define WZPROF_BEGIN(stage) do { if (wzProfOn) wzProfStart[stage] = wzProfNow(); } while (0)
#define WZPROF_END(stage) do { if (wzProfOn) wzProfTotal[stage] += wzProfNow() - wzProfStart[stage]; } while (0)

/// Call once per frame; reports every 100 frames.
void wzProfFrame(unsigned int pies, unsigned int polys, unsigned int tiles, unsigned int states);

/// Prints why GL batches were drawn (gles_compat.c), per frame, and resets the counts.
void glesProfReport(unsigned int frames);

#endif // __INCLUDED_LIB_FRAMEWORK_WZPROF_H__
