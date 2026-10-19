/*
 * GLES 1.1 Compatibility Layer for Warzone 2100
 * Replaces immediate mode (glBegin/glEnd) with vertex arrays
 */

#include "gles_compat.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Undef macros to prevent recursion in this implementation file */
#ifdef USE_GLES
#undef glColor3f
#undef glColor4f
#undef glColor4ub
#undef glColor4ubv
#undef glNormal3f
#undef glEnableClientState
#undef glDisableClientState
#undef glDrawArrays
#undef glDrawElements
#undef glEnable
#undef glDisable
#undef glIsEnabled
#undef glBindTexture
#undef glDeleteTextures
#undef glBlendFunc
#undef glDepthMask
#undef glDepthFunc
#undef glAlphaFunc
#undef glCullFace
#undef glFrontFace
#undef glDepthRangef
#undef glPolygonOffset
#undef glColorMask
#undef glTexParameteri
#undef glTexParameterf
#undef glTexEnvf
#undef glTexImage2D
#undef glTexSubImage2D
#undef glClear
#undef glFinish
#undef glFlush
#undef glReadPixels
#undef glScissor
#undef glViewport
#undef glFogf
#undef glFogfv
#undef glLightfv
#undef glLightModelf
#undef glLightModelfv
#undef glMaterialf
#undef glMaterialfv
#undef glStencilFunc
#undef glStencilOp
#undef glStencilMask
#undef glHint
#undef glColor4fv
#undef glMatrixMode
#undef glLoadIdentity
#undef glLoadMatrixf
#undef glMultMatrixf
#undef glPushMatrix
#undef glPopMatrix
#undef glTranslatef
#undef glRotatef
#undef glScalef
#undef glOrthof
#undef glFrustumf
#endif

#include "lib/framework/wzprof.h"

/* Which client arrays are on: bit 0 vertex, 1 colour, 2 texcoord, 3 normal */
static unsigned int clientArrays = 0;

static unsigned int clientArrayBit(GLenum array)
{
    switch (array) {
        case GL_VERTEX_ARRAY:        return 1;
        case GL_COLOR_ARRAY:         return 2;
        case GL_TEXTURE_COORD_ARRAY: return 4;
        case GL_NORMAL_ARRAY:        return 8;
        default:                     return 0;
    }
}

void glesEnableClientState(GLenum array)
{
    unsigned int bit = clientArrayBit(array);
    if (bit && (clientArrays & bit)) return;
    clientArrays |= bit;
    glEnableClientState(array);
}

void glesDisableClientState(GLenum array)
{
    unsigned int bit = clientArrayBit(array);
    if (bit && !(clientArrays & bit)) return;
    clientArrays &= ~bit;
    glDisableClientState(array);
}

/* Turns each client array on or off to match the batch about to be drawn */
static void glesClientArray(GLenum array, int on)
{
    if (on) {
        glesEnableClientState(array);
    } else {
        glesDisableClientState(array);
    }
}

/*
 * Draw call merging
 *
 * The TouchPad's driver spends about 50us of CPU and GPU time on every draw
 * call however small, so drawing each glBegin/glEnd and each model on its own
 * (some 300 a frame) cost more than anything else. Instead their triangles
 * collect in one batch: the vertices are moved into eye space here, by the
 * modelview matrix of the moment, and the batch is drawn in one call when
 * something that affects drawing is about to change.
 *
 * Every GL state change the game makes goes through a wrapper below that
 * draws the batch first, and changes that change nothing are dropped so they
 * do not break batches. Colour, texcoords and the modelview matrix need no
 * wrapper: the batch carries its own colours and coordinates and is drawn
 * with an identity modelview.
 */
#define BATCH_VERTICES 8192
#define BATCH_INDICES (BATCH_VERTICES * 3)
static GLfloat batchPos[BATCH_VERTICES * 3];
static GLfloat batchTex[BATCH_VERTICES * 2];
static GLubyte batchCol[BATCH_VERTICES * 4];
static GLushort batchIdx[BATCH_INDICES];
static int batchVerts = 0, batchIndices = 0;
static GLenum matrixMode = GL_MODELVIEW;
static GLfloat texScaleU = 1, texScaleV = 1, texOffsetU = 0, texOffsetV = 0;

