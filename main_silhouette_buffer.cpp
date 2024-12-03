#if 0

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <IL/il.h>
#include <IL/ilu.h>

#include "SDL.h"

#define main SDL_main

// Alert and return if GL error
#define RETURN_IF_GL_ERROR(F) RETURN_IF_GL_ERROR2("Function " F " failed with error")
#define RETURN_IF_GL_ERROR2(M) { GLenum error = glGetError(); if (error != GL_NO_ERROR) { std::cout<< M ": " << gluErrorString(error) << std::endl; return false; } }

SDL_Window*     window;
SDL_Surface*    screen;
SDL_Renderer*   sdlRenderer;
SDL_GLContext   openGlContext;

int             screenWidth = 1280;
int             screenHeight = 720;

glm::mat4       projectionMatrix;
GLint           projectionMatrixLocation;

glm::mat4       modelViewMatrix;
GLint           modelViewMatrixLocation;

GLint           texUnitLocation;

GLuint          textureId;

GLuint          frameBufferId = 0;
GLuint          silhouetteTextureId = 0;


struct TexCoords
{
    GLfloat s;
    GLfloat t;
};

struct VertexPos3D
{
    GLfloat x;
    GLfloat y;
    GLfloat z;
};

struct ColorRgba
{
    GLfloat r;
    GLfloat g;
    GLfloat b;
    GLfloat a;
};

struct VertexData3D
{
    VertexPos3D	pos;
    TexCoords	texCoords;
    ColorRgba	color;
};

struct Shader
{
    GLuint                      programId;
    GLuint                      vertexBufferId;
    GLuint                      indexBufferId;
    int                         vertexBufferSize;
    std::vector<VertexData3D>   vertexData;
    std::vector<GLuint>         indexData;
    GLuint                      texturedQuadVao;
    GLint                       vertexPos2dLocation;
    GLint                       vertexTexCoordsLocation;
    GLint                       vertexColorLocation;
};

Shader shader;


struct Vertex2
{
    float x;
    float y;
};

// Start at full red for groups.
ColorRgba groupColor { 1.0f, 0.0f, 0.0f, 1.0f };

uint32_t colorCounter = 0;

// From a counter value derive a color visually distinct to the human eye
// compared to the other colors nearby in the permutation.
uint32_t getGroupColor(uint32_t counter)
{
    // Determine which component or components will be used.
    // 7 combinations, not 8, because using no components doesn't make sense.

    // R     | 1 0 0   4
    //   G   | 0 1 0   2
    //     B | 0 0 1   1
    // R G   | 1 1 0   6
    // R   B | 1 0 1   5
    //   G B | 0 1 1   3
    // R G B | 1 1 1   7

    int components = (counter % 7) + 1;

    // Get the component bit, and shift it into bit position 0, then do a lshift to shift it into bit position 8.
    // After that cast to a signed char and shift right by 7 to do an arithmetic shift back into position 0, filling 
    // all bits with the least significant bit. Then, recast it back to an unsigned 8 bit int to remove the leading 1's
    // that get added in a two's compliment represenation of signed integers, and concatenate it onto the component mask
    // by casting it back to a unsigned 32 bit int. Bitwise and the components mask with the component value, which is a
    // counter that counts by increments of ((256 / 8) + 1), ((256 / 4) + 1), and ((256 / 2) + 1), and will overflow
    // because they are stored in a uint8_t. This is so that the different color component values don't increment at
    // the same strides.  Also, they will count down from 255, so that the first color isn't full black.

    uint32_t blue = ((uint32_t)((uint8_t)((int8_t)((components & 0x1) << 7) >> 7) & (255 - (uint8_t)(counter * 33))) << 8);
    uint32_t green = ((uint32_t)((uint8_t)((int8_t)(((components & 0x2) >> 1) << 7) >> 7) & (255 - (uint8_t)(counter * 65))) << 16);
    uint32_t red = ((uint32_t)((uint8_t)((int8_t)(((components & 0x4) >> 2) << 7) >> 7) & (255 - (uint8_t)(counter * 129))) << 24);

    // Always use full alpha channel.
    uint32_t finalColor = red | green | blue | 0x000000FF;

    return finalColor;
};

