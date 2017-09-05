/*****************************************************************************
 * ==> Earth demo -----------------------------------------------------------*
 *****************************************************************************
 * Description : A textured sphere representing the earth                    *
 * Developer   : Jean-Milost Reymond                                         *
 *****************************************************************************/

// supported platforms check (for now, only supports iOS and Android devices.
// NOTE Android support is theorical, never tested on a such device)
#if !defined(IOS) && !defined(ANDROID)
    #error "Not supported platform!"
#endif

// std
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef ANDROID
    #include <gles2/gl2.h>
    #include <gles2/gl2ext.h>
#endif
#ifdef IOS
    #include <OpenGLES/ES2/gl.h>
    #include <OpenGLES/ES2/glext.h>
#endif

// mini API
#include "MiniAPI/MiniGeometry.h"
#include "MiniAPI/MiniVertex.h"
#include "MiniAPI/MiniShapes.h"
#include "MiniAPI/MiniShader.h"

#ifdef ANDROID
    #define EARTH_TEXTURE_FILE "/sdcard/C++ Compiler/earthmap.bmp"
#else
    #define EARTH_TEXTURE_FILE "Resources/earthmap.bmp"
#endif

//------------------------------------------------------------------------------
// renderer buffers should no more be generated since CCR version 1.1
#if ((__CCR__ < 1) || ((__CCR__ == 1) && (__CCR_MINOR__ < 1)))
    #ifndef ANDROID
        GLuint g_Renderbuffer, g_Framebuffer;
    #endif
