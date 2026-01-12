/*
 * GLES 1.1 Compatibility Layer for Warzone 2100
 * Replaces immediate mode (glBegin/glEnd) with vertex arrays
 */

#include "gles_compat.h"
#include <string.h>

/* Undef macros to prevent recursion in this implementation file */
#ifdef USE_GLES
#undef glColor3f
#undef glColor4f
#undef glColor4ub
#undef glColor4ubv
#undef glNormal3f
#endif

/* Global immediate mode buffer */
GLESImmediateBuffer g_imm;

/* Attribute stack for glPushAttrib/glPopAttrib emulation */
GLESAttribState g_attribStack[GLES_ATTRIB_STACK_DEPTH];
int g_attribStackDepth = 0;

void glesCompatInit(void)
{
    memset(&g_imm, 0, sizeof(g_imm));
    g_imm.currentColor[0] = 255;
    g_imm.currentColor[1] = 255;
    g_imm.currentColor[2] = 255;
    g_imm.currentColor[3] = 255;
}

void glesBegin(GLenum mode)
{
    g_imm.mode = mode;
    g_imm.count = 0;
    g_imm.hasColor = 0;
    g_imm.hasTexCoord = 0;
    g_imm.hasNormal = 0;
}

static void glesFlush(void)
{
    if (g_imm.count == 0) return;

    /* Set up vertex pointer - always enabled */
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, g_imm.position);

    /* Color array */
    if (g_imm.hasColor) {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, g_imm.color);
    }

    /* Texture coordinate array */
    if (g_imm.hasTexCoord) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, g_imm.texcoord);
    }

    /* Normal array */
    if (g_imm.hasNormal) {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, 0, g_imm.normal);
    }

    /* Handle GL_QUADS by converting to triangles */
    if (g_imm.mode == GL_QUADS) {
        /* Convert quads to triangles: 0,1,2 and 0,2,3 for each quad */
        int numQuads = g_imm.count / 4;
        GLushort indices[GLES_MAX_VERTICES * 6 / 4];
        int idx = 0;
        for (int q = 0; q < numQuads; q++) {
            int base = q * 4;
            indices[idx++] = base + 0;
            indices[idx++] = base + 1;
            indices[idx++] = base + 2;
            indices[idx++] = base + 0;
            indices[idx++] = base + 2;
            indices[idx++] = base + 3;
        }
        glDrawElements(GL_TRIANGLES, idx, GL_UNSIGNED_SHORT, indices);
    }
    /* Handle GL_QUAD_STRIP by converting to triangle strip */
    else if (g_imm.mode == GL_QUAD_STRIP) {
        /* Quad strip can be drawn as triangle strip with same vertices */
        glDrawArrays(GL_TRIANGLE_STRIP, 0, g_imm.count);
    }
    else {
        /* Draw normally for supported primitives */
        glDrawArrays(g_imm.mode, 0, g_imm.count);
    }

    /* Disable client states */
    glDisableClientState(GL_VERTEX_ARRAY);
    if (g_imm.hasColor) {
        glDisableClientState(GL_COLOR_ARRAY);
    }
    if (g_imm.hasTexCoord) {
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    if (g_imm.hasNormal) {
        glDisableClientState(GL_NORMAL_ARRAY);
    }
}

void glesEnd(void)
{
    glesFlush();
}

