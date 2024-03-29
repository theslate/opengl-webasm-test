
#include <emscripten.h>
#define GLFW_INCLUDE_ES3
#include <GLFW/glfw3.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "linmath.h"
#include "lodepng.h"

GLFWwindow * window;
GLuint vertex_buffer, vertex_shader, fragment_shader, program;
GLint mvp_location, vpos_location, vcol_location, vtexCoord_location;
static const struct
{
    float x, y;
    float u, v;
} vertices[4] =
{
    { -0.5f, 0.5f, 0.f, 0.f },
    { -0.5f, -0.5f, 0.f, 1.f },
    { 0.5f, 0.5f, 1.f, 0.f },
    { 0.5f, -.5f, 1.f, 1.f }
};

static const char* vertex_shader_text =
    "#version 300 es\n"
    "uniform mat4 MVP;\n"
    "in highp vec2 vPos;\n"
    "in lowp vec2 vTexCoord;\n"
    "out lowp vec2 v_texCoord;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = MVP * vec4(vPos, 0.0, 1.0);\n"
    "    v_texCoord = vTexCoord; \n"
    "}\n";

static const char* fragment_shader_text =
    "#version 300 es\n"
    "in lowp vec2 v_texCoord;\n"
    "out lowp vec4 o_color;\n"
    "uniform sampler2D texture2d;\n"
    "void main()\n"
    "{\n"
    "   o_color = texture(texture2d, v_texCoord);"
    "}\n";

static void output_error(int error, const char * msg) {
    fprintf(stderr, "Error: %s\n", msg);
}

static void generate_frame() {
    float ratio;
    int width, height;
    mat4x4 m, p, mvp;
    glfwGetFramebufferSize(window, &width, &height);
    ratio = width / (float) height;
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    
    mat4x4_identity(m);
    float time = (float) glfwGetTime();
    float scale = .99f;
    int tris = fmin(time*60.0f, 10000);
    float rot = time*.01f + 3.14/4;
    float offset = 0; 
    float offsetAmount = .05f;
    float windowSize = 2;//tris * offsetAmount;
    glUseProgram(program);
    mat4x4_ortho(p, -windowSize*ratio, windowSize*ratio, -windowSize, windowSize, windowSize, -windowSize);
    for (int tri = 0; tri < tris; tri++)
    { 
        rot = rot * 1.0001f;  
        offset = offset * scale;
        mat4x4_scale_aniso(m, m, scale, scale, scale);
        mat4x4_rotate_Z(m, m, rot);
        mat4x4_translate_in_place(m, offset, 0, 0);
        mat4x4_mul(mvp, p, m);
    
        glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat*) mvp);
    
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        offset += offsetAmount *scale;
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
}

static int check_compiled(shader) {
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if(success == GL_FALSE) {
        GLint max_len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_len);

        GLchar err_log[max_len];
        glGetShaderInfoLog(shader, max_len, &max_len, &err_log[0]);
        glDeleteShader(shader);

        fprintf(stderr, "Shader compilation failed: %s\n", err_log);
    }

    return success;
}

static int check_linked(program) {
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    if(success == GL_FALSE) {
        GLint max_len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_len);

        GLchar err_log[max_len];
        glGetProgramInfoLog(program, max_len, &max_len, &err_log[0]);

        fprintf(stderr, "Program linking failed: %s\n", err_log);
    }

    return success;
}

EM_JS(int, canvas_get_width, (), {
  return document.getElementById("canvas").clientWidth;
});

EM_JS(int, canvas_get_height, (), {
  return document.getElementById("canvas").clientHeight;
});

int main() {
    
    glfwSetErrorCallback(output_error);

    if (!glfwInit()) {
        fputs("Faileid to initialize GLFW", stderr);
        emscripten_force_exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(canvas_get_width(), canvas_get_height(), "My Title", NULL, NULL);

    if (!window) {
        fputs("Failed to create GLFW window", stderr);
        glfwTerminate();
        emscripten_force_exit(EXIT_FAILURE);
    }

    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
    glfwMakeContextCurrent(window);

    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShader(vertex_shader);
    check_compiled(vertex_shader);

    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);
    check_compiled(fragment_shader);

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    check_linked(program);

    // Load file and decode image.  
    // Ratio for power of two version compared to actual version, to render the non power of two image with proper size.
    unsigned char* image;
    unsigned width, height;
    unsigned error = lodepng_decode32_file(&image, &width, &height, "assets/goose60.png");
    size_t u2 = 1; while(u2 < width) u2 *= 2;
    size_t v2 = 1; while(v2 < height) v2 *= 2;

    // glEnable(GL_TEXTURE_2D);
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, u2, v2, 0, GL_RGBA, GL_UNSIGNED_BYTE, &image[0]);

    mvp_location = glGetUniformLocation(program, "MVP");
    vpos_location = glGetAttribLocation(program, "vPos");
    vtexCoord_location = glGetAttribLocation(program, "vTexCoord");

    glEnableVertexAttribArray(vpos_location);
    glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
        sizeof(float) * 4, (void*) 0);

    glEnableVertexAttribArray(vtexCoord_location);
    glVertexAttribPointer(vtexCoord_location, 2, GL_FLOAT, GL_FALSE,
        sizeof(float) * 4, (void*) (sizeof(float) * 2));

    emscripten_set_main_loop(generate_frame, 60, 0);
}
