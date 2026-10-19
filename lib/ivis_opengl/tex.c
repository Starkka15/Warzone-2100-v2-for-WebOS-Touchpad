/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2011  Warzone 2100 Project

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

#include "gl_compat.h"
#include "lib/framework/frame.h"

#ifndef USE_GLES
#if defined(WZ_OS_MAC)
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif
#endif

#include "lib/ivis_common/ivisdef.h"
#include "lib/ivis_common/piestate.h"
#include "lib/ivis_common/tex.h"
#include "lib/ivis_common/rendmode.h"
#include "lib/ivis_common/piepalette.h"
#include "lib/ivis_common/ivispatch.h"
#include "lib/ivis_common/png_util.h"

#include "screen.h"

//*************************************************************************

iTexPage _TEX_PAGE[iV_TEX_MAX];
unsigned int _TEX_INDEX;

static void pie_PrintLoadedTextures(void);

#ifdef USE_GLES
/*
 * Model texture atlas
 * Each model part has a texture page of its own and a droid's body,
 * propulsion and weapon are on different pages, so drawing models switched
 * texture all the time, and every switch costs a draw call (see the batching
 * in gles_compat.c). Model pages of up to ATLAS_SLOT square go into slots of
 * one big texture instead; selecting one sets the texture coordinate
 * transform that maps its 0-256 coordinates into its slot.
 */
#define ATLAS_SIZE 2048
#define ATLAS_SLOT 256
#define ATLAS_PER_ROW (ATLAS_SIZE / ATLAS_SLOT)
#define ATLAS_SLOTS (ATLAS_PER_ROW * ATLAS_PER_ROW)
static GLuint atlasTexture = 0;
static int atlasSlotsUsed = 0;

/* Puts the image (already scaled to width x height) in an atlas slot of page i;
 * false if it cannot go in the atlas */