void rotatePoints(float rotationAngle, std::vector<Vertex2> pointsToRotate, std::vector<Vertex2>& rotatedPoints, float originTranslationX, float originTranslationY)
{
    // Convert degrees to radians and set the cos and sin values for rotation.
    double pi = 3.1415926535897;

    // The point after being translated to the native origin.
    float translatedToScreenOriginX = 0.0f;
    float translatedToScreenOriginY = 0.0f;

    // The point after being rotated about the origin.
    float rotatedX = 0.0f;
    float rotatedY = 0.0f;

    float radians = (rotationAngle * pi) / 180.0;

    float sinTheta = sin(radians);
    float cosTheta = cos(radians);

    for (size_t i = 0; i < pointsToRotate.size(); i++)
    {
        // STEP 1: Translate each value to origin.
        translatedToScreenOriginX = pointsToRotate[i].x;
        translatedToScreenOriginY = pointsToRotate[i].y;

        translatedToScreenOriginX -= originTranslationX;
        translatedToScreenOriginY -= originTranslationY;

        // STEP 2: Do the actual rotation transform about the native origin.
        rotatedX = (translatedToScreenOriginX * cosTheta) - (translatedToScreenOriginY * sinTheta);
        rotatedY = (translatedToScreenOriginX * sinTheta) + (translatedToScreenOriginY * cosTheta);

        // STEP 3: Translate the vertices back to original position.
        rotatedX += originTranslationX;
        rotatedY += originTranslationY;

        // STEP 4: Set the rotated values into the corners objects.
        rotatedPoints[i].x = rotatedX;
        rotatedPoints[i].y = rotatedY;
    }
}

void addQuad(float x, float y, float rotationDegrees, float scale, bool newGroup)
{
    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    int quadSize = 50 * scale;

    int quadHalfSize = quadSize / 2;

    int screenHalfWidth = screenWidth / 2;

    int screenHalfHeight = screenHeight / 2;

    //Set vertex data
    VertexData3D vData[4];

    std::vector<Vertex2> corners;

    corners.push_back( Vertex2{ x + screenHalfWidth - quadHalfSize, y + screenHalfHeight - quadHalfSize });
    corners.push_back( Vertex2{ x + screenHalfWidth + quadHalfSize, corners[0].y });
    corners.push_back( Vertex2{ corners[1].x, y + screenHalfHeight + quadHalfSize });
    corners.push_back( Vertex2{ corners[0].x, corners[02].y });


    std::vector<Vertex2> transformedCorners;

    transformedCorners.resize(4);

    rotatePoints(rotationDegrees, corners, transformedCorners, corners[0].x + quadHalfSize, corners[0].y + quadHalfSize);
    
    // Position
    vData[0].pos.x = transformedCorners[0].x;
    vData[0].pos.y = transformedCorners[0].y;
    vData[0].pos.z = 0.0f;

    vData[1].pos.x = transformedCorners[1].x;
    vData[1].pos.y = transformedCorners[1].y;
    vData[1].pos.z = 0.0f;

    vData[2].pos.x = transformedCorners[2].x;
    vData[2].pos.y = transformedCorners[2].y;
    vData[2].pos.z = 0.0f;

    vData[3].pos.x = transformedCorners[3].x;
    vData[3].pos.y = transformedCorners[3].y;
    vData[3].pos.z = 0.0f;


    vData[0].texCoords.s = 0.0;
    vData[0].texCoords.t = 0.0;

    vData[1].texCoords.s = 1.0;
    vData[1].texCoords.t = 0.0;

    vData[2].texCoords.s = 1.0;
    vData[2].texCoords.t = 1.0;

    vData[3].texCoords.s = 0.0;
    vData[3].texCoords.t = 1.0;

    if (newGroup == true)
    {
        colorCounter++;

        uint32_t color = getGroupColor(colorCounter);

        groupColor.r = ((color & 0xFF000000) >> 24) / 255.0f;
        groupColor.g = ((color & 0x00FF0000) >> 16) / 255.0f;
        groupColor.b = ((color & 0x0000FF00) >> 8 ) / 255.0f;
        
    }

    vData[0].color.r = groupColor.r;
    vData[0].color.g = groupColor.g;
    vData[0].color.b = groupColor.b;
    vData[0].color.a = 1.0;

    vData[1].color.r = groupColor.r;
    vData[1].color.g = groupColor.g;
    vData[1].color.b = groupColor.b;
    vData[1].color.a = 1.0;

    vData[2].color.r = groupColor.r;
    vData[2].color.g = groupColor.g;
    vData[2].color.b = groupColor.b;
    vData[2].color.a = 1.0;

    vData[3].color.r = groupColor.r;
    vData[3].color.g = groupColor.g;
    vData[3].color.b = groupColor.b;
    vData[3].color.a = 1.0;


    int vertexCount = shader.vertexData.size();

    shader.indexData.push_back(vertexCount);
    shader.indexData.push_back(vertexCount + 1);
    shader.indexData.push_back(vertexCount + 2);
    shader.indexData.push_back(vertexCount + 3);

    shader.vertexData.push_back(vData[0]);
    shader.vertexData.push_back(vData[1]);
    shader.vertexData.push_back(vData[2]);
    shader.vertexData.push_back(vData[3]);
}

bool loadBufferIntoTexture(GLint* buffer, size_t size, int width, int height, GLuint& textureId)
{
    // Generate texture ID
    glGenTextures(1, &textureId);

    RETURN_IF_GL_ERROR("glGenTextures");

    // Bind texture ID
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_RECTANGLE, textureId);

    glTexImage2D(
        GL_TEXTURE_RECTANGLE,
        0,
        GL_RGBA32I,
        width,
        height,
        0,
        GL_RGBA_INTEGER,
        GL_INT,
        (GLuint*)buffer);

    RETURN_IF_GL_ERROR("glTexImage2D");

    //Set texture parameters
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    //Unbind texture
    glBindTexture(GL_TEXTURE_RECTANGLE, NULL);

    return true;
}

bool loadImageIntoTexture(std::string filename, GLuint& textureId, uint32_t level)
{
    bool ret = true;

    // Read the bitmap file to a byte array.
    std::ifstream bitmapFile;

    bitmapFile.open(filename.c_str(), std::ios::in | std::ios::binary);

    int imageSize = 0;

    char* imageBuffer;

    if (bitmapFile.is_open())
    {
        imageSize = std::filesystem::file_size(std::filesystem::path(filename));

        imageBuffer = new char[imageSize];

        bitmapFile.read((char*)imageBuffer, imageSize);
    }
    else
    {
        return false;
    }

    // Generate and set current image ID
    ILuint imgID = 0;
    ilGenImages(1, &imgID);
    ilBindImage(imgID);

    ILboolean success = ilLoadL(IL_PNG, imageBuffer, imageSize);

    ILinfo imageInfo;

    //Image loaded successfully
    if (success == IL_TRUE)
    {
        //Convert image to RGBA
        success = ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

        if (success == IL_TRUE)
        {
            iluGetImageInfo(&imageInfo);

            // Generate texture ID
            glGenTextures(1, &textureId);

            //Check for error
            GLenum error = glGetError();

            if (error != GL_NO_ERROR)
            {
                std::cout << "glGenTextures failed with error: " << gluErrorString(error) << std::endl;

                return false;
            }

            // Bind texture ID
            glActiveTexture(GL_TEXTURE0 + level);

            glBindTexture(GL_TEXTURE_2D, textureId);

            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA,
                imageInfo.Width,
                imageInfo.Height,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                (GLuint*)imageInfo.Data);

            //Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

            //Unbind texture
            glBindTexture(GL_TEXTURE_2D, NULL);

            //Check for error
            error = glGetError();

            if (error != GL_NO_ERROR)
            {
                std::cout << "Error loading texture from byte array: " << gluErrorString(error) << std::endl;
                ret = false;
            }
        }
        else
        {
            ILenum error = ilGetError();

            std::cout << "Failed to convert image pixels to RGBA format: " << iluErrorString(error) << std::endl;

            ret = false;
        }
    }
    else
    {
        ILenum error = ilGetError();

        std::cout << "Failed to load sprite sheet image: " << iluErrorString(error) << std::endl;

        ret = false;
    }

    delete [] imageBuffer;

    ilDeleteImage(imgID);

    return ret;
}

