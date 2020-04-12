
#include <stdlib.h>
#include <stdio.h>

#include <SDL.h>
#include <SDL_opengles2.h>

#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3platform.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "my_point.h"
#include "snis_font.h"

const unsigned int DISP_WIDTH = 800;
const unsigned int DISP_HEIGHT = 600;

struct my_vect_obj **font;
static GLuint shader_object;
static SDL_Window *window; 
static SDL_GLContext context;

char *get_file_contents(const char *filename) {
    unsigned int bytes_read = 0;
    unsigned int file_size = 0;
    if (filename == NULL) {
        printf("Null filename passed to get_file_contents!\n");
        return NULL;
    }

    FILE *fp = fopen(filename, "rb");
    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    printf("File size is %d bytes.\n", file_size);
    fseek(fp, 0L, SEEK_SET);

    if (file_size == 0) {
        printf("Tried to read 0-byte file!\n");
        fclose(fp);
        return NULL;
    }
    
    char *out_buf = malloc(file_size + 1);
    out_buf[file_size] = 0;
    bytes_read = fread(out_buf, 1, file_size, fp);
    printf("Read %d bytes from file.\n", bytes_read);
    if (bytes_read < file_size) {
        printf("Didn't read enough data! Errors may occur!");
    }
    fclose(fp);
    return out_buf;
}

GLuint load_shader_file(const char *filename, GLenum type) {

    GLuint shader;
    GLint compiled;

    const char *shader_src = get_file_contents(filename);

    printf("Compiling shader at %s\n", filename);
    //printf(shader_src);

    if (shader_src == NULL) {
        printf("Unable to read shader file at %s! Shader not compiled.\n", filename);
        return 0;
    }

    shader = glCreateShader(type);
    glShaderSource(shader, 1, &shader_src, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if(!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

        if(infoLen > 1) {
            char* infoLog = malloc(sizeof(char) * infoLen);
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            printf("Error compiling shader:\n%s\n", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

int sdl_init() {

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not be started! SDL Error: %s\n", SDL_GetError());
    } else {
        //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

        window = SDL_CreateWindow("Test Window", 0, 0,
                        DISP_WIDTH, DISP_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetSwapInterval(1);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, context);
        printf("Clearing color!\n");
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        printf("done.\n");
    }

    return 1;
}

int graph_init(void) {

    GLuint vert_shader;
    GLuint frag_shader;

    vert_shader = load_shader_file("files/vert_shader.shader", GL_VERTEX_SHADER);
    frag_shader = load_shader_file("files/frag_shader.shader", GL_FRAGMENT_SHADER);

    if (!vert_shader || !frag_shader) {
        return 0;
    }

    shader_object = glCreateProgram();

    if (!shader_object) {
        printf("glCreateProgram failed!"); 
    }

    glAttachShader(shader_object, vert_shader);
    glAttachShader(shader_object, frag_shader);

    glLinkProgram(shader_object);
    glUseProgram(shader_object);

    return 0;

}

int get_lines_in_glyph(struct my_vect_obj *glyph) {
    int n_points = glyph->npoints;
    int n_breaks = 0;
    for (int i = 0; i < n_points; i++) {
        if (glyph->p[i].x == LINE_BREAK) {
            n_breaks++;
        }
    }
    if (n_points == 0) {
        return 0;
    } else {
        return (n_points - 1) - (2 * n_breaks);
    }
}

GLshort *generate_verts_for_glyph(struct my_vect_obj *glyph, GLshort *buf, short scale, short glyph_offset) {
    int n_points = glyph->npoints;
    GLshort *curr_vert_elem = buf;
    for (int j = 0; j < n_points - 1; j++) {
        /* If this or the next point is a line break, we can skip this point. */
        if (glyph->p[j].x == LINE_BREAK || glyph->p[j + 1].x == LINE_BREAK) {
            continue;
        }

        *(curr_vert_elem) = (glyph->p[j].x + 4 * glyph_offset - 20) * scale;
        *(curr_vert_elem + 1)= glyph->p[j].y * scale;
        *(curr_vert_elem + 2) = (short) 0;

        *(curr_vert_elem + 3) = (glyph->p[j + 1].x + 4 * glyph_offset - 20) * scale;
        *(curr_vert_elem + 4) = glyph->p[j + 1].y * scale;
        *(curr_vert_elem + 5) = (short) 0;

        printf("%d %d %d -> %d %d %d\n", *curr_vert_elem, *(curr_vert_elem + 1),
                         *(curr_vert_elem + 2),
                          *(curr_vert_elem + 3), *(curr_vert_elem + 4), *(curr_vert_elem + 5));

        curr_vert_elem += 6;
    }
    printf("--------\n");
    return curr_vert_elem;
}

void draw_text(char *str) {
    int total_lines = 0;
    int vbo_size = 0;

    char *curr_c = str;
    
    while (*curr_c != 0) {
        printf("Calculating for glyph %c\n", *curr_c);
        int n_lines = get_lines_in_glyph(font[*curr_c]);
        printf("n_lines for glyph: %d\n", n_lines);
        vbo_size += 6 * n_lines * sizeof(GLshort);
        total_lines += n_lines;
        curr_c++;
    }
    
    GLshort *verts = malloc(vbo_size);
    GLshort *curr_vert_elem = verts;

    curr_c = str;
    while (*curr_c) {
        curr_vert_elem = generate_verts_for_glyph(font[*curr_c], curr_vert_elem, 1000, curr_c - str);
        curr_c++;
    }

    printf("n_lines: %d\n", total_lines);

    GLuint vbo;

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(GL_ARRAY_BUFFER, vbo_size, verts, GL_STATIC_DRAW);
    GLint posAttrib = glGetAttribLocation(shader_object, "position");
    glEnableVertexAttribArray(posAttrib);
    glVertexAttribPointer(posAttrib, 3, GL_SHORT, GL_TRUE, 0, 0);
    printf("GL_LINES element count: %d\n", total_lines * 2);
    glDrawArrays(GL_LINES, 0, total_lines * 2);
    SDL_GL_SwapWindow(window);
}

int main() {
    snis_make_font(&font, ascii_font, 1.0, -1.0);
    sdl_init();
    graph_init();
    draw_text("Hello World!");
}