void glesVertex2f(GLfloat x, GLfloat y)
{
    if (g_imm.count >= GLES_MAX_VERTICES) return;

    int i = g_imm.count;
    g_imm.position[i * 3 + 0] = x;
    g_imm.position[i * 3 + 1] = y;
    g_imm.position[i * 3 + 2] = 0.0f;

    /* Copy current color */
    g_imm.color[i * 4 + 0] = g_imm.currentColor[0];
    g_imm.color[i * 4 + 1] = g_imm.currentColor[1];
    g_imm.color[i * 4 + 2] = g_imm.currentColor[2];
    g_imm.color[i * 4 + 3] = g_imm.currentColor[3];

    /* Copy current texcoord */
    g_imm.texcoord[i * 2 + 0] = g_imm.currentTexCoord[0];
    g_imm.texcoord[i * 2 + 1] = g_imm.currentTexCoord[1];

    /* Copy current normal */
    g_imm.normal[i * 3 + 0] = g_imm.currentNormal[0];
    g_imm.normal[i * 3 + 1] = g_imm.currentNormal[1];
    g_imm.normal[i * 3 + 2] = g_imm.currentNormal[2];

    g_imm.count++;
}

void glesVertex2i(GLint x, GLint y)
{
    glesVertex2f((GLfloat)x, (GLfloat)y);
}

void glesVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    if (g_imm.count >= GLES_MAX_VERTICES) return;

    int i = g_imm.count;
    g_imm.position[i * 3 + 0] = x;
    g_imm.position[i * 3 + 1] = y;
    g_imm.position[i * 3 + 2] = z;

    /* Copy current color */
    g_imm.color[i * 4 + 0] = g_imm.currentColor[0];
    g_imm.color[i * 4 + 1] = g_imm.currentColor[1];
    g_imm.color[i * 4 + 2] = g_imm.currentColor[2];
    g_imm.color[i * 4 + 3] = g_imm.currentColor[3];

    /* Copy current texcoord */
    g_imm.texcoord[i * 2 + 0] = g_imm.currentTexCoord[0];
    g_imm.texcoord[i * 2 + 1] = g_imm.currentTexCoord[1];

    /* Copy current normal */
    g_imm.normal[i * 3 + 0] = g_imm.currentNormal[0];
    g_imm.normal[i * 3 + 1] = g_imm.currentNormal[1];
    g_imm.normal[i * 3 + 2] = g_imm.currentNormal[2];

    g_imm.count++;
}

void glesVertex3fv(const GLfloat *v)
{
    glesVertex3f(v[0], v[1], v[2]);
}

void glesColor3f(GLfloat r, GLfloat g, GLfloat b)
{
    g_imm.currentColor[0] = (GLubyte)(r * 255.0f);
    g_imm.currentColor[1] = (GLubyte)(g * 255.0f);
    g_imm.currentColor[2] = (GLubyte)(b * 255.0f);
    g_imm.currentColor[3] = 255;
    g_imm.hasColor = 1;

    /* Also set the GL color for non-immediate mode calls */
    glColor4f(r, g, b, 1.0f);
}

void glesColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    g_imm.currentColor[0] = (GLubyte)(r * 255.0f);
    g_imm.currentColor[1] = (GLubyte)(g * 255.0f);
    g_imm.currentColor[2] = (GLubyte)(b * 255.0f);
    g_imm.currentColor[3] = (GLubyte)(a * 255.0f);
    g_imm.hasColor = 1;

    glColor4f(r, g, b, a);
}

void glesColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    g_imm.currentColor[0] = r;
    g_imm.currentColor[1] = g;
    g_imm.currentColor[2] = b;
    g_imm.currentColor[3] = a;
    g_imm.hasColor = 1;

    glColor4ub(r, g, b, a);
}

void glesColor4ubv(const GLubyte *v)
{
    g_imm.currentColor[0] = v[0];
    g_imm.currentColor[1] = v[1];
    g_imm.currentColor[2] = v[2];
    g_imm.currentColor[3] = v[3];
    g_imm.hasColor = 1;

    glColor4ub(v[0], v[1], v[2], v[3]);
}

void glesTexCoord2f(GLfloat u, GLfloat v)
{
    g_imm.currentTexCoord[0] = u;
    g_imm.currentTexCoord[1] = v;
    g_imm.hasTexCoord = 1;
}

void glesTexCoord2fv(const GLfloat *v)
{
    g_imm.currentTexCoord[0] = v[0];
    g_imm.currentTexCoord[1] = v[1];
    g_imm.hasTexCoord = 1;
}