void glesSetTexTransform(GLfloat scaleU, GLfloat scaleV, GLfloat offsetU, GLfloat offsetV)
{
    texScaleU = scaleU;
    texScaleV = scaleV;
    texOffsetU = offsetU;
    texOffsetV = offsetV;
}

static void countedDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    wzProfDrawCalls++;
    WZPROF_BEGIN(PROF_DRAWCALL);
    glDrawElements(mode, count, type, indices);
    WZPROF_END(PROF_DRAWCALL);
}

/* Why batches get drawn, counted while profiling (glesProfReport) */
#define MAX_REASONS 32
static struct { const char *what; int arg; unsigned int count; } reasons[MAX_REASONS];
static int nReasons = 0;

static void flushReason(const char *what, int arg)
{
    int i;

    for (i = 0; i < nReasons; i++) {
        if (reasons[i].what == what && reasons[i].arg == arg) {
            reasons[i].count++;
            return;
        }
    }
    if (nReasons < MAX_REASONS) {
        reasons[nReasons].what = what;
        reasons[nReasons].arg = arg;
        reasons[nReasons].count = 1;
        nReasons++;
    }
}

/* Draws the batch, noting why when profiling and there is something to draw */
#define FLUSH_FOR(what, arg) do { \
        if (batchIndices && wzProfOn > 0) flushReason(what, (int)(arg)); \
        glesFlushBatch(); \
    } while (0)

void glesProfReport(unsigned int frames)
{
    int i, j;

    /* the most common first */
    for (i = 0; i < nReasons; i++) {
        for (j = i + 1; j < nReasons; j++) {
            if (reasons[j].count > reasons[i].count) {
                const char *w = reasons[i].what; int a = reasons[i].arg; unsigned int c = reasons[i].count;
                reasons[i] = reasons[j];
                reasons[j].what = w; reasons[j].arg = a; reasons[j].count = c;
            }
        }
    }
    fprintf(stderr, "flushes:");
    for (i = 0; i < nReasons && i < 10; i++) {
        fprintf(stderr, " %s(0x%x) %.1f", reasons[i].what, reasons[i].arg, reasons[i].count / (float)frames);
    }
    fprintf(stderr, "\n");
    nReasons = 0;
}

void glesFlushBatch(void)
{
    if (batchIndices == 0) {
        batchVerts = 0;
        return;
    }
    glesClientArray(GL_VERTEX_ARRAY, 1);
    glesClientArray(GL_COLOR_ARRAY, 1);
    glesClientArray(GL_TEXTURE_COORD_ARRAY, 1);
    glesClientArray(GL_NORMAL_ARRAY, 0);
    glVertexPointer(3, GL_FLOAT, 0, batchPos);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, batchCol);
    glTexCoordPointer(2, GL_FLOAT, 0, batchTex);

    if (matrixMode != GL_MODELVIEW) {
        glMatrixMode(GL_MODELVIEW);
    }
    glPushMatrix();
    glLoadIdentity();
    countedDrawElements(GL_TRIANGLES, batchIndices, GL_UNSIGNED_SHORT, batchIdx);
    glPopMatrix();
    if (matrixMode != GL_MODELVIEW) {
        glMatrixMode(matrixMode);
    }
    batchVerts = 0;
    batchIndices = 0;

    /* Drawing with a colour array leaves the current colour undefined; later
     * draws without one expect the colour last set */
    glColor4ub(g_imm.currentColor[0], g_imm.currentColor[1], g_imm.currentColor[2], g_imm.currentColor[3]);
}

/* Triangles that n vertices make in the given mode, or 0 if it is not one we batch */
static int batchTriangles(GLenum mode, int n)
{
    switch (mode) {
        case GL_TRIANGLES:      return n / 3;
        case GL_QUADS:          return n / 4 * 2;
        case GL_TRIANGLE_STRIP:
        case GL_QUAD_STRIP:
        case GL_TRIANGLE_FAN:   return n >= 3 ? n - 2 : 0;
        default:                return 0;
    }
}

/* Adds n vertices in object space to the batch, with texcoords (or none, as
 * 0,0, which is what GL would use) and either a colour per vertex or one for all */
