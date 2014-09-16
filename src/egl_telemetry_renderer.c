/* RasPiFPV
 *
 * Copyright (C) 2014 Pod <monsieur.pod@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *  Parts of this module derived from OMXPlayer: 
 *  https://github.com/huceke/omxplayer/blob/master/SubtitleRenderer.cpp
 */

#include "egl_telemetry_renderer.h"
#include <bcm_host.h>
#include <EGL/egl.h>
#include <VG/openvg.h>
#include <ft2build.h>
#include <pthread.h>
#include <assert.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

static const int LAYER_NUMBER = 1;
static const char * FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
static const float FONT_SIZE = 0.05;
#define MAX_GLYPHS 256

typedef struct {
    float width;
} GlyphAttributes;

struct _FPVEGLTelemetryRenderer {
    int width;
    int height;
    pthread_t thread;
    int running;
    FPVTelemetryRX *telemetry_rx;
    int show_altitude;
    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    FT_Library ft;
    FT_Face font_face;
    FT_Stroker font_stroker;
    VGFont font;
    VGFont font_outline;
    GlyphAttributes glyphs[MAX_GLYPHS];
};

typedef enum {
    ALIGNMENT_LEFT,
    ALIGNMENT_CENTER,
    ALIGNMENT_RIGHT
} Alignment;

typedef struct {
    float x;
    float y;
} Point;

static inline Point FPVEGLPointMake(float x, float y) { return (Point){x, y}; };

#pragma mark -
#pragma mark Forward declarations

static int fpv_egl_telemetry_renderer_init_window(FPVEGLTelemetryRenderer * renderer);
static int fpv_egl_telemetry_renderer_init_egl(FPVEGLTelemetryRenderer * renderer);
static int fpv_egl_telemetry_renderer_init_font(FPVEGLTelemetryRenderer * renderer);
static void fpv_egl_telemetry_renderer_cleanup_window(FPVEGLTelemetryRenderer * renderer);
static void fpv_egl_telemetry_renderer_cleanup_egl(FPVEGLTelemetryRenderer * renderer);
static void fpv_egl_telemetry_renderer_cleanup_font(FPVEGLTelemetryRenderer * renderer);
static void * fpv_egl_telemetry_renderer_thread_entry(void *userinfo);
static void fpv_egl_telemetry_renderer_render(FPVEGLTelemetryRenderer * renderer);
static void fpv_egl_telemetry_renderer_draw_text(FPVEGLTelemetryRenderer * renderer, char * text, Point location, Alignment alignment);
int fpv_egl_telemetry_renderer_load_glyph(FPVEGLTelemetryRenderer * renderer, int codepoint);
int fpv_egl_telemetry_renderer_load_glyph_with_font(int codepoint, FT_Face face, VGFont font, FT_Stroker stroker, GlyphAttributes *outGlyphAttributes);

#pragma mark -

FPVEGLTelemetryRenderer * fpv_egl_telemetry_renderer_new(FPVTelemetryRX * telemetry_rx) {
    FPVEGLTelemetryRenderer *renderer = (FPVEGLTelemetryRenderer*)calloc(1, sizeof(FPVEGLTelemetryRenderer));
    renderer->telemetry_rx = telemetry_rx;
    renderer->show_altitude = 0;
    return renderer;
}

void fpv_egl_telemetry_renderer_dispose(FPVEGLTelemetryRenderer * renderer) {
    free(renderer);
}

int fpv_egl_telemetry_renderer_start(FPVEGLTelemetryRenderer * renderer) {
    if ( renderer->running ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer already running\n");
        return -1;
    }

    renderer->running = 1;
    int result = pthread_create(&renderer->thread, NULL, fpv_egl_telemetry_renderer_thread_entry, renderer);
    if ( result != 0 ) {
        fprintf(stderr, "Unable to launch FPVEGLTelemetryRenderer thread: %s\n", strerror(result));
    }

    return result == 0;
}

void fpv_egl_telemetry_renderer_stop(FPVEGLTelemetryRenderer * renderer) {
    renderer->running = 0;
    pthread_join(renderer->thread, NULL);
}

#pragma mark -
#pragma mark Setup/Cleanup