static bool atlasAdd(unsigned int i, const char *filename, int width, int height, unsigned int format, const unsigned char *bmp)
{
	unsigned char *rgba = NULL;
	int slot, x, y;

	// the skybox repeats its texture, which an atlas slot cannot
	if (width > ATLAS_SLOT || height > ATLAS_SLOT || strncmp(filename, "page-25", 7) == 0)
	{
		return false;
	}
	if (_TEX_PAGE[i].atlasSlot > 0)
	{
		slot = _TEX_PAGE[i].atlasSlot - 1;	// a replaced page keeps its slot
	}
	else if (atlasSlotsUsed < ATLAS_SLOTS)
	{
		slot = atlasSlotsUsed++;
	}
	else
	{
		debug(LOG_TEXTURE, "texture atlas full; %s gets a texture of its own", filename);
		return false;
	}

	if (atlasTexture == 0)
	{
		glGenTextures(1, &atlasTexture);
		glBindTexture(GL_TEXTURE_2D, atlasTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
	glBindTexture(GL_TEXTURE_2D, atlasTexture);

	// GLES uploads only in the texture's own format
	if (format != GL_RGBA)
	{
		int p;

		rgba = malloc(width * height * 4);
		for (p = 0; p < width * height; p++)
		{
			rgba[p * 4 + 0] = bmp[p * 3 + 0];
			rgba[p * 4 + 1] = bmp[p * 3 + 1];
			rgba[p * 4 + 2] = bmp[p * 3 + 2];
			rgba[p * 4 + 3] = 255;
		}
		bmp = rgba;
	}
	x = (slot % ATLAS_PER_ROW) * ATLAS_SLOT;
	y = (slot / ATLAS_PER_ROW) * ATLAS_SLOT;
	glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bmp);
	free(rgba);

	_TEX_PAGE[i].id = atlasTexture;
	_TEX_PAGE[i].atlasSlot = slot + 1;
	// texture coordinates are 0-256 whatever the page size, and the texture matrix scales by 1/256
	_TEX_PAGE[i].atlasScaleU = (float)width / ATLAS_SIZE;
	_TEX_PAGE[i].atlasScaleV = (float)height / ATLAS_SIZE;
	_TEX_PAGE[i].atlasOffsetU = x * OLD_TEXTURE_SIZE_FIX / ATLAS_SIZE;
	_TEX_PAGE[i].atlasOffsetV = y * OLD_TEXTURE_SIZE_FIX / ATLAS_SIZE;

	// the binding changed behind pie_SetTexturePage's back
	pie_SetTexturePage(TEXPAGE_NONE);
	return true;
}
#endif

//*************************************************************************


/**************************************************************************
	Add an image buffer given in s as a new texture page in the texture
	table.  We check first if the given image has already been loaded,
	as a sanity check (should never happen).  The texture numbers are
	stored in a special texture table, not in the resource system, for
	some unknown reason. Start looking for an available slot in the
	texture table at the given slot number.

	Returns the texture number of the image.
**************************************************************************/
int pie_AddTexPage(iV_Image *s, const char* filename, int slot, int maxTextureSize)
{
	unsigned int i = 0;
	int width, height;
	void *bmp;
	bool scaleDown = false;

	/* Have we already loaded this one? Should not happen here. */
	while (i < _TEX_INDEX)
	{
		if (strncmp(filename, _TEX_PAGE[i].name, iV_TEXNAME_MAX) == 0)
		{
			pie_PrintLoadedTextures();
		}
		ASSERT(strncmp(filename, _TEX_PAGE[i].name, iV_TEXNAME_MAX) != 0,
		       "pie_AddTexPage: %s loaded again! Already loaded as %s|%u", filename,
		       _TEX_PAGE[i].name, i);
		i++;
	}

	/* Use first unused slot */
	for (i = slot; i < iV_TEX_MAX && _TEX_PAGE[i].name[0] != '\0'; i++);

	if (i == _TEX_INDEX)
	{
		_TEX_INDEX++; // increase table
	}
	ASSERT(i != iV_TEX_MAX, "pie_AddTexPage: too many texture pages");

	debug(LOG_TEXTURE, "pie_AddTexPage: %s page=%d", filename, _TEX_INDEX);
	assert(s != NULL);

	/* Stick the name into the tex page structures */
	sstrcpy(_TEX_PAGE[i].name, filename);

#ifdef USE_GLES
	if (maxTextureSize)
	{
		int w = s->width, h = s->height;
		unsigned char *pixels = s->bmp;

		if ((w & (w - 1)) == 0 && (h & (h - 1)) == 0)
		{
			if (w > maxTextureSize || h > maxTextureSize)
			{
				w = MIN(w, maxTextureSize);
				h = MIN(h, maxTextureSize);
				pixels = malloc(4 * w * h);
				gluScaleImage(iV_getPixelFormat(s), s->width, s->height, GL_UNSIGNED_BYTE, s->bmp,
				              w, h, GL_UNSIGNED_BYTE, pixels);
			}
			if (atlasAdd(i, filename, w, h, iV_getPixelFormat(s), pixels))
			{
				if (pixels != s->bmp)
				{
					free(pixels);
				}
				free(s->bmp);
				s->bmp = NULL;
				_TEX_INDEX++;	// as below
				return i;
			}
			if (pixels != s->bmp)
			{
				free(pixels);
			}
		}
	}
	_TEX_PAGE[i].atlasSlot = 0;
#endif

	glGenTextures(1, &_TEX_PAGE[i].id);
	// FIXME: This function is used instead of glBindTexture, but we're juggling with difficult to trace global state here. Look into pie_SetTexturePage's definition for details.
	pie_SetTexturePage(i);

	width = s->width;
	height = s->height;
	bmp = s->bmp;
	if ((width & (width-1)) == 0 && (height & (height-1)) == 0)
	{
		if (maxTextureSize && width > maxTextureSize)
		{
			width = maxTextureSize;
			scaleDown = true;
		}
		if (maxTextureSize && height > maxTextureSize)
		{
			height = maxTextureSize;
			scaleDown = true;
		}
		if (scaleDown)
		{
			debug(LOG_TEXTURE, "scaling down texture %s from %ix%i to %ix%i", filename, s->width, s->height, width, height);
			bmp = malloc(4 * width * height); // FIXME: don't know for sure it is 4 bytes per pixel
			gluScaleImage(iV_getPixelFormat(s), s->width, s->height, GL_UNSIGNED_BYTE, s->bmp,
			                                    width,    height,    GL_UNSIGNED_BYTE, bmp);
			free(s->bmp);
		}

		if (maxTextureSize)
		{
#ifdef USE_GLES
			// 3D textures are drawn minified; GLES 1.1 builds their mipmaps on upload
			glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
#endif
			// this is a 3D texture, use texture compression
			gluBuild2DMipmaps(GL_TEXTURE_2D, wz_texture_compression, width, height, iV_getPixelFormat(s), GL_UNSIGNED_BYTE, bmp);
		}
		else
		{
			// this is an interface texture, do not use compression
			gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height, iV_getPixelFormat(s), GL_UNSIGNED_BYTE, bmp);
		}
	} else {
		debug(LOG_ERROR, "pie_AddTexPage: non POT texture %s", filename);
	}
	free(bmp); // it is uploaded, we do not need it anymore
	s->bmp = NULL;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#ifdef USE_GLES
	/* Mipmaps only for 3D textures (see above); interface images are drawn 1:1 */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, maxTextureSize ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
#else
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
#endif
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Use anisotropic filtering, if available, but only max 4.0 to reduce processor burden
	if (GLEE_EXT_texture_filter_anisotropic)
	{
		GLfloat max;

		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, MIN(4.0f, max));
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	/* Send back the texpage number so we can store it in the IMD */

	_TEX_INDEX++;

	return i;
}


void pie_InitSkybox(SDWORD pageNum)
{
	pie_SetTexturePage(pageNum);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
}


