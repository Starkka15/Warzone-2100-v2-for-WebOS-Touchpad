/*
 * GLES 1.1 Compatibility Layer for Warzone 2100
 * Replaces immediate mode (glBegin/glEnd) with vertex arrays
 */

#ifndef GLES_COMPAT_H
#define GLES_COMPAT_H

#ifdef USE_GLES
#include <GLES/gl.h>
#else
#include <GL/gl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum vertices per immediate mode batch */
#define GLES_MAX_VERTICES 64

/* Vertex buffer for immediate mode emulation */
typedef struct {
    GLfloat position[GLES_MAX_VERTICES * 3];  /* x, y, z */
    GLfloat texcoord[GLES_MAX_VERTICES * 2];  /* u, v */
    GLubyte color[GLES_MAX_VERTICES * 4];     /* r, g, b, a */
    GLfloat normal[GLES_MAX_VERTICES * 3];    /* nx, ny, nz */
    int count;
    GLenum mode;
    int hasColor;
    int hasTexCoord;
    int hasNormal;
    GLubyte currentColor[4];
    GLfloat currentTexCoord[2];
    GLfloat currentNormal[3];
} GLESImmediateBuffer;

/* Global immediate mode buffer */
extern GLESImmediateBuffer g_imm;

/* Initialize the compatibility layer */
void glesCompatInit(void);

/* Immediate mode emulation */
void glesBegin(GLenum mode);
void glesEnd(void);

void glesVertex2f(GLfloat x, GLfloat y);
void glesVertex2i(GLint x, GLint y);
void glesVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glesVertex3fv(const GLfloat *v);

void glesColor3f(GLfloat r, GLfloat g, GLfloat b);
void glesColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glesColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void glesColor4ubv(const GLubyte *v);

void glesTexCoord2f(GLfloat u, GLfloat v);
void glesTexCoord2fv(const GLfloat *v);

void glesNormal3fv(const GLfloat *v);

/* Attribute stack emulation for glPushAttrib/glPopAttrib */
#define GLES_ATTRIB_STACK_DEPTH 16

typedef struct {
    GLboolean depthMask;
    GLboolean fogEnabled;
    GLboolean blendEnabled;
    GLboolean alphaTestEnabled;
    GLenum blendSrc;
    GLenum blendDst;
} GLESAttribState;

extern GLESAttribState g_attribStack[GLES_ATTRIB_STACK_DEPTH];
extern int g_attribStackDepth;

void glesPushAttrib(GLbitfield mask);
void glesPopAttrib(void);

/* Macros to replace GL calls */
#ifdef USE_GLES
#define glBegin(m)      glesBegin(m)
#define glEnd()         glesEnd()
#define glVertex2f      glesVertex2f
#define glVertex2i      glesVertex2i
#define glVertex3f      glesVertex3f
#define glVertex3fv     glesVertex3fv
#define glColor3f       glesColor3f
#define glColor4f       glesColor4f
#define glColor4ub      glesColor4ub
#define glColor4ubv     glesColor4ubv
#define glTexCoord2f    glesTexCoord2f
#define glTexCoord2fv   glesTexCoord2fv
#define glNormal3fv     glesNormal3fv

/* GL_QUADS not supported in GLES - will be converted to triangles */
#ifndef GL_QUADS
#define GL_QUADS 0x0007
#endif
#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP 0x0008
#endif

/* Functions that use double in GL but float in GLES */
#define glOrtho(l,r,b,t,n,f)      glOrthof((GLfloat)(l),(GLfloat)(r),(GLfloat)(b),(GLfloat)(t),(GLfloat)(n),(GLfloat)(f))
#define glFrustum(l,r,b,t,n,f)    glFrustumf((GLfloat)(l),(GLfloat)(r),(GLfloat)(b),(GLfloat)(t),(GLfloat)(n),(GLfloat)(f))
#define glDepthRange(n,f)         glDepthRangef((GLclampf)(n),(GLclampf)(f))
#define glClearDepth(d)           glClearDepthf((GLclampf)(d))