static void batchAppend(GLenum mode, int n, const GLfloat *pos, const GLfloat *tex,
                        const GLubyte *colours, const GLubyte *colour)
{
    const int tris = batchTriangles(mode, n);
    GLfloat m[16];
    GLushort base;
    int i, k;

    if (tris == 0) {
        return;
    }
    if (batchVerts + n > BATCH_VERTICES || batchIndices + tris * 3 > BATCH_INDICES) {
        FLUSH_FOR("batch full", 0);
    }
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    base = batchVerts;

    for (i = 0; i < n; i++) {
        const GLfloat x = pos[i * 3 + 0], y = pos[i * 3 + 1], z = pos[i * 3 + 2];
        GLfloat *p = &batchPos[(base + i) * 3];
        GLubyte *c = &batchCol[(base + i) * 4];
        const GLubyte *src = colours ? &colours[i * 4] : colour;

        p[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
        p[1] = m[1] * x + m[5] * y + m[9] * z + m[13];
        p[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
        if (tex) {
            batchTex[(base + i) * 2 + 0] = tex[i * 2 + 0] * texScaleU + texOffsetU;
            batchTex[(base + i) * 2 + 1] = tex[i * 2 + 1] * texScaleV + texOffsetV;
        } else {
            batchTex[(base + i) * 2 + 0] = texOffsetU;
            batchTex[(base + i) * 2 + 1] = texOffsetV;
        }
        c[0] = src[0];
        c[1] = src[1];
        c[2] = src[2];
        c[3] = src[3];
    }

    for (k = 0; k < tris; k++) {
        GLushort *idx = &batchIdx[batchIndices + k * 3];

        switch (mode) {
            case GL_TRIANGLES:
                idx[0] = base + k * 3; idx[1] = base + k * 3 + 1; idx[2] = base + k * 3 + 2;
                break;
            case GL_QUADS: {
                const int q = (k / 2) * 4;
                if (k & 1) {
                    idx[0] = base + q; idx[1] = base + q + 2; idx[2] = base + q + 3;
                } else {
                    idx[0] = base + q; idx[1] = base + q + 1; idx[2] = base + q + 2;
                }
                break;
            }
            case GL_TRIANGLE_FAN:
                idx[0] = base; idx[1] = base + k + 1; idx[2] = base + k + 2;
                break;
            default: /* strips; every other triangle swaps two corners to keep the winding */
                if (k & 1) {
                    idx[0] = base + k + 1; idx[1] = base + k; idx[2] = base + k + 2;
                } else {
                    idx[0] = base + k; idx[1] = base + k + 1; idx[2] = base + k + 2;
                }
                break;
        }
    }
    batchVerts += n;
    batchIndices += tris * 3;
}

void glesBatchTriangles(int n, const GLfloat *pos, const GLfloat *tex, const GLubyte colour[4])
{
    /* in pieces that fit a batch, whole triangles each */
    const int piece = BATCH_VERTICES / 3 * 3;

    while (n > 0) {
        const int now = n < piece ? n : piece;

        batchAppend(GL_TRIANGLES, now, pos, tex, NULL, colour);
        pos += now * 3;
        tex += now * 2;
        n -= now;
    }
}

void glesDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    FLUSH_FOR("glDrawArrays", 0);
    wzProfDrawCalls++;
    WZPROF_BEGIN(PROF_DRAWCALL);
    glDrawArrays(mode, first, count);
    WZPROF_END(PROF_DRAWCALL);
}

void glesDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    FLUSH_FOR("glDrawElements", 0);
    countedDrawElements(mode, count, type, indices);
}

/* Enable state of the capabilities the game uses: -1 not known yet */
static struct { GLenum cap; signed char on; } caps[] = {
    {GL_TEXTURE_2D, -1}, {GL_BLEND, -1}, {GL_ALPHA_TEST, -1}, {GL_DEPTH_TEST, -1},
    {GL_CULL_FACE, -1}, {GL_FOG, -1}, {GL_LIGHTING, -1}, {GL_STENCIL_TEST, -1},
    {GL_NORMALIZE, -1}, {GL_POLYGON_OFFSET_FILL, -1}, {GL_SCISSOR_TEST, -1},
    {GL_LIGHT0, -1}, {GL_COLOR_MATERIAL, -1},
};

static signed char *capState(GLenum cap)
{
    unsigned int i;

    for (i = 0; i < sizeof(caps) / sizeof(caps[0]); i++) {
        if (caps[i].cap == cap) {
            return &caps[i].on;
        }
    }
    return NULL;
}

