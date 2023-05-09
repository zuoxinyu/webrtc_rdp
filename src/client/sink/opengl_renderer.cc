#include "opengl_renderer.hh"
#include "logger.hh"

#include <cstdio>
#include <stdexcept>

#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_version.h>
#include <SDL2/SDL_video.h>

static const std::string vs_src = R"(
    #version 330 core

    layout (location = 0) in vec2 aPosition;
    layout (location = 1) in vec2 aTexCoord;

    varying vec2 vTexCoord;

    void main() {
      gl_Position = vec4(aPosition, 0.0, 1.0);
      vTexCoord = aTexCoord;
    }
)";
static const std::string fs_src = R"(
    #version 330 core

    varying vec2 vTexCoord;

    uniform sampler2D uTexY;
    uniform sampler2D uTexU;
    uniform sampler2D uTexV;

    void main() {
      vec3 yuv;
      vec3 rgb;
      yuv.x = texture2D(uTexY, vTexCoord).r;
      yuv.y = texture2D(uTexU, vTexCoord).r - 0.5;
      yuv.z = texture2D(uTexV, vTexCoord).r - 0.5;
      rgb = mat3( 1,       1,         1,
                  0,       -0.39465,  2.03211,
                  1.13983, -0.58060,  0) * yuv;
      gl_FragColor = vec4(rgb, 1);
    }
)";
static const float vertices[] = {
    // position|texcoord
    -1.0, -1.0, 0.0, 1.0, // lt
    +1.0, -1.0, 1.0, 1.0, // rt
    -1.0, +1.0, 0.0, 0.0, // lb
    +1.0, +1.0, 1.0, 0.0, // rb
};

static const int indices[] = {
    0, 1, 2, //
    1, 2, 3, //
};

OpenGLRenderer::OpenGLRenderer(Config conf) : VideoRenderer(std::move(conf))
{
    glctx_ = SDL_GL_CreateContext(window_);
    if (glewInit() != GLEW_OK) {
        throw std::runtime_error("glew init failed");
    }
    assert(glctx_);

    glViewport(0, 0, conf_.width, conf_.height);

    program_ = create_program(vs_src, fs_src);
    glUseProgram(program_);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, false, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, false, 4 * sizeof(float), (void *)8);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    textures_[Y] = create_texture();
    textures_[U] = create_texture();
    textures_[V] = create_texture();

    glBindVertexArray(vao);
    glUseProgram(program_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    SDL_GL_SwapWindow(window_);
}

OpenGLRenderer::~OpenGLRenderer()
{

    glDeleteTextures(1, &textures_[V]);
    glDeleteTextures(1, &textures_[U]);
    glDeleteTextures(1, &textures_[Y]);

    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    glDeleteProgram(program_);
    SDL_GL_DeleteContext(glctx_);
}

GLuint OpenGLRenderer::create_texture()
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return tex;
}

GLuint OpenGLRenderer::create_shader(unsigned typ, const std::string &code)
{
    const char *src = code.c_str();
    GLuint shader = glCreateShader(typ);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (!status) {
        char logbuf[512] = {0};
        glGetShaderInfoLog(shader, 512, nullptr, logbuf);
        logger::critical("shader compile failed: {}", logbuf);
        exit(EXIT_FAILURE);
    }

    return shader;
}

GLuint OpenGLRenderer::create_program(const std::string &vs,
                                      const std::string &fs)
{
    auto vsShader = create_shader(GL_VERTEX_SHADER, vs);
    auto fsShader = create_shader(GL_FRAGMENT_SHADER, fs);
    GLuint program = glCreateProgram();

    glAttachShader(program, vsShader);
    glAttachShader(program, fsShader);
    glLinkProgram(program);
    glDetachShader(program, vsShader);
    glDetachShader(program, fsShader);
    glDeleteShader(vsShader);
    glDeleteShader(fsShader);

    int status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char logbuf[512] = {0};
        glGetProgramInfoLog(program, 512, nullptr, logbuf);
        logger::critical("program link failed: {}", logbuf);
        exit(EXIT_FAILURE);
    }

    return program;
}

void OpenGLRenderer::update_textures(const void *ydata, const void *udata,
                                     const void *vdata)
{
    SDL_GL_MakeCurrent(window_, glctx_);

    glActiveTexture(GL_TEXTURE0 + Y);
    glBindTexture(GL_TEXTURE_2D, textures_[Y]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, conf_.width, conf_.height, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, ydata);
    glUniform1i(glGetUniformLocation(program_, "uTexY"), Y);

    glActiveTexture(GL_TEXTURE0 + U);
    glBindTexture(GL_TEXTURE_2D, textures_[U]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, conf_.width / 2,
                 conf_.height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, udata);
    glUniform1i(glGetUniformLocation(program_, "uTexU"), U);

    glActiveTexture(GL_TEXTURE0 + V);
    glBindTexture(GL_TEXTURE_2D, textures_[V]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, conf_.width / 2,
                 conf_.height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, vdata);
    glUniform1i(glGetUniformLocation(program_, "uTexV"), V);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    SDL_GL_SwapWindow(window_);
}