/* GL_CLAMP doesn't exist in GLES, use GL_CLAMP_TO_EDGE */
#ifdef GL_CLAMP
#undef GL_CLAMP
#endif
#define GL_CLAMP GL_CLAMP_TO_EDGE

/* GLee stubs - no extension loading needed for GLES */
#define GLEE_ARB_texture_compression 0
#define GLEE_EXT_texture_filter_anisotropic 0
#define GLEE_EXT_stencil_two_side 0
#define GLEE_ATI_separate_stencil 0
#define GLEE_EXT_stencil_wrap 0
#define GLEE_ARB_texture_rectangle 0
#define GLEE_VERSION_1_2 0
#define GLEE_VERSION_1_3 0
#define GLEE_VERSION_1_4 0
#define GLEE_VERSION_1_5 0
#define GLEE_VERSION_2_0 0
#define GLEE_VERSION_2_1 0
#define GLEE_VERSION_3_0 0

/* GLU function replacements */
const char* glesErrorString(GLenum error);
void glesBuild2DMipmaps(GLenum target, GLint internalformat, GLsizei width,
                        GLsizei height, GLenum format, GLenum type, const void *data);

#define gluErrorString(e)         glesErrorString(e)
#define gluBuild2DMipmaps(t,i,w,h,f,ty,d) glesBuild2DMipmaps(t,GL_RGBA,w,h,f,ty,d)

void glesScaleImage(GLenum format, GLsizei srcW, GLsizei srcH, GLenum srcType, const void *src,
                    GLsizei dstW, GLsizei dstH, GLenum dstType, void *dst);
#define gluScaleImage(f,sw,sh,st,s,dw,dh,dt,d) glesScaleImage(f,sw,sh,st,s,dw,dh,dt,d)

/* Texture compression not supported - use RGBA */
#define GL_COMPRESSED_RGBA_ARB GL_RGBA

/* Attribute stack emulation */
#define glPushAttrib(m)   glesPushAttrib(m)
#define glPopAttrib()     glesPopAttrib()

/* Attrib bit masks (not in GLES, only used by our emulation) */
#ifndef GL_DEPTH_BUFFER_BIT
#define GL_DEPTH_BUFFER_BIT 0x00000100
#endif
#ifndef GL_COLOR_BUFFER_BIT
#define GL_COLOR_BUFFER_BIT 0x00004000
#endif
#ifndef GL_ENABLE_BIT
#define GL_ENABLE_BIT 0x00002000
#endif
#ifndef GL_FOG_BIT
#define GL_FOG_BIT 0x00000080
#endif

/* glLightModeli doesn't exist in GLES 1.1, use float version */
#define glLightModeli(pname, param) glLightModelf(pname, (GLfloat)(param))

/* GL_LIGHT_MODEL_LOCAL_VIEWER doesn't exist in GLES 1.1 - stub it out */
#ifndef GL_LIGHT_MODEL_LOCAL_VIEWER
#define GL_LIGHT_MODEL_LOCAL_VIEWER 0x0B51
#endif

/* Stencil wrap extensions - use standard values or no-op */
#ifndef GL_INCR_WRAP_EXT
#define GL_INCR_WRAP_EXT GL_INCR
#endif
#ifndef GL_DECR_WRAP_EXT
#define GL_DECR_WRAP_EXT GL_DECR
#endif
#ifndef GL_INCR
#define GL_INCR 0x1E02
#endif
#ifndef GL_DECR
#define GL_DECR 0x1E03
#endif

/* Two-sided stencil not available in GLES 1.1 - stub out */
#ifndef GL_STENCIL_TEST_TWO_SIDE_EXT
#define GL_STENCIL_TEST_TWO_SIDE_EXT 0x8910
#endif
#define glActiveStencilFaceEXT(face) ((void)0)
#define glStencilOpSeparateATI(face, sfail, dpfail, dppass) ((void)0)

#endif /* USE_GLES */

#ifdef __cplusplus
}
#endif

#endif /* GLES_COMPAT_H */