void glesEnable(GLenum cap)
{
    signed char *on = capState(cap);

    if (on && *on == 1) return;
    FLUSH_FOR("glEnable", cap);
    glEnable(cap);
    if (on) *on = 1;
}

void glesDisable(GLenum cap)
{
    signed char *on = capState(cap);

    if (on && *on == 0) return;
    FLUSH_FOR("glDisable", cap);
    glDisable(cap);
    if (on) *on = 0;
}

GLboolean glesIsEnabled(GLenum cap)
{
    signed char *on = capState(cap);

    if (on && *on >= 0) return *on ? GL_TRUE : GL_FALSE;
    return glIsEnabled(cap);
}

static GLuint boundTexture = 0;
static int boundTextureKnown = 0;

void glesBindTexture(GLenum target, GLuint texture)
{
    if (boundTextureKnown && texture == boundTexture) return;
    FLUSH_FOR("glBindTexture", 0);
    glBindTexture(target, texture);
    boundTexture = texture;
    boundTextureKnown = 1;
}

void glesDeleteTextures(GLsizei n, const GLuint *textures)
{
    FLUSH_FOR("glDeleteTextures", 0);
    glDeleteTextures(n, textures);
    boundTextureKnown = 0;	/* the name may come back for a new texture */
}

/* Wrappers that keep the last value set and skip repeats */
#define CACHED2(name, glname, T1, T2) \
    void name(T1 a, T2 b) { \
        static int known = 0; static T1 lastA; static T2 lastB; \
        if (known && a == lastA && b == lastB) return; \
        FLUSH_FOR(#glname, 0); glname(a, b); known = 1; lastA = a; lastB = b; }
#define CACHED1(name, glname, T1) \
    void name(T1 a) { \
        static int known = 0; static T1 lastA; \
        if (known && a == lastA) return; \
        FLUSH_FOR(#glname, 0); glname(a); known = 1; lastA = a; }

CACHED2(glesBlendFunc, glBlendFunc, GLenum, GLenum)
CACHED1(glesDepthMask, glDepthMask, GLboolean)
CACHED1(glesDepthFunc, glDepthFunc, GLenum)
CACHED2(glesAlphaFunc, glAlphaFunc, GLenum, GLclampf)
CACHED1(glesCullFace, glCullFace, GLenum)
CACHED2(glesDepthRangef, glDepthRangef, GLclampf, GLclampf)
CACHED2(glesPolygonOffset, glPolygonOffset, GLfloat, GLfloat)
#undef CACHED1
#undef CACHED2

/* Wrappers that always draw the batch first; these are rare */
void glesFrontFace(GLenum mode) { FLUSH_FOR("glFrontFace", 0); glFrontFace(mode); }
void glesColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { FLUSH_FOR("glColorMask", 0); glColorMask(r, g, b, a); }
void glesTexParameteri(GLenum target, GLenum pname, GLint param) { FLUSH_FOR("glTexParameteri", 0); glTexParameteri(target, pname, param); }
void glesTexParameterf(GLenum target, GLenum pname, GLfloat param) { FLUSH_FOR("glTexParameterf", 0); glTexParameterf(target, pname, param); }
void glesTexEnvf(GLenum target, GLenum pname, GLfloat param) { FLUSH_FOR("glTexEnvf", 0); glTexEnvf(target, pname, param); }
void glesTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                    GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
    FLUSH_FOR("glTexImage2D", 0);
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}
void glesTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                       GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    FLUSH_FOR("glTexSubImage2D", 0);
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}
void glesClear(GLbitfield mask) { FLUSH_FOR("glClear", 0); glClear(mask); }
void glesFinish(void) { FLUSH_FOR("glFinish", 0); glFinish(); }
void glesFlushGL(void) { FLUSH_FOR("glFlush", 0); glFlush(); }
void glesReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
    FLUSH_FOR("glReadPixels", 0);
    glReadPixels(x, y, width, height, format, type, pixels);
}
void glesScissor(GLint x, GLint y, GLsizei width, GLsizei height) { FLUSH_FOR("glScissor", 0); glScissor(x, y, width, height); }
void glesViewport(GLint x, GLint y, GLsizei width, GLsizei height) { FLUSH_FOR("glViewport", 0); glViewport(x, y, width, height); }
void glesFogf(GLenum pname, GLfloat param) { FLUSH_FOR("glFogf", 0); glFogf(pname, param); }
void glesFogfv(GLenum pname, const GLfloat *params) { FLUSH_FOR("glFogfv", 0); glFogfv(pname, params); }
void glesLightfv(GLenum light, GLenum pname, const GLfloat *params) { FLUSH_FOR("glLightfv", 0); glLightfv(light, pname, params); }
void glesLightModelf(GLenum pname, GLfloat param) { FLUSH_FOR("glLightModelf", 0); glLightModelf(pname, param); }
void glesLightModelfv(GLenum pname, const GLfloat *params) { FLUSH_FOR("glLightModelfv", 0); glLightModelfv(pname, params); }
void glesMaterialf(GLenum face, GLenum pname, GLfloat param) { FLUSH_FOR("glMaterialf", 0); glMaterialf(face, pname, param); }
void glesMaterialfv(GLenum face, GLenum pname, const GLfloat *params) { FLUSH_FOR("glMaterialfv", 0); glMaterialfv(face, pname, params); }
void glesStencilFunc(GLenum func, GLint ref, GLuint mask) { FLUSH_FOR("glStencilFunc", 0); glStencilFunc(func, ref, mask); }
void glesStencilOp(GLenum fail, GLenum zfail, GLenum zpass) { FLUSH_FOR("glStencilOp", 0); glStencilOp(fail, zfail, zpass); }
void glesStencilMask(GLuint mask) { FLUSH_FOR("glStencilMask", 0); glStencilMask(mask); }
void glesHint(GLenum target, GLenum mode) { FLUSH_FOR("glHint", 0); glHint(target, mode); }

/* Matrix calls: only the projection and texture matrices affect the batch */
#define FLUSH_UNLESS_MODELVIEW() do { if (matrixMode != GL_MODELVIEW) FLUSH_FOR("matrix op", matrixMode); } while (0)

void glesMatrixMode(GLenum mode) { matrixMode = mode; glMatrixMode(mode); }
void glesLoadIdentity(void) { FLUSH_UNLESS_MODELVIEW(); glLoadIdentity(); }
void glesLoadMatrixf(const GLfloat *m) { FLUSH_UNLESS_MODELVIEW(); glLoadMatrixf(m); }
void glesMultMatrixf(const GLfloat *m) { FLUSH_UNLESS_MODELVIEW(); glMultMatrixf(m); }
void glesPushMatrix(void) { FLUSH_UNLESS_MODELVIEW(); glPushMatrix(); }
void glesPopMatrix(void) { FLUSH_UNLESS_MODELVIEW(); glPopMatrix(); }
void glesTranslatef(GLfloat x, GLfloat y, GLfloat z) { FLUSH_UNLESS_MODELVIEW(); glTranslatef(x, y, z); }
void glesRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) { FLUSH_UNLESS_MODELVIEW(); glRotatef(angle, x, y, z); }
void glesScalef(GLfloat x, GLfloat y, GLfloat z) { FLUSH_UNLESS_MODELVIEW(); glScalef(x, y, z); }
void glesOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) { FLUSH_UNLESS_MODELVIEW(); glOrthof(l, r, b, t, n, f); }
void glesFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) { FLUSH_UNLESS_MODELVIEW(); glFrustumf(l, r, b, t, n, f); }
#undef FLUSH_UNLESS_MODELVIEW