static int fpv_egl_telemetry_renderer_init_window(FPVEGLTelemetryRenderer * renderer) {
    bcm_host_init();
    
    uint32_t display_width;
    uint32_t display_height;
    int ret = graphics_get_display_size(0, &display_width, &display_height);
    if ( ret < 0 ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't open display\n");
        return 0;
    }

    renderer->width = display_width;
    renderer->height = display_height;

    VC_RECT_T dst_rect = { .x = 0, .y = 0, .width = display_width, .height = display_height };
    VC_RECT_T src_rect = { .x = 0, .y = 0, .width = display_width << 16, .height = display_height << 16 };

    renderer->dispman_display = vc_dispmanx_display_open(0);
    if ( !renderer->dispman_display ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't open display\n");
        return 0;
    }
    DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
    if ( !update ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't update dispmanx\n");
        vc_dispmanx_display_close(renderer->dispman_display);
        return 0;
    }
    
    renderer->dispman_element = vc_dispmanx_element_add(update, renderer->dispman_display, LAYER_NUMBER,
                                    &dst_rect, 0, &src_rect, DISPMANX_PROTECTION_NONE, 
                                    0, 0, (DISPMANX_TRANSFORM_T)0);
    vc_dispmanx_update_submit_sync(update);

    if ( !renderer->dispman_element ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't setup dispmanx element\n");
        vc_dispmanx_display_close(renderer->dispman_display);
        return 0;
    }

    return 1;
}

static void fpv_egl_telemetry_renderer_cleanup_window(FPVEGLTelemetryRenderer * renderer) {
    DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
    vc_dispmanx_element_remove(update, renderer->dispman_element);
    vc_dispmanx_update_submit_sync(update);
    vc_dispmanx_display_close(renderer->dispman_display);
}

static int fpv_egl_telemetry_renderer_init_egl(FPVEGLTelemetryRenderer * renderer) {
    renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if ( !renderer->display ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't get EGL display\n");
        return 0;
    }
    if ( !eglInitialize(renderer->display, NULL, NULL) ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't init EGL\n");
        return 0;
    }

    const EGLint attributeList[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };
    EGLConfig config;
    EGLint configCount;
    eglChooseConfig(renderer->display, attributeList, &config, 1, &configCount);
    if ( configCount == 0 ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't setup display\n");
        return 0;
    }

    if ( !eglBindAPI(EGL_OPENVG_API) ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't init OpenVG API\n");
        return 0;
    }

    EGL_DISPMANX_WINDOW_T nativeWindow = {
        .element = renderer->dispman_element,
        .width = renderer->width,
        .height = renderer->height
    };

    renderer->surface = eglCreateWindowSurface(renderer->display, config, &nativeWindow, NULL);
    if ( !renderer->surface ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't setup EGL surface\n");
        return 0;
    }

    renderer->context = eglCreateContext(renderer->display, config, EGL_NO_CONTEXT, NULL);
    if ( !renderer->context ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't setup EGL context\n");
        return 0;
    }
    
    if ( !eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context) ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't init EGL context\n");
        return 0;
    }

    vgSeti(VG_FILTER_FORMAT_LINEAR, VG_TRUE);
    vgSeti(VG_IMAGE_QUALITY, VG_IMAGE_QUALITY_BETTER);
    
    renderer->font = vgCreateFont(64);
    assert(renderer->font);
    renderer->font_outline = vgCreateFont(64);
    assert(renderer->font_outline);

    return 1;
}

static void fpv_egl_telemetry_renderer_cleanup_egl(FPVEGLTelemetryRenderer * renderer) {
    vgDestroyFont(renderer->font);
    vgDestroyFont(renderer->font_outline);
    
    eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(renderer->display);
}

static int fpv_egl_telemetry_renderer_init_font(FPVEGLTelemetryRenderer * renderer) {
    if ( FT_Init_FreeType(&renderer->ft) != 0 ||
         FT_New_Face(renderer->ft, FONT_PATH, 0, &renderer->font_face) != 0 ||
         FT_Set_Pixel_Sizes(renderer->font_face, 0, FONT_SIZE * (float)renderer->height) != 0 ||
         FT_Stroker_New(renderer->ft, &renderer->font_stroker) != 0 ) {
        fprintf(stderr, "FPVEGLTelemetryRenderer: Can't init fonts\n");
        return 0;
    }

    FT_Stroker_Set(renderer->font_stroker, FONT_SIZE * (float)renderer->height * 0.1, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

    return 1;
}

static void fpv_egl_telemetry_renderer_cleanup_font(FPVEGLTelemetryRenderer * renderer) {
    FT_Done_FreeType(renderer->ft);
}

#pragma mark -
#pragma mark Rendering

static void * fpv_egl_telemetry_renderer_thread_entry(void *userinfo) {
    FPVEGLTelemetryRenderer * renderer = (FPVEGLTelemetryRenderer*)userinfo;
    
    if ( !fpv_egl_telemetry_renderer_init_window(renderer) ) {
        renderer->running = 0;
        return NULL;
    }
    
    if ( !fpv_egl_telemetry_renderer_init_egl(renderer) ) {
        fpv_egl_telemetry_renderer_cleanup_window(renderer);
        renderer->running = 0;
        return NULL;
    }
    
    if ( !fpv_egl_telemetry_renderer_init_font(renderer) ) {
        fpv_egl_telemetry_renderer_cleanup_egl(renderer);
        fpv_egl_telemetry_renderer_cleanup_window(renderer);
        renderer->running = 0;
        return NULL;
    }
    
    const float framerate = 1.0/30.0;
    while ( renderer->running ) {
        fpv_egl_telemetry_renderer_render(renderer);
        usleep(framerate * 1.0e6);
    }
    renderer->running = 0;
    
    fpv_egl_telemetry_renderer_cleanup_font(renderer);
    fpv_egl_telemetry_renderer_cleanup_egl(renderer);
    fpv_egl_telemetry_renderer_cleanup_window(renderer);
    
    return NULL;
}

static void fpv_egl_telemetry_renderer_render(FPVEGLTelemetryRenderer * renderer) {
    vgClear(0, 0, renderer->width, renderer->height);

    fpv_egl_telemetry_renderer_draw_text(renderer, "hello", (Point){0, renderer->height - 100}, ALIGNMENT_LEFT);

    eglSwapBuffers(renderer->display, renderer->surface);
}

static void fpv_egl_telemetry_renderer_draw_text(FPVEGLTelemetryRenderer * renderer, char * text, Point location, Alignment alignment) {
    // Prepare glyphs and calculate width, for alignment
    float width = 0.0;
    int i;
    for ( i=0; text[i]; i++ ) {
        int codepoint = text[i];
        if ( codepoint > MAX_GLYPHS ) continue;
        
        if ( !renderer->glyphs[codepoint].width ) {
            // Load glyph
            fpv_egl_telemetry_renderer_load_glyph(renderer, codepoint);
        }
        
        width += renderer->glyphs[codepoint].width;
    }

    if ( alignment == ALIGNMENT_RIGHT ) {
        location.x -= width;
    } else if ( alignment == ALIGNMENT_CENTER ) {
        location.x -= width / 2.0;
    }
    
    vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_MULTIPLY);
    assert(!vgGetError());
    
    // Render outline
    VGPaint paint = vgCreatePaint();
    assert(paint);
    vgSetColor(paint, 0x00000000);
    assert(!vgGetError());
    vgSetPaint(paint, VG_FILL_PATH);
    assert(!vgGetError());
    vgDestroyPaint(paint);
    assert(!vgGetError());
    
    VGfloat pos[] = {location.x, location.y};
    vgSetfv(VG_GLYPH_ORIGIN, 2, pos);
    assert(!vgGetError());
    
    for ( i=0; text[i]; i++ ) {
        vgDrawGlyph(renderer->font_outline, text[i], VG_FILL_PATH, VG_FALSE);
        assert(!vgGetError());
    }

    // Render fill
    paint = vgCreatePaint();
    assert(paint);
    vgSetColor(paint, 0xFFFFFFFF);
    assert(!vgGetError());
    vgSetPaint(paint, VG_FILL_PATH);
    assert(!vgGetError());
    vgDestroyPaint(paint);
    assert(!vgGetError());
    
    vgSetfv(VG_GLYPH_ORIGIN, 2, pos);
    assert(!vgGetError());
    
    for ( i=0; text[i]; i++ ) {
        vgDrawGlyph(renderer->font, text[i], VG_FILL_PATH, VG_FALSE);
        assert(!vgGetError());
    }
}

int fpv_egl_telemetry_renderer_load_glyph(FPVEGLTelemetryRenderer * renderer, int codepoint) {
    if ( !fpv_egl_telemetry_renderer_load_glyph_with_font(codepoint, renderer->font_face, renderer->font, NULL, &renderer->glyphs[codepoint]) ) {
        return 0;
    }
    
    if ( !fpv_egl_telemetry_renderer_load_glyph_with_font(codepoint, renderer->font_face, renderer->font_outline, renderer->font_stroker, NULL) ) {
        return 0;
    }
    
    return 1;
}