#endif
GLuint             g_ShaderProgram  = 0;
float*             g_pVertexBuffer  = 0;
int                g_VertexCount    = 0;
MV_Index*          g_pIndexes       = 0;
int                g_IndexCount     = 0;
float              g_Radius         = 5.0f;
float              g_Angle          = 0.0f;
float              g_RotationSpeed  = 0.1f;
float              g_Time           = 0.0f;
float              g_Interval       = 0.0f;
const unsigned int g_FPS            = 15;
GLuint             g_TextureIndex   = GL_INVALID_VALUE;
GLuint             g_PositionSlot   = 0;
GLuint             g_ColorSlot      = 0;
GLuint             g_TexCoordSlot   = 0;
GLuint             g_TexSamplerSlot = 0;
MV_VertexFormat    g_VertexFormat;
//------------------------------------------------------------------------------
void ApplyOrtho(float maxX, float maxY) const
{
    // get orthogonal matrix
    float left  = -5.0f;
    float right =  5.0f;
    float near  =  1.0f;
    float far   =  20.0f;

    // screen ratio was modified since CCR version 1.1
    #if ((__CCR__ < 1) || ((__CCR__ == 1) && (__CCR_MINOR__ < 1)))
        float bottom = -5.0f * 1.12f;
        float top    =  5.0f * 1.12f;
    #else
        float bottom = -5.0f * 1.24f;
        float top    =  5.0f * 1.24f;
    #endif

    MG_Matrix ortho;
    GetOrtho(&left, &right, &bottom, &top, &near, &far, &ortho);

    // connect projection matrix to shader
    GLint projectionUniform = glGetUniformLocation(g_ShaderProgram, "qr_uProjection");
    glUniformMatrix4fv(projectionUniform, 1, 0, &ortho.m_Table[0][0]);
}
//------------------------------------------------------------------------------
void on_GLES2_Init(int view_w, int view_h)
{
    // renderer buffers should no more be generated since CCR version 1.1
    #if ((__CCR__ < 1) || ((__CCR__ == 1) && (__CCR_MINOR__ < 1)))
        #ifndef ANDROID
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

    // compile, link and use shaders
    g_ShaderProgram = CompileShaders(g_pVSTextured, g_pFSTextured);
    glUseProgram(g_ShaderProgram);

    g_VertexFormat.m_UseNormals  = 0;
    g_VertexFormat.m_UseTextures = 1;
    g_VertexFormat.m_UseColors   = 1;

    // generate sphere
    CreateSphere(&g_Radius,
                 10,
                 24,
                 0xFFFFFFFF,
                 &g_VertexFormat,
                 &g_pVertexBuffer,
                 &g_VertexCount,
                 &g_pIndexes,
                 &g_IndexCount);

    // load earth texture
    g_TextureIndex = LoadTexture(EARTH_TEXTURE_FILE);

    // get shader attributes
    g_PositionSlot   = glGetAttribLocation(g_ShaderProgram, "qr_vPosition");
    g_ColorSlot      = glGetAttribLocation(g_ShaderProgram, "qr_vColor");
    g_TexCoordSlot   = glGetAttribLocation(g_ShaderProgram, "qr_vTexCoord");
    g_TexSamplerSlot = glGetAttribLocation(g_ShaderProgram, "qr_sColorMap");

    // configure OpenGL depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDepthRangef(0.0f, 1.0f);

    // enable culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);

    // calculate frame interval
    g_Interval = 1000.0f / g_FPS;
}
//------------------------------------------------------------------------------
void on_GLES2_Final()
{
    // delete buffer index table
    if (g_pIndexes)
    {
        free(g_pIndexes);
        g_pIndexes = 0;
    }

    // delete vertexes
    if (g_pVertexBuffer)
    {
        free(g_pVertexBuffer);
        g_pVertexBuffer = 0;
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
    ApplyOrtho(2.0f, 2.0f);
}
//------------------------------------------------------------------------------
void on_GLES2_Update(float timeStep_sec)
{
    unsigned int frameCount = 0;

    // calculate next time
    g_Time += (timeStep_sec * 1000.0f);

    // count frames to skip
    while (g_Time > g_Interval)
    {
        g_Time -= g_Interval;
        ++frameCount;
    }

    // calculate next rotation angle
    g_Angle += (g_RotationSpeed * frameCount);

    // is rotating angle out of bounds?
    while (g_Angle >= 6.28f)
        g_Angle -= 6.28f;
}
//------------------------------------------------------------------------------
void on_GLES2_Render()
{
    unsigned    i;
    unsigned    j;
    int         stride;
    float       xAngle;
    MG_Vector3  r;
    MG_Matrix   xRotateMatrix;
    MG_Matrix   yRotateMatrix;
    MG_Matrix   modelViewMatrix;
    GLvoid*     pCoords;
    GLvoid*     pTexCoords;
    GLvoid*     pColors;

    // clear scene background and depth buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // set rotation on X axis
    r.m_X = 1.0f;
    r.m_Y = 0.0f;
    r.m_Z = 0.0f;

    // rotate 90 degrees
    xAngle = 1.57075;

    // calculate model view matrix (it's a rotation on the y axis)
    GetRotateMatrix(&xAngle, &r, &xRotateMatrix);

    // set rotation on Y axis
    r.m_X = 0.0f;
    r.m_Y = 1.0f;
    r.m_Z = 0.0f;

    // calculate model view matrix (it's a rotation on the y axis)
    GetRotateMatrix(&g_Angle, &r, &yRotateMatrix);

    // build model view matrix
    MatrixMultiply(&xRotateMatrix, &yRotateMatrix, &modelViewMatrix);

    // connect model view matrix to shader
    GLint modelviewUniform = glGetUniformLocation(g_ShaderProgram, "qr_uModelview");
    glUniformMatrix4fv(modelviewUniform, 1, 0, &modelViewMatrix.m_Table[0][0]);

    // configure texture to draw
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_TexSamplerSlot, GL_TEXTURE0);

    // calculate vertex stride
    stride = g_VertexFormat.m_Stride;

    // enable position and color slots
    glEnableVertexAttribArray(g_PositionSlot);
    glEnableVertexAttribArray(g_TexCoordSlot);
    glEnableVertexAttribArray(g_ColorSlot);

    // iterate through vertex fan buffers to draw
    for (int i = 0; i < g_IndexCount; ++i)
    {
        // get next vertexes fan buffer
        pCoords    = &g_pVertexBuffer[g_pIndexes[i].m_Start];
        pTexCoords = &g_pVertexBuffer[g_pIndexes[i].m_Start + 3];
        pColors    = &g_pVertexBuffer[g_pIndexes[i].m_Start + 5];

        // connect buffer to shader
        glVertexAttribPointer(g_PositionSlot, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), pCoords);
        glVertexAttribPointer(g_TexCoordSlot, 2, GL_FLOAT, GL_FALSE, stride * sizeof(float), pTexCoords);
        glVertexAttribPointer(g_ColorSlot,    4, GL_FLOAT, GL_FALSE, stride * sizeof(float), pColors);

        // draw it
        glDrawArrays(GL_TRIANGLE_STRIP, 0, g_pIndexes[i].m_Length / stride);
    }

    // disconnect slots from shader
    glDisableVertexAttribArray(g_PositionSlot);
    glDisableVertexAttribArray(g_TexCoordSlot);
    glDisableVertexAttribArray(g_ColorSlot);
}
//------------------------------------------------------------------------------
void on_GLES2_TouchBegin(float x, float y)
{}
//------------------------------------------------------------------------------
void on_GLES2_TouchEnd(float x, float y)
{}
//------------------------------------------------------------------------------
void on_GLES2_TouchMove(float prev_x, float prev_y, float x, float y)
{
    // increase or decrease rotation speed
    g_RotationSpeed += (x - prev_x) * -0.001f;
}
//------------------------------------------------------------------------------
#ifdef IOS
    void on_GLES2_DeviceRotate(int orientation)
    {}
#endif
//------------------------------------------------------------------------------