/* Triangle indices for GL_QUADS, the same for every batch, built once */
static GLushort quadIndices[GLES_MAX_VERTICES / 4 * 6];

/* Global immediate mode buffer */
GLESImmediateBuffer g_imm;

/* Attribute stack for glPushAttrib/glPopAttrib emulation */
GLESAttribState g_attribStack[GLES_ATTRIB_STACK_DEPTH];
int g_attribStackDepth = 0;

void glesCompatInit(void)
{
    int q;

    for (q = 0; q < GLES_MAX_VERTICES / 4; q++) {
        quadIndices[q * 6 + 0] = q * 4 + 0;
        quadIndices[q * 6 + 1] = q * 4 + 1;
        quadIndices[q * 6 + 2] = q * 4 + 2;
        quadIndices[q * 6 + 3] = q * 4 + 0;
        quadIndices[q * 6 + 4] = q * 4 + 2;
        quadIndices[q * 6 + 5] = q * 4 + 3;
    }

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

    /* Point each array at the batch; arrays this batch lacks are switched off,
     * and the GL current colour/texcoord/normal apply instead */
    glesClientArray(GL_VERTEX_ARRAY, 1);
    glVertexPointer(3, GL_FLOAT, 0, g_imm.position);
    glesClientArray(GL_COLOR_ARRAY, g_imm.hasColor);
    if (g_imm.hasColor) {
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, g_imm.color);
    }
    glesClientArray(GL_TEXTURE_COORD_ARRAY, g_imm.hasTexCoord);
    if (g_imm.hasTexCoord) {
        int i;

        for (i = 0; i < g_imm.count; i++) {
            g_imm.texcoord[i * 2 + 0] = g_imm.texcoord[i * 2 + 0] * texScaleU + texOffsetU;
            g_imm.texcoord[i * 2 + 1] = g_imm.texcoord[i * 2 + 1] * texScaleV + texOffsetV;
        }
        glTexCoordPointer(2, GL_FLOAT, 0, g_imm.texcoord);
    }
    glesClientArray(GL_NORMAL_ARRAY, g_imm.hasNormal);
    if (g_imm.hasNormal) {
        glNormalPointer(GL_FLOAT, 0, g_imm.normal);
    }

    /* Handle GL_QUADS by converting to triangles: 0,1,2 and 0,2,3 for each quad */
    if (g_imm.mode == GL_QUADS) {
        glesDrawElements(GL_TRIANGLES, g_imm.count / 4 * 6, GL_UNSIGNED_SHORT, quadIndices);
    }
    /* Handle GL_QUAD_STRIP by converting to triangle strip */
    else if (g_imm.mode == GL_QUAD_STRIP) {
        /* Quad strip can be drawn as triangle strip with same vertices */
        glesDrawArrays(GL_TRIANGLE_STRIP, 0, g_imm.count);
    }
    else {
        /* Draw normally for supported primitives */
        glesDrawArrays(g_imm.mode, 0, g_imm.count);
    }
}