void glesNormal3fv(const GLfloat *v)
{
    g_imm.currentNormal[0] = v[0];
    g_imm.currentNormal[1] = v[1];
    g_imm.currentNormal[2] = v[2];
    g_imm.hasNormal = 1;

    glNormal3f(v[0], v[1], v[2]);
}

void glesPushAttrib(GLbitfield mask)
{
    if (g_attribStackDepth >= GLES_ATTRIB_STACK_DEPTH) {
        return; /* Stack overflow - silently ignore */
    }

    GLESAttribState *state = &g_attribStack[g_attribStackDepth];

    /* Query current state */
    glGetBooleanv(GL_DEPTH_WRITEMASK, &state->depthMask);
    state->fogEnabled = glIsEnabled(GL_FOG);
    state->blendEnabled = glIsEnabled(GL_BLEND);
    state->alphaTestEnabled = glIsEnabled(GL_ALPHA_TEST);
    glGetIntegerv(GL_BLEND_SRC, (GLint*)&state->blendSrc);
    glGetIntegerv(GL_BLEND_DST, (GLint*)&state->blendDst);

    g_attribStackDepth++;
}

void glesPopAttrib(void)
{
    if (g_attribStackDepth <= 0) {
        return; /* Stack underflow - silently ignore */
    }

    g_attribStackDepth--;
    GLESAttribState *state = &g_attribStack[g_attribStackDepth];

    /* Restore state */
    glDepthMask(state->depthMask);

    if (state->fogEnabled) {
        glEnable(GL_FOG);
    } else {
        glDisable(GL_FOG);
    }

    if (state->blendEnabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }

    if (state->alphaTestEnabled) {
        glEnable(GL_ALPHA_TEST);
    } else {
        glDisable(GL_ALPHA_TEST);
    }

    glBlendFunc(state->blendSrc, state->blendDst);
}

const char* glesErrorString(GLenum error)
{
    switch (error) {
        case GL_NO_ERROR:          return "no error";
        case GL_INVALID_ENUM:      return "invalid enum";
        case GL_INVALID_VALUE:     return "invalid value";
        case GL_INVALID_OPERATION: return "invalid operation";
        case GL_OUT_OF_MEMORY:     return "out of memory";
        case GL_STACK_OVERFLOW:    return "stack overflow";
        case GL_STACK_UNDERFLOW:   return "stack underflow";
        default:                   return "unknown error";
    }
}

void glesBuild2DMipmaps(GLenum target, GLint internalformat, GLsizei width,
                        GLsizei height, GLenum format, GLenum type, const void *data)
{
    /* GLES 1.1 can auto-generate mipmaps via GL_GENERATE_MIPMAP parameter */
    glTexParameteri(target, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2D(target, 0, internalformat, width, height, 0, format, type, data);
}

void glesScaleImage(GLenum format, GLsizei srcW, GLsizei srcH, GLenum srcType, const void *src,
                    GLsizei dstW, GLsizei dstH, GLenum dstType, void *dst)
{
    /* Simple box filter downscale - assumes RGBA, GL_UNSIGNED_BYTE */
    const unsigned char *srcData = (const unsigned char *)src;
    unsigned char *dstData = (unsigned char *)dst;
    int bpp = (format == GL_RGBA) ? 4 : 3;

    for (int y = 0; y < dstH; y++) {
        for (int x = 0; x < dstW; x++) {
            /* Map destination pixel to source region */
            int sx = x * srcW / dstW;
            int sy = y * srcH / dstH;

            /* Simple nearest neighbor for now */
            int srcIdx = (sy * srcW + sx) * bpp;
            int dstIdx = (y * dstW + x) * bpp;

            for (int c = 0; c < bpp; c++) {
                dstData[dstIdx + c] = srcData[srcIdx + c];
            }
        }
    }
}