bool createTexture()
{
    //GLint glintMax = std::numeric_limits<GLint>::max();

    //// 3x1 texture with a red, green, and blue pixel.
    //GLint buffer[12] = {
    //	2,        0,        0,        glintMax, // Red
    //	0,        glintMax, 0,        glintMax, // Green
    //	0,        0,        glintMax, glintMax  // Blue
    //};

    //bool texLoaded = loadBufferIntoTexture(buffer, 12, 3, 1, textureId);

    bool texLoaded = true;

    texLoaded &= loadImageIntoTexture("debug_texture.png", textureId, 0);

    return texLoaded;
}

void freeVbo()
{
    //Free VBO and IBO
    if (shader.vertexBufferId != 0)
    {
        glDeleteBuffers(1, &shader.vertexBufferId);
        glDeleteBuffers(1, &shader.indexBufferId);

        shader.vertexBufferId = 0;
        shader.indexBufferId = 0;
    }
}

void freeVao()
{
    if (shader.texturedQuadVao != 0)
    {
        glDeleteVertexArrays(1, &shader.texturedQuadVao);

        shader.texturedQuadVao = 0;
    }
}

GLuint createShaders()
{
    // Read the code for the shaders into strings.
    std::string vertexShaderCode = R"V0G0N(
#version 330 core

//Transformation Matrices
uniform mat4 projectionMatrix;
uniform mat4 modelViewMatrix;

in vec3 vertexPos3D;

in vec2 tex_coords_in;
in vec4 color_in;

out VS_OUT
{
    vec2 tex_coords;
    vec4 color;
} vs_out;

void main() 
{  
     
    vs_out.tex_coords = tex_coords_in;
    vs_out.color = color_in;

    gl_Position = projectionMatrix * modelViewMatrix * vec4(vertexPos3D.x, vertexPos3D.y, vertexPos3D.z, 1.0);
}
)V0G0N";


    std::string fragmentShaderCode = R"V0G0N(
#version 330 core

out vec4 fragColor;

uniform sampler2D textureUnit;

in vec4 gl_FragCoord;

in VS_OUT
{
    vec2 tex_coords;
    vec4 color;
} fs_in;


bool isOutline(vec4 textureSample)
{
    vec2 texelSize = vec2(1.0f / 50.0f, 1.0f / 50.0f);

    // Get the neighboring texel values. 
    vec4 pixelUp    = texture(textureUnit, fs_in.tex_coords - vec2(0, texelSize.y));

    vec4 pixelDown  = texture(textureUnit, fs_in.tex_coords + vec2(0, texelSize.y));

    vec4 pixelLeft  = texture(textureUnit, fs_in.tex_coords - vec2(texelSize.x, 0));

    vec4 pixelRight = texture(textureUnit, fs_in.tex_coords + vec2(texelSize.x, 0));

    // If this pixel is transparent, and any neighboring pixels are not, it is an edge pixel.
    if (textureSample.a == 0 && (pixelUp.a > 0 || pixelDown.a > 0 || pixelLeft.a > 0 || pixelRight.a > 0))
    {
        return true;
    }

    return false;
}