void glesEnd(void)
{
    /* Triangles join the batch; lines, points and lit (normal-carrying)
     * primitives are drawn now, after it */
    if (!g_imm.hasNormal && batchTriangles(g_imm.mode, g_imm.count) > 0) {
        batchAppend(g_imm.mode, g_imm.count, g_imm.position,
                    g_imm.hasTexCoord ? g_imm.texcoord : NULL, g_imm.color, NULL);
    } else {
        FLUSH_FOR("line or lit primitive", g_imm.mode);
        glesFlush();
    }
}

void glesVertex2f(GLfloat x, GLfloat y)
{
    int i;
    if (g_imm.count >= GLES_MAX_VERTICES) return;

    i = g_imm.count;
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
    int i;
    if (g_imm.count >= GLES_MAX_VERTICES) return;

    i = g_imm.count;
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

void glesColor4fv(const GLfloat *v)
{
    glesColor4f(v[0], v[1], v[2], v[3]);
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
    GLESAttribState *state;
    (void)mask; /* unused for now */

    if (g_attribStackDepth >= GLES_ATTRIB_STACK_DEPTH) {
        return; /* Stack overflow - silently ignore */
    }

    state = &g_attribStack[g_attribStackDepth];

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
    GLESAttribState *state;

    if (g_attribStackDepth <= 0) {
        return; /* Stack underflow - silently ignore */
    }

    g_attribStackDepth--;
    state = &g_attribStack[g_attribStackDepth];

    /* Restore state */
    glesDepthMask(state->depthMask);

    if (state->fogEnabled) {
        glesEnable(GL_FOG);
    } else {
        glesDisable(GL_FOG);
    }

    if (state->blendEnabled) {
        glesEnable(GL_BLEND);
    } else {
        glesDisable(GL_BLEND);
    }

    if (state->alphaTestEnabled) {
        glesEnable(GL_ALPHA_TEST);
    } else {
        glesDisable(GL_ALPHA_TEST);
    }

    glesBlendFunc(state->blendSrc, state->blendDst);
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
    /*
     * GLES 1.1: Just upload the base texture level.
     * We use GL_LINEAR filtering instead of mipmaps for simplicity and reliability.
     * GL_GENERATE_MIPMAP can be unreliable on some GLES implementations.
     */
    glesTexImage2D(target, 0, internalformat, width, height, 0, format, type, data);
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
