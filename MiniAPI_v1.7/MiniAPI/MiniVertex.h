/*****************************************************************************
 * ==> Minimal vertex library -----------------------------------------------*
 *****************************************************************************
 * Description : Minimal vertex library                                      *
 * Developer   : Jean-Milost Reymond                                         *
 * Copyright   : 2015 - 2017, this file is part of the Minimal API. You are  *
 *               free to copy or redistribute this file, modify it, or use   *
 *               it for your own projects, commercial or not                 *
 *****************************************************************************/

#ifndef MiniVertexH
#define MiniVertexH

// std
#include <stdio.h>
#include <stdlib.h>

// openGL
#ifdef ANDROID
    #include <gles2/gl2.h>
    #include <gles2/gl2ext.h>
#endif
#ifdef IOS
    #include <OpenGLES/ES2/gl.h>
    #include <OpenGLES/ES2/glext.h>
#endif
#ifdef __CODEGEARC__
    #include <Windows.h>
    #include <gl/gl.h>

    // missing in RAD studio OpenGL header
    #define GL_CLAMP_TO_EDGE 0x812F
#endif

// mini API
#include "MiniCommon.h"
#include "MiniGeometry.h"

//-----------------------------------------------------------------------------
// Structures
//-----------------------------------------------------------------------------

/**
* Vertex format
*/
typedef struct
{
    int      m_UseNormals;
    int      m_UseTextures;
    int      m_UseColors;
    unsigned m_Stride;
} MV_VertexFormat;

/**
* Vertex index
*/
typedef struct
{
    int m_Start;
    int m_Length;
} MV_Index;

//-----------------------------------------------------------------------------
// Vertex functions
//-----------------------------------------------------------------------------

/**
* Calculates vertex stride
*@param[in, out] pVertexFormat, vertex format for which stride should be calculated
*/
void CalculateStride(MV_VertexFormat* pVertexFormat)
{
    pVertexFormat->m_Stride = 3;

    // do use normals?
    if (pVertexFormat->m_UseNormals)
        pVertexFormat->m_Stride += 3;

    // do use textures?
    if (pVertexFormat->m_UseTextures)
        pVertexFormat->m_Stride += 2;

    // do use colors?
    if (pVertexFormat->m_UseColors)
        pVertexFormat->m_Stride += 4;
}

/**
* Adds a vertex to a vertex buffer
*@param pVertexFormat - vertex format
*@param pVertex - vertex to add
*@param pNormal - normal to add
*@param pUV - texture coordinate to add
*@param color - vertex color
*@param[out] pVB - vertex buffer to add to
*@param[out] pVertexCount - vertex buffer count
*/
void AddVertexToVB(MV_VertexFormat* pVertexFormat,
                   MG_Vector3*      pVertex,
                   MG_Vector3*      pNormal,
                   MG_Vector2*      pUV,
                   unsigned int     color,
                   float**          pVB,
                   unsigned int*    pVertexCount)
{
    unsigned int offset = *pVertexCount;

    // allocate memory for new vertex
    if (!(*pVertexCount))
        *pVB = (float*)malloc(pVertexFormat->m_Stride * sizeof(float));
    else
        *pVB = (float*)realloc(*pVB,
                             ((*pVertexCount) + pVertexFormat->m_Stride) * sizeof(float));

    // copy vertex from source
    (*pVB)[offset]     = pVertex->m_X;
    (*pVB)[offset + 1] = pVertex->m_Y;
    (*pVB)[offset + 2] = pVertex->m_Z;

    offset += 3;

    // do include normals?
    if (pVertexFormat->m_UseNormals)
    {
        // copy normal from source
        (*pVB)[offset]     = pNormal->m_X;
        (*pVB)[offset + 1] = pNormal->m_Y;
        (*pVB)[offset + 2] = pNormal->m_Z;

        offset += 3;
    }

    // do include texture coordinates?
    if (pVertexFormat->m_UseTextures)
    {
        // copy texture coordinates from source
        (*pVB)[offset]     = pUV->m_X;
        (*pVB)[offset + 1] = pUV->m_Y;

        offset += 2;
    }

    // do include colors?
    if (pVertexFormat->m_UseColors)
    {
        // set color data
        (*pVB)[offset]     = (float)((color >> 24) & 0xFF) / 255.0f;
        (*pVB)[offset + 1] = (float)((color >> 16) & 0xFF) / 255.0f;
        (*pVB)[offset + 2] = (float)((color >> 8)  & 0xFF) / 255.0f;
        (*pVB)[offset + 3] = (float) (color        & 0xFF) / 255.0f;
    }

    // update vertex count
    *pVertexCount += pVertexFormat->m_Stride;
}