void main() 
{
    vec4 textureSample = texture(textureUnit, fs_in.tex_coords);

    // If this pixel is transparent, and any neighboring pixels are not, it is an edge pixel.
    bool useOutline = false;

    if (isOutline(textureSample) && useOutline)
    {
        //// If a fragment for this group already exists in the buffer, skip the outline.
        //bool discard = false;

        //if (discard == false)
        //{
            fragColor.rgba = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        //}
    }
    else
    {
        fragColor = textureSample;

        if (fs_in.color.a > 0.0 && fragColor.a > 0.0f)
        {
            fragColor = fs_in.color;
        }
    }

}
)V0G0N";

    // Create the shaders
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

    GLint Result = GL_FALSE;
    int InfoLogLength;

    // Compile Vertex Shader
    char const* vertexSourcePointer = vertexShaderCode.c_str();
    glShaderSource(vertexShaderId, 1, &vertexSourcePointer, NULL);
    glCompileShader(vertexShaderId);

    // Check Vertex Shader
    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &Result);

    glGetShaderiv(vertexShaderId, GL_INFO_LOG_LENGTH, &InfoLogLength);

    if (InfoLogLength > 0)
    {
        std::vector<char> vertexShaderErrorMessage(InfoLogLength);

        glGetShaderInfoLog(vertexShaderId, InfoLogLength, NULL, &vertexShaderErrorMessage[0]);

        std::cout << &vertexShaderErrorMessage[0] << std::endl;
    }

    // Compile Fragment Shader
    char const* fragmentSourcePointer = fragmentShaderCode.c_str();
    glShaderSource(fragmentShaderId, 1, &fragmentSourcePointer, NULL);
    glCompileShader(fragmentShaderId);

    // Check Fragment Shader
    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(fragmentShaderId, GL_INFO_LOG_LENGTH, &InfoLogLength);

    if (InfoLogLength > 0)
    {
        std::vector<char> fragmentShaderErrorMessage(InfoLogLength);
        glGetShaderInfoLog(fragmentShaderId, InfoLogLength, NULL, &fragmentShaderErrorMessage[0]);
        std::cout << &fragmentShaderErrorMessage[0] << std::endl;
    }

    // Link
    GLuint programId = glCreateProgram();
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);
    glLinkProgram(programId);

    // Check the program
    glGetProgramiv(programId, GL_LINK_STATUS, &Result);
    glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &InfoLogLength);

    if (InfoLogLength > 0)
    {
        std::vector<char> programErrorMessage(std::max(InfoLogLength, int(1)));
        glGetProgramInfoLog(programId, InfoLogLength, NULL, &programErrorMessage[0]);
        std::cout << &programErrorMessage[0] << std::endl;
    }

    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);

    return programId;
}

bool initOpenGl()
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    openGlContext = SDL_GL_CreateContext(window);

    if (openGlContext == NULL)
    {
        std::cout << "OpenGL context creation failed with error: " << SDL_GetError() << std::endl;
    }

    //Initialize GLEW
    GLenum glewError = glewInit();

    if (glewError != GLEW_OK)
    {
        std::cout << "Error initializing GLEW: " << glewGetErrorString(glewError) << std::endl;
        return false;
    }

    //Make sure OpenGL 2.1 is supported
    if (!GLEW_VERSION_2_1)
    {
        std::cout << "OpenGL 2.1 not supported" << std::endl;
        return false;
    }

    std::cout << "GLEW version: " << glewGetString(GLEW_VERSION) << std::endl;

    //Set the viewport
    glViewport(0.f, 0.f, screenWidth, screenHeight);

    //Initialize clear color
    glClearColor(0.f, 0.f, 0.f, 1.f);

    //Enable texturing
    glEnable(GL_TEXTURE_2D);

    //Set blending
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //Check for error
    GLenum error = glGetError();

    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL renderer initialization failed with error: " << gluErrorString(error) << std::endl;

        return false;
    }

    std::cout << "OpenGL version " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    return true;
}

bool initializeScreen()
{
    if (window != NULL)
    {
        SDL_DestroyWindow(window);
    }

    // Create the window via SDL
    window = SDL_CreateWindow("Untitled Game - Firemelon Engine",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        screenWidth,
        screenHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);


    if (window == NULL)
    {
        std::cout << "Window creation failed with error: " << SDL_GetError() << std::endl;

        return false;
    }

    SDL_ShowCursor(1);

    screen = SDL_GetWindowSurface(window);

    if (screen == nullptr)
    {
        return false;
    }

    // Create the renderer.
    sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (sdlRenderer == nullptr)
    {
        std::cout << "Renderer creation failed with error: " << SDL_GetError() << std::endl;

        return false;
    }

    if (initOpenGl() == false)
    {
        return false;
    }

    return true;
}

bool initFbo()
{

    glGenFramebuffers(1, &frameBufferId);

    RETURN_IF_GL_ERROR("glGenFramebuffers");

    glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);

    RETURN_IF_GL_ERROR("glBindFramebuffer");
    
    glGenTextures(1, &silhouetteTextureId);

    RETURN_IF_GL_ERROR("glGenTextures");

    glBindTexture(GL_TEXTURE_2D, silhouetteTextureId);

    RETURN_IF_GL_ERROR("glBindTexture");

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    RETURN_IF_GL_ERROR("glTexImage2D");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, silhouetteTextureId, 0);

    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };

    glDrawBuffers(1, drawBuffers); 

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        return false;
    }

    return true;
}

