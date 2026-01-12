/*
 * OpenGL/GLES Compatibility Wrapper for Warzone 2100
 * Include this instead of GLee.h
 */

#ifndef GL_COMPAT_H
#define GL_COMPAT_H

#ifdef USE_GLES
/* Use GLES 1.1 compatibility layer */
#include "gles_compat.h"
#else
/* Use standard desktop OpenGL with GLee */
#include <GLee.h>
#endif

#endif /* GL_COMPAT_H */