/*!
 * Turns filename into a pagename if possible
 * \param[in,out] filename Filename to pagify
 */
void pie_MakeTexPageName(char * filename)
{
	if (strncmp(filename, "page-", 5) == 0)
	{
		int i;
		for( i = 5; i < iV_TEXNAME_MAX-1 && isdigit(filename[i]); i++);
		filename[i] = '\0';
	}
}


/*!
 * Print the names of all loaded textures to LOG_ERROR
 */
static void pie_PrintLoadedTextures(void)
{
	unsigned int i = 0;

	debug(LOG_ERROR, "Available texture pages in memory (%d out of %d max):", _TEX_INDEX, iV_TEX_MAX);

	for ( i = 0; i < iV_TEX_MAX && _TEX_PAGE[i].name[0] != '\0'; i++ )
	{
		debug(LOG_ERROR, "%02d : %s", i, _TEX_PAGE[i].name);
	}
}

/** Retrieve the texture number for a given texture resource.
 *
 *  @note We keep textures in a separate data structure _TEX_PAGE apart from the
 *        normal resource system.
 *
 *  @param filename The filename of the texture page to search for.
 *
 *  @return a non-negative index number for the texture, negative if no texture
 *          with the given filename could be found
 */
int iV_GetTexture(const char *filename)
{
	unsigned int i = 0;

	/* Have we already loaded this one then? */
	for ( i = 0; i < iV_TEX_MAX; i++ ) {
		if (strncmp(filename, _TEX_PAGE[i].name, iV_TEXNAME_MAX) == 0) {
			return i;
		}
	}

	/* This should never happen - by now all textures should have been loaded. */
	debug(LOG_ERROR, "*** texture %s not loaded! ***", filename);
	debug(LOG_ERROR, "This error probably means you did not specify this texture to be preloaded in the appropriate wrf files before referencing it in some pie file");
	debug(LOG_ERROR, "Remember that patches override several standard wrf files as well");

	pie_PrintLoadedTextures();

	return -1;
}


/**************************************************************************
	WRF files may specify overrides for the textures on a map. This
	is done through an ugly hack involving cutting the texture name
	down to just "page-NN", where NN is the page number, and
	replaceing the texture page with the same name if another file
	with this prefix is loaded.
**************************************************************************/
int pie_ReplaceTexPage(iV_Image *s, const char *texPage, int maxTextureSize)
{
	int i = iV_GetTexture(texPage);

	ASSERT(i >= 0, "pie_ReplaceTexPage: Cannot find any %s to replace!", texPage);
	if (i < 0)
	{
		return -1;
	}

#ifdef USE_GLES
	if (_TEX_PAGE[i].atlasSlot == 0)	// an atlas page is overwritten in its slot
#endif
	glDeleteTextures(1, &_TEX_PAGE[i].id);
	debug(LOG_TEXTURE, "Reloading texture %s from index %d", texPage, i);
	_TEX_PAGE[i].name[0] = '\0';
	pie_AddTexPage(s, texPage, i, maxTextureSize);

	return i;
}


/*
	Alex - fixed this so it doesn't try to free up the memory if it got the page from resource
	handler - this is because the resource handler will deal with freeing it, and in all probability
	will have already done so by the time this is called, thus avoiding an 'already freed' moan.
*/
void pie_TexShutDown(void)
{
	unsigned int i = 0;

	while (i < _TEX_INDEX)
	{
#ifdef USE_GLES
		if (_TEX_PAGE[i].atlasSlot > 0)
		{
			_TEX_PAGE[i].atlasSlot = 0;
			i++;
			continue;
		}
#endif
		glDeleteTextures(1, (GLuint *) &_TEX_PAGE[i].id);
		i++;
	}
#ifdef USE_GLES
	if (atlasTexture)
	{
		glDeleteTextures(1, &atlasTexture);
		atlasTexture = 0;
	}
	atlasSlotsUsed = 0;
#endif

	debug(LOG_TEXTURE, "pie_TexShutDown successful - did free %u texture pages", i);
}

void pie_TexInit(void)
{
	int i = 0;

	while (i < iV_TEX_MAX) {
		_TEX_PAGE[i].name[0] = '\0';
		_TEX_PAGE[i].atlasSlot = 0;
		i++;
	}
	debug(LOG_TEXTURE, "pie_TexInit successful - initialized %d texture pages\n", i);
}


void iV_unloadImage(iV_Image *image)
{
	if (image)
	{
		if (image->bmp)
		{
			free(image->bmp);
			image->bmp = NULL;
		}
	}
	else
	{
		debug(LOG_ERROR, "Tried to free invalid image!");
	}
}


unsigned int iV_getPixelFormat(const iV_Image *image)
{
	switch (image->depth)
	{
		case 3:
			return GL_RGB;
		case 4:
			return GL_RGBA;
		default:
			debug(LOG_ERROR, "iV_getPixelFormat: Unsupported image depth: %u", image->depth);
			return GL_INVALID_ENUM;
	}
}
