/*****************************************************************************
 * ==> Plasma effect demo ---------------------------------------------------*
 *****************************************************************************
 * Description : An old school plasma effect that use shader for calculation *
 * Developer   : Jean-Milost Reymond                                         *
 * Copyright   : 2015 - 2018, this file is part of the Minimal API. You are  *
 *               free to copy or redistribute this file, modify it, or use   *
 *               it for your own projects, commercial or not. This file is   *
 *               provided "as is", without ANY WARRANTY OF ANY KIND          *
 *****************************************************************************/

// supported platforms check. NOTE iOS only, but may works on other platforms
#if !defined(_OS_IOS_) && !defined(_OS_ANDROID_) && !defined(_OS_WINDOWS_)
    #error "Not supported platform!"
#endif

#ifdef CCR_FORCE_LLVM_INTERPRETER
    #error "Clang/LLVM on iOS does not support function pointer yet. Consider using CPP built-in compiler."
#endif

// std
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// opengl
#include <gles2.h>
#include <gles2ext.h>

// mini API
#include "MiniAPI/MiniCommon.h"
#include "MiniAPI/MiniGeometry.h"
#include "MiniAPI/MiniVertex.h"
#include "MiniAPI/MiniShapes.h"
#include "MiniAPI/MiniShader.h"
#include "MiniAPI/MiniRenderer.h"

#if __CCR__ > 2 || (__CCR__ == 2 && (__CCR_MINOR__ > 2 || ( __CCR_MINOR__ == 2 && __CCR_PATCHLEVEL__ >= 1)))
    #include <ccr.h>
#endif

//------------------------------------------------------------------------------
// renderer buffers should no more be generated since CCR version 1.1
#if ((__CCR__ < 1) || ((__CCR__ == 1) && (__CCR_MINOR__ < 1)))
    #ifndef _OS_ANDROID_
        GLuint g_Renderbuffer, g_Framebuffer;
    #endif
#endif
MINI_Shader       g_Shader;
GLuint            g_ShaderProgram      = 0;
float*            g_pSurfaceVB         = 0;
unsigned int      g_SurfaceVertexCount = 0;
const float       g_SurfaceWidth       = 10.0f;
const float       g_SurfaceHeight      = 12.5f;
const float       g_MaxTime            = 12.0f * M_PI;
float             g_Time               = 0.0f;
GLuint            g_TimeSlot           = 0;
GLuint            g_SizeSlot           = 0;
MINI_VertexFormat g_VertexFormat;
//------------------------------------------------------------------------------
// plasma vertex shader program
const char* g_pVSPlasma =
    "precision mediump float;"
    "attribute vec3  mini_vPosition;"
    "uniform   float mini_uTime;"
    "uniform   vec2  mini_uSize;"
    "varying   float mini_fTime;"
    "varying   vec2  mini_fSize;"
    "void main(void)"
    "{"
    "    mini_fSize  = mini_uSize;"
    "    mini_fTime  = mini_uTime;"
    "    gl_Position = vec4(mini_vPosition, 1.0);"
    "}";
//------------------------------------------------------------------------------
// plasma fragment shader program. NOTE the above used plasma algorithm is based on Bidouille.org
// oldscool plasma effect post, available here:
// http://www.bidouille.org/prog/plasma
const char* g_pFSPlasma =
    "precision mediump float;"
    "varying float mini_fTime;"
    "varying vec2  mini_fSize;"
    "void main(void)"
    "{"
    "    const float pi      = 3.1415926535897932384626433832795;"
    "    vec2        vK      = vec2(0.34, 0.25);"
    "    vec2        vCoords = vec2((gl_FragCoord.x / mini_fSize.x) * 100.0,"
    "                               (gl_FragCoord.y / mini_fSize.y) * 100.0);"
    "    float v             = 0.0;"
    "    vec2  c             = vCoords * (vK - (vK / 2.0));"
    "    v                  += sin((c.x + mini_fTime));"
    "    v                  += sin((c.y + mini_fTime) / 2.0);"
    "    v                  += sin((c.x + c.y + mini_fTime) / 2.0);"
    "    c                  += vK / 2.0 * vec2(sin(mini_fTime / 3.0), cos(mini_fTime / 2.0));"
    "    v                  += sin(sqrt(c.x * c.x + c.y * c.y + 1.0) + mini_fTime);"
    "    v                   = v / 2.0;"
    "    vec3  col           = vec3(1, sin(pi * v), cos(pi * v));"
    "    gl_FragColor        = vec4(col * 0.5 + 0.5, 1);"
    "}";