bool initVbo()
{
    if (shader.vertexBufferId == 0)
    {
        // Start with a buffer size of 500. Re-allocate a larger buffer if
        // it becomes necessary later.
        VertexData3D vData[500];
        GLuint iData[500];

        //Create VBO
        glGenBuffers(1, &shader.vertexBufferId);
        glBindBuffer(GL_ARRAY_BUFFER, shader.vertexBufferId);
        glBufferData(GL_ARRAY_BUFFER, 500 * sizeof(VertexData3D), vData, GL_DYNAMIC_DRAW);

        //Check for error
        GLenum error = glGetError();

        if (error != GL_NO_ERROR)
        {
            std::cout << "Error creating vertex buffer: " << gluErrorString(error) << std::endl;
            return false;
        }

        //Create IBO
        glGenBuffers(1, &shader.indexBufferId);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader.indexBufferId);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 500 * sizeof(GLuint), iData, GL_DYNAMIC_DRAW);

        //Check for error
        error = glGetError();

        if (error != GL_NO_ERROR)
        {
            std::cout << "Error creating vertex index buffer: " << gluErrorString(error) << std::endl;
            return false;
        }

        //Unbind buffers
        glBindBuffer(GL_ARRAY_BUFFER, NULL);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, NULL);
    }

    return true;
}

bool initShaders()
{
    GLenum error;

    shader.programId = createShaders();

    glUseProgram(shader.programId);

    shader.vertexPos2dLocation = glGetAttribLocation(shader.programId, "vertexPos3D");
    shader.vertexTexCoordsLocation = glGetAttribLocation(shader.programId, "tex_coords_in");
    shader.vertexColorLocation = glGetAttribLocation(shader.programId, "color_in");

    projectionMatrixLocation = glGetUniformLocation(shader.programId, "projectionMatrix");
    modelViewMatrixLocation = glGetUniformLocation(shader.programId, "modelViewMatrix");
    texUnitLocation = glGetUniformLocation(shader.programId, "textureUnit");

    // Initialize the projection matrix
    projectionMatrix = glm::ortho<GLfloat>(0.0, screenWidth, screenHeight, 0.0, 1.0, -1.0);
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    //Initialize modelview
    modelViewMatrix = glm::mat4();
    glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelViewMatrix));

    glUniform1i(texUnitLocation, 0);

    RETURN_IF_GL_ERROR2("Error setting texture location");

    bool fboInitOk = initFbo();

    if (fboInitOk == false) {
        return false;
    }


    // Initialize the vertex buffer and index buffer objects that
    // will be used to render the quads.
    bool vboInitOk = initVbo();

    if (vboInitOk == false) {
        return false;
    }

    //Generate textured quad VAO
    glGenVertexArrays(1, &shader.texturedQuadVao);

    //Bind vertex array
    glBindVertexArray(shader.texturedQuadVao);

    RETURN_IF_GL_ERROR2("Error binding vertex array");

    // Enable vertex attributes.
    glEnableVertexAttribArray(shader.vertexPos2dLocation);

    RETURN_IF_GL_ERROR2("Error enabling vertex attribute 'Position'");

    glEnableVertexAttribArray(shader.vertexTexCoordsLocation);

    RETURN_IF_GL_ERROR2("Error enabling vertex attribute 'Tex Coords'");

    glEnableVertexAttribArray(shader.vertexColorLocation);

    RETURN_IF_GL_ERROR2("Error enabling vertex attribute 'Color'");

    //Set vertex data
    glBindBuffer(GL_ARRAY_BUFFER, shader.vertexBufferId);

    glVertexAttribPointer(shader.vertexPos2dLocation,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(VertexData3D),
        (GLvoid*)offsetof(VertexData3D, pos));

    glVertexAttribPointer(shader.vertexTexCoordsLocation,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(VertexData3D),
        (GLvoid*)offsetof(VertexData3D, texCoords));

    glVertexAttribPointer(shader.vertexColorLocation,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(VertexData3D),
        (GLvoid*)offsetof(VertexData3D, color));


    RETURN_IF_GL_ERROR2("Error setting vertex data");

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader.indexBufferId);

    //Unbind VAO
    glBindVertexArray(NULL);

    return true;
}