int fpv_egl_telemetry_renderer_load_glyph_with_font(int codepoint, FT_Face face, VGFont font, FT_Stroker stroker, GlyphAttributes *outGlyphAttributes) {
    if ( outGlyphAttributes ) {
        memset(outGlyphAttributes, 0, sizeof(GlyphAttributes));
    }
    
    FT_UInt ft_glyph_index = FT_Get_Char_Index(face, codepoint);
    if ( FT_Load_Glyph(face, ft_glyph_index, FT_LOAD_NO_HINTING) != 0 ) {
        fprintf(stderr, "Unable to load glyph %d (FT_Load_Glyph)\n", codepoint);
        return 0;
    }
    
    FT_Glyph glyph;
    if ( FT_Get_Glyph(face->glyph, &glyph) != 0 ) {
        fprintf(stderr, "Unable to load glyph %d (FT_Get_Glyph)\n", codepoint);
        return 0;
    }
    
    if ( stroker ) {
        if ( FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1) != 0 ) {
            fprintf(stderr, "Unable to load glyph %d (FT_Glyph_StrokeBorder)\n", codepoint);
            FT_Done_Glyph(glyph);
            return 0;
        }
    }
    
    if ( FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, NULL, 1) != 0 ) {
        fprintf(stderr, "Unable to load glyph %d (FT_Glyph_To_Bitmap)\n", codepoint);
        FT_Done_Glyph(glyph);
        return 0;
    }
    FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
    FT_Bitmap *bitmap = &bitmap_glyph->bitmap;
    
    VGImage image = NULL;
    VGfloat glyph_origin[2] = {0, 0};
    
    VGErrorCode vgerror = 0;
    
    if ( bitmap->width > 0 && bitmap->rows > 0 ) {
        VGfloat blur_stddev = 0.52;
        const int padding = 3.0*blur_stddev + 0.5;
        const int image_width = bitmap->width + padding*2;
        const int image_height = bitmap->rows + padding*2;
    
        image = vgCreateImage(VG_A_8, image_width, image_height, VG_IMAGE_QUALITY_NONANTIALIASED);
        if ( (vgerror=vgGetError()) ) {
            fprintf(stderr, "Unable to load glyph %d (vgCreateImage): error %d\n", codepoint, vgerror);
            FT_Done_Glyph(glyph);
            return 0;
        }
    
        vgImageSubData(image,
                       bitmap->pitch > 0 ? bitmap->buffer + bitmap->pitch*(bitmap->rows-1) : bitmap->buffer,
                       bitmap->pitch > 0 ? -bitmap->pitch : bitmap->pitch,
                       VG_A_8,
                       padding,
                       padding,
                       bitmap->width,
                       bitmap->rows);
        if ( (vgerror=vgGetError()) ) {
            fprintf(stderr, "Unable to load glyph %d (vgImageSubData): error %d\n", codepoint, vgerror);
            FT_Done_Glyph(glyph);
            return 0;
        }
    
        VGImage softened_image = vgCreateImage(VG_A_8, image_width, image_height, VG_IMAGE_QUALITY_NONANTIALIASED);
        if ( (vgerror=vgGetError()) ) {
            fprintf(stderr, "Unable to load glyph %d (vgCreateImage): error %d\n", codepoint, vgerror);
            FT_Done_Glyph(glyph);
            return 0;
        }

        vgGaussianBlur(softened_image, image, blur_stddev, blur_stddev, VG_TILE_FILL);
        assert(!vgGetError());

        vgDestroyImage(image);
        assert(!vgGetError());

        image = softened_image;
        
        glyph_origin[0] = padding - bitmap_glyph->left;
        glyph_origin[1] = padding + bitmap->rows - bitmap_glyph->top - 1;
    }
    
    VGfloat escapement[2] = {(face->glyph->advance.x + 32.0) / 64.0, 0.0};
    
    if ( outGlyphAttributes ) {
        outGlyphAttributes->width = escapement[0];
    }
    
    vgSetGlyphToImage(font, codepoint, image, glyph_origin, escapement);
    if ( (vgerror=vgGetError()) ) {
        fprintf(stderr, "Unable to load glyph %d (vgSetGlyphToImage): error %d\n", codepoint, vgerror);
        FT_Done_Glyph(glyph);
        return 0;
    }
    
    if ( image ) {
        vgDestroyImage(image);
    }
    
    FT_Done_Glyph(glyph);
    
    return 1;
}