/**
* Loads texture from bitmap file
*@param pFileName - bitmap file name to load from
*@return newly generated texture index, GL_INVALID_VALUE on error
*@note For now only most common bitmaps are loaded, some bitmap types may be unsupported
*/
GLuint LoadTexture(const unsigned char* pFileName)
{
    FILE*          pFile;
    GLuint         index;
    unsigned int   dataOffset;
    unsigned int   headerSize;
    unsigned int   width;
    unsigned int   height;
    unsigned int   x;
    unsigned int   y;
    unsigned int   bytesPerRow;
    unsigned int   bitmapSize;
    unsigned char  c;
    unsigned short bpp;
    unsigned short compressed;
    unsigned char* pBitmapData;
    unsigned char* pPixels;
    unsigned char  intBuffer[4];
    unsigned char  shortBuffer[2];

    // open bitmap file
    pFile = MINI_FILE_OPEN(pFileName, "rb");

    // succeeded?
    if (!pFile)
        return GL_INVALID_VALUE;

    // read bitmap signature
    MINI_FILE_READ(&shortBuffer[0], sizeof(unsigned char), 2, pFile);

    // is bitmap signature correct?
    if (shortBuffer[0] != 'B' || shortBuffer[1] != 'M')
        return GL_INVALID_VALUE;

    // skip 8 next bytes
    MINI_FILE_SEEK(pFile, 8, SEEK_CUR);

    // read data offset
    MINI_FILE_READ(&intBuffer[0], sizeof(unsigned char), 4, pFile);
    dataOffset = (unsigned int)(((unsigned char)intBuffer[3] << 24) |
                                ((unsigned char)intBuffer[2] << 16) |
                                ((unsigned char)intBuffer[1] << 8)  |
                                 (unsigned char)intBuffer[0]);

    // read header size
    MINI_FILE_READ(&intBuffer[0], sizeof(unsigned char), 4, pFile);
    headerSize = (unsigned int)(((unsigned char)intBuffer[3] << 24) |
                                ((unsigned char)intBuffer[2] << 16) |
                                ((unsigned char)intBuffer[1] << 8)  |
                                 (unsigned char)intBuffer[0]);

    // search for bitmap type
    switch (headerSize)
    {
        // V3
        case 40:
        {
            // read bitmap width
            MINI_FILE_READ(&intBuffer[0], sizeof(unsigned char), 4, pFile);
            width = (unsigned int)(((unsigned char)intBuffer[3] << 24) |
                                   ((unsigned char)intBuffer[2] << 16) |
                                   ((unsigned char)intBuffer[1] << 8)  |
                                    (unsigned char)intBuffer[0]);

            // read bitmap height
            MINI_FILE_READ(&intBuffer[0], sizeof(unsigned char), 4, pFile);
            height = (unsigned int)(((unsigned char)intBuffer[3] << 24) |
                                    ((unsigned char)intBuffer[2] << 16) |
                                    ((unsigned char)intBuffer[1] << 8)  |
                                     (unsigned char)intBuffer[0]);

            // skip next 2 bytes
            MINI_FILE_SEEK(pFile, 2, SEEK_CUR);

            // read bitmap bit per pixels
            MINI_FILE_READ(&shortBuffer[0], sizeof(unsigned char), 2, pFile);
            bpp = (unsigned short)(((unsigned char)shortBuffer[1] << 8) |
                                    (unsigned char)shortBuffer[0]);

            // is bpp supported?
            if (bpp != 24)
                return GL_INVALID_VALUE;

            // read bitmap compressed flag
            MINI_FILE_READ(&shortBuffer[0], sizeof(unsigned char), 2, pFile);
            compressed = (unsigned short)(((unsigned char)shortBuffer[1] << 8) |
                                           (unsigned char)shortBuffer[0]);

            // is compressed?
            if (compressed)
                return GL_INVALID_VALUE;

            break;
        }

        // OS/2 V1
        case 12:
        {
            // read bitmap width
            MINI_FILE_READ(&shortBuffer[0], sizeof(unsigned char), 2, pFile);
            width = (unsigned int)(((unsigned char)shortBuffer[1] << 8) |
                                    (unsigned char)shortBuffer[0]);

            // read bitmap height
            MINI_FILE_READ(&shortBuffer[0], sizeof(unsigned char), 2, pFile);
            height = (unsigned int)(((unsigned char)shortBuffer[1] << 8) |
                                     (unsigned char)shortBuffer[0]);

            // skip next 2 bytes
            MINI_FILE_SEEK(pFile, 2, SEEK_CUR);

            // read bitmap bit per pixels
            MINI_FILE_READ(&shortBuffer[0], sizeof(unsigned char), 2, pFile);
            bpp = (unsigned short)(((unsigned char)shortBuffer[1] << 8) |
                                    (unsigned char)shortBuffer[0]);

            // is bpp supported?
            if (bpp != 24)
                return GL_INVALID_VALUE;

            break;
        }

        // Windows V4
        case 108:
        {
            // read bitmap width
            MINI_FILE_READ(&shortBuffer[0], sizeof(unsigned char), 2, pFile);
            width = (unsigned int)(((unsigned char)shortBuffer[1] << 8) |
                                    (unsigned char)shortBuffer[0]);

            // skip next 2 bytes
            MINI_FILE_SEEK(pFile, 2, SEEK_CUR);

            // read bitmap height
            MINI_FILE_READ(&shortBuffer[0], sizeof(unsigned char), 2, pFile);
            height = (unsigned int)(((unsigned char)shortBuffer[1] << 8) |
                                     (unsigned char)shortBuffer[0]);

            // skip next 4 bytes
            MINI_FILE_SEEK(pFile, 4, SEEK_CUR);

            // read bitmap bit per pixels
            MINI_FILE_READ(&shortBuffer[0], sizeof(unsigned char), 2, pFile);
            bpp = (unsigned short)(((unsigned char)shortBuffer[1] << 8) |
                                    (unsigned char)shortBuffer[0]);

            // is bpp supported?
            if (bpp != 24)
                return GL_INVALID_VALUE;

            break;
        }

        default:
            // unsupported bitmap format
            return GL_INVALID_VALUE;
    }

    bytesPerRow = ((width * 3 + 3) / 4) * 4 - (width * 3 % 4);
    bitmapSize  = bytesPerRow * height;
    pBitmapData = (unsigned char*)malloc(sizeof(unsigned char) * bitmapSize);

    // read bitmap data
    MINI_FILE_SEEK(pFile, dataOffset, SEEK_SET);
    MINI_FILE_READ(pBitmapData, sizeof(unsigned char), bitmapSize, pFile);

    // close file
    MINI_FILE_CLOSE(pFile);

    pPixels = (unsigned char*)malloc(sizeof(unsigned char) * width * height * 3);

    // get bitmap data into right format
    for (y = 0; y < height; ++y)
        for (x = 0; x < width; ++x)
            for (c = 0; c < 3; ++c)
                pPixels[3 * (width * y + x) + c] =
                        pBitmapData[bytesPerRow * y + 3 * (width - x - 1) + (2 - c)];

    // create new OpenGL texture
    glGenTextures(1, &index);
    glBindTexture(GL_TEXTURE_2D, index);

    // set texture filtering
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // set texture wrapping mode
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // generate texture from bitmap data
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 width,
                 height,
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 pPixels);

    // delete buffers
    free(pPixels);
    free(pBitmapData);

    return index;
}

#endif // MiniVertexH