void updateVbo()
{
    // Update the VBO contents. If the size of the array has increased, allocate a new VBO.
    // Otherwise update the current VBO with the vertex data for this frame.
    int size = shader.vertexData.size();

    if (size > 0)
    {
        VertexData3D* vData = &shader.vertexData[0];
        GLuint* iData = &shader.indexData[0];

        if (size > shader.vertexBufferSize)
        {
            // Allocate a new VBO and IBO to fit the new data size.
            shader.vertexBufferSize = size;

            // Destroy the old VBO and IBO
            freeVbo();

            //Create new VBO
            glGenBuffers(1, &shader.vertexBufferId);
            glBindBuffer(GL_ARRAY_BUFFER, shader.vertexBufferId);
            glBufferData(GL_ARRAY_BUFFER, size * sizeof(VertexData3D), vData, GL_DYNAMIC_DRAW);

            //Create new IBO
            glGenBuffers(1, &shader.indexBufferId);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader.indexBufferId);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, size * sizeof(GLuint), iData, GL_DYNAMIC_DRAW);

            // Bind the new VBO and IBO to the VAO.
            glBindVertexArray(shader.texturedQuadVao);

            //Set vertex data
            glBindBuffer(GL_ARRAY_BUFFER, shader.vertexBufferId);

            glVertexAttribPointer(shader.vertexPos2dLocation,
                3,
                GL_FLOAT,
                GL_FALSE,
                sizeof(VertexData3D),
                (GLvoid*)offsetof(VertexData3D, pos));

            glVertexAttribPointer(shader.vertexTexCoordsLocation,
                2,
                GL_FLOAT,
                GL_FALSE,
                sizeof(VertexData3D),
                (GLvoid*)offsetof(VertexData3D, texCoords));


            glVertexAttribPointer(shader.vertexColorLocation,
                4,
                GL_FLOAT,
                GL_FALSE,
                sizeof(VertexData3D),
                (GLvoid*)offsetof(VertexData3D, color));

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader.indexBufferId);

            //Unbind VAO
            glBindVertexArray(NULL);

            //Unbind buffers
            glBindBuffer(GL_ARRAY_BUFFER, NULL);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, NULL);
        }
        else
        {
            // Bind vertex buffer.
            glBindBuffer(GL_ARRAY_BUFFER, shader.vertexBufferId);

            // Update vertex buffer data.
            glBufferSubData(GL_ARRAY_BUFFER, 0, size * sizeof(VertexData3D), vData);

            // Bind index buffer.
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shader.indexBufferId);

            // Update index buffer.		
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size * sizeof(GLuint), iData);
        }
    }
}

int main(int argc, char* argv[])
{
    if (!initializeScreen()) { std::cout << "OpenGL Initialization Failed" << std::endl; }
    if (!initShaders()) { std::cout << "Shaders Initialization Failed" << std::endl; }
    if (!createTexture()) { std::cout << "Texture Creation Failed" << std::endl; }

    bool quit = false;

    while (quit == false)
    {
        // Init the scene.
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        // Clear color buffer
        glClear(GL_COLOR_BUFFER_BIT);

        shader.vertexData.clear();
        shader.indexData.clear();

        SDL_Event event;

        // While there's an event to handle...
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:

                quit = true;

                break;

            default:
                break;
            }
        }

        // Reset the color group counter
        colorCounter = 0;

        addQuad(0, 0, 0, 3, true);
        addQuad(48, 48, -12, 3, true);
        addQuad(-48, -48, 45, 3, false);
        addQuad(64, -32, -60, 2, true);

        GLuint vertexCount = shader.vertexData.size();

        if (vertexCount > 0)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, frameBufferId);
            glViewport(0, 0, screenWidth, screenHeight); 

            glUseProgram(shader.programId);

            updateVbo();

            glActiveTexture(GL_TEXTURE0);

            glBindTexture(GL_TEXTURE_2D, textureId);

            glBindVertexArray(shader.texturedQuadVao);

            glDrawElements(GL_QUADS, vertexCount, GL_UNSIGNED_INT, NULL);

            glBindVertexArray(NULL);
        }

        SDL_GL_SwapWindow(window);
    }

    return 0;
}

#endif