//------------------------------------------------------------------------------
void on_GLES2_Init(int view_w, int view_h)
{
    // renderer buffers should no more be generated since CCR version 1.1
    #if ((__CCR__ < 1) || ((__CCR__ == 1) && (__CCR_MINOR__ < 1)))
        #ifndef _OS_ANDROID_
            // generate and bind in memory frame buffers to render to
            glGenRenderbuffers(1, &g_Renderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, g_Renderbuffer);
            glGenFramebuffers(1,&g_Framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, g_Framebuffer);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                      GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER,
                                      g_Renderbuffer);
        #endif
    #endif

    // compile, link and use shader
    g_ShaderProgram = miniCompileShaders(g_pVSPlasma, g_pFSPlasma);
    glUseProgram(g_ShaderProgram);

    // get shader attributes
    g_Shader.m_VertexSlot = glGetAttribLocation(g_ShaderProgram,  "mini_vPosition");
    g_TimeSlot            = glGetUniformLocation(g_ShaderProgram, "mini_uTime");
    g_SizeSlot            = glGetUniformLocation(g_ShaderProgram, "mini_uSize");

    // configure OpenGL depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDepthRangef(0.0f, 1.0f);

    // enable culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);

    g_VertexFormat.m_UseNormals  = 0;
    g_VertexFormat.m_UseTextures = 0;
    g_VertexFormat.m_UseColors   = 0;

    // generate surface
    miniCreateSurface(&g_SurfaceWidth,
                      &g_SurfaceHeight,
                      0xFFFFFFFF,
                      &g_VertexFormat,
                      &g_pSurfaceVB,
                      &g_SurfaceVertexCount);
}
//------------------------------------------------------------------------------
void on_GLES2_Final()
{
    // delete surface vertices
    if (g_pSurfaceVB)
    {
        free(g_pSurfaceVB);
        g_pSurfaceVB = 0;
    }

    // delete shader program
    if (g_ShaderProgram)
        glDeleteProgram(g_ShaderProgram);

    g_ShaderProgram = 0;
}
//------------------------------------------------------------------------------
void on_GLES2_Size(int view_w, int view_h)
{
    glViewport(0, 0, view_w, view_h);

    // notify shader about screen size
    glUniform2f(g_SizeSlot, view_w, view_h);
}
//------------------------------------------------------------------------------
void on_GLES2_Update(float timeStep_sec)
{
    // calculate next time
    g_Time += timeStep_sec * 5.0f;

    // is time out of bounds?
    if (g_Time > g_MaxTime)
        g_Time -= g_MaxTime;

    // notify shader about elapsed time
    glUniform1f(g_TimeSlot, g_Time);
}
//------------------------------------------------------------------------------
void on_GLES2_Render()
{
    miniBeginScene(0.0f, 0.0f, 0.0f, 1.0f);

    // draw the plasma
    miniDrawSurface(g_pSurfaceVB,
                    g_SurfaceVertexCount,
                    &g_VertexFormat,
                    &g_Shader);

    miniEndScene();
}
//------------------------------------------------------------------------------
void on_GLES2_TouchBegin(float x, float y)
{}
//------------------------------------------------------------------------------
void on_GLES2_TouchEnd(float x, float y)
{}
//------------------------------------------------------------------------------
void on_GLES2_TouchMove(float prev_x, float prev_y, float x, float y)
{}
//------------------------------------------------------------------------------
#if __CCR__ > 2 || (__CCR__ == 2 && (__CCR_MINOR__ > 2 || ( __CCR_MINOR__ == 2 && __CCR_PATCHLEVEL__ >= 1)))
    int main()
    {
        ccrSet_GLES2_Init_Callback(on_GLES2_Init);
        ccrSet_GLES2_Final_Callback(on_GLES2_Final);
        ccrSet_GLES2_Size_Callback(on_GLES2_Size);
        ccrSet_GLES2_Update_Callback(on_GLES2_Update);
        ccrSet_GLES2_Render_Callback(on_GLES2_Render);
        ccrSet_GLES2_TouchBegin_Callback(on_GLES2_TouchBegin);
        ccrSet_GLES2_TouchMove_Callback(on_GLES2_TouchMove);
        ccrSet_GLES2_TouchEnd_Callback(on_GLES2_TouchEnd);

        ccrBegin_GLES2_Drawing();

        while (ccrGetEvent(false) != CCR_EVENT_QUIT);

        ccrEnd_GLES2_Drawing();

        return 0;
    }
#endif
//------------------------------------------------------------------------------
