#if 1
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
    GLint                       vertexColorLocation;
};

Shader shader;

struct Vertex2
{
    float x = 0.0f;
    float y = 0.0f;
};

void addQuad(float x, float y, float w, float h)
{
    int quadHalfWidth = w / 2;

    int quadHalfHeight = h / 2;

    int screenHalfWidth = screenWidth / 2;

    int screenHalfHeight = screenHeight / 2;

    //Set vertex data
    VertexData3D vData[4];

    std::vector<Vertex2> corners;

    corners.push_back(Vertex2{ x + screenHalfWidth - quadHalfWidth, y + screenHalfHeight - quadHalfHeight });
    corners.push_back(Vertex2{ x + screenHalfWidth + quadHalfWidth, corners[0].y });
    corners.push_back(Vertex2{ corners[1].x,                        y + screenHalfHeight + quadHalfHeight });
    corners.push_back(Vertex2{ corners[0].x,                        corners[02].y });


    // Use an offset to the quad center point, to rotate around the center.
    Vertex2 originOffset{ corners[0].x + quadHalfWidth, corners[0].y + quadHalfHeight };

    // Position
    vData[0].pos.x = corners[0].x;
    vData[0].pos.y = corners[0].y;
    vData[0].pos.z = 0.0f;

    vData[1].pos.x = corners[1].x;
    vData[1].pos.y = corners[1].y;
    vData[1].pos.z = 0.0f;

    vData[2].pos.x = corners[2].x;
    vData[2].pos.y = corners[2].y;
    vData[2].pos.z = 0.0f;

    vData[3].pos.x = corners[3].x;
    vData[3].pos.y = corners[3].y;
    vData[3].pos.z = 0.0f;

    vData[0].color.r = 1.0f;
    vData[0].color.g = 0.0f;
    vData[0].color.b = 0.0f;
    vData[0].color.a = 1.0;

    vData[1].color.r = 1.0f;
    vData[1].color.g = 0.0f;
    vData[1].color.b = 0.0f;
    vData[1].color.a = 1.0;

    vData[2].color.r = 1.0f;
    vData[2].color.g = 0.0f;
    vData[2].color.b = 0.0f;
    vData[2].color.a = 1.0;

    vData[3].color.r = 1.0f;
    vData[3].color.g = 0.0f;
    vData[3].color.b = 0.0f;
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

in vec4 color_in;

out VS_OUT
{
    vec4 color;
} vs_out;

void main() 
{  
     
    vs_out.color = color_in;

    gl_Position = projectionMatrix * modelViewMatrix * vec4(vertexPos3D.x, vertexPos3D.y, vertexPos3D.z, 1.0);
}
)V0G0N";


    std::string fragmentShaderCode = R"V0G0N(
#version 330 core

layout(location = 0) out vec4 fragColor;

in vec4 gl_FragCoord;

in VS_OUT
{
    vec4 color;
} fs_in;


void main() 
{
    fragColor = fs_in.color;
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
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

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
    shader.vertexColorLocation = glGetAttribLocation(shader.programId, "color_in");

    projectionMatrixLocation = glGetUniformLocation(shader.programId, "projectionMatrix");
    modelViewMatrixLocation = glGetUniformLocation(shader.programId, "modelViewMatrix");

    // Initialize the projection matrix
    projectionMatrix = glm::ortho<GLfloat>(0.0, screenWidth, screenHeight, 0.0, 1.0, -1.0);
    glUniformMatrix4fv(projectionMatrixLocation, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    //Initialize modelview
    modelViewMatrix = glm::mat4();
    glUniformMatrix4fv(modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelViewMatrix));


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

        // Reset the group color.
        colorCounter = 0;

        groupColor.r = 0.0f;
        groupColor.g = 0.0f;
        groupColor.b = 1.0f;

        addQuad(0, 0, 256, 256);

        //addSquareQuad (-100,  100,   0, 2, false );
        //addSquareQuad (   0,    0,   0, 3, false );
        //addSquareQuad ( -48,  -48, -12, 3, true  );
        //addSquareQuad (-148, -124,  45, 3, false );
        //addSquareQuad (  64,  -32, -60, 2, true  );
        //addSquareQuad (   0,    0,  45, 1, true  );
        //addSquareQuad ( 100,  100,  30, 3, true  );
        //addSquareQuad ( 130,  200,  60, 2, false );
        //addSquareQuad ( 200,  200,  16, 1, false );

        GLuint vertexCount = shader.vertexData.size();

        if (vertexCount > 0)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0); // Render to texture

            // Init the scene.
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

            // Clear color buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glEnable(GL_DEPTH_TEST);

            //glViewport(0, 0, screenWidth, screenHeight); already set

            glUseProgram(shader.programId);

            updateVbo();

            //glActiveTexture(GL_TEXTURE0);

            //glBindTexture(GL_TEXTURE_2D, silhouetteTextureId);

            glBindVertexArray(shader.texturedQuadVao);

            glDrawElements(GL_QUADS, vertexCount, GL_UNSIGNED_INT, NULL);


            glBindVertexArray(NULL);
        }

        SDL_GL_SwapWindow(window);
    }

    return 0;
}
#endif