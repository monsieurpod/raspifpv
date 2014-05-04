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
 */

#include "egl_renderer.h"
#include <bcm_host.h>
#include <EGL/egl.h>
#include <VG/openvg.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

static const int LAYER_NUMBER = 1;
static const char * FONT_PATH = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
static const float FONT_SIZE = 0.05;

struct _FPVEGLRenderer {
    int width;
    int height;
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
    VGFont font_border;
};

typedef enum {
    ALIGNMENT_LEFT,
    ALIGNMENT_CENTER,
    ALIGNMENT_RIGHT
} Alignment;

int fpv_egl_init_window(FPVEGLRenderer * renderer);
int fpv_egl_init_vg(FPVEGLRenderer * renderer);
int fpv_egl_init_font(FPVEGLRenderer * renderer);
void fpv_egl_cleanup_window(FPVEGLRenderer * renderer);
void fpv_egl_cleanup_vg(FPVEGLRenderer * renderer);
void fpv_egl_cleanup_font(FPVEGLRenderer * renderer);


FPVEGLRenderer * fpv_egl_renderer_new(FPVTelemetryRX * telemetry_rx) {
    FPVEGLRenderer *renderer = (FPVEGLRenderer*)calloc(1, sizeof(FPVEGLRenderer));
    renderer->telemetry_rx = telemetry_rx;
    renderer->show_altitude = 0;

    bcm_host_init();

    if ( !fpv_egl_init_window(renderer) ) {
        free(renderer);
        return NULL;
    }

    if ( !fpv_egl_init_vg(renderer) ) {
        fpv_egl_cleanup_window(renderer);
        free(renderer);
        return NULL;
    }

    if ( !fpv_egl_init_font(renderer) ) {
        fpv_egl_cleanup_vg(renderer);
        fpv_egl_cleanup_window(renderer);
        free(renderer);
        return NULL;
    }

    return renderer;
}

void fpv_egl_renderer_dispose(FPVEGLRenderer * renderer) {
    fpv_egl_cleanup_font(renderer);
    fpv_egl_cleanup_vg(renderer);
    fpv_egl_cleanup_window(renderer);
    free(renderer);
}

static int fpv_egl_init_window(FPVEGLRenderer * renderer) {
    uint32_t display_width;
    uint32_t display_height;
    int ret = graphics_get_display_size(0, &display_width, &display_height);
    if ( ret < 0 ) {
        fprintf(stderr, "FPVEGLRenderer: Can't open display\n");
        return 0;
    }

    renderer->width = display_width;
    renderer->height = display_height;

    VC_RECT_T dst_rect = { .x = 0, .y = 0, .width = display_width, .height = display_height };
    VC_RECT_T src_rect = dst_rect;

    renderer->dispman_display = vc_dispmanx_display_open(0);
    if ( !renderer->dispman_display ) {
        fprintf(stderr, "FPVEGLRenderer: Can't open display\n");
        return 0;
    }
    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);
    renderer->dispman_element = vc_dispmanx_element_add(dispman_update, renderer->dispman_display, LAYER_NUMBER, &dst_rect, 0, &src_rect, DISPMANX_PROTECTION_NONE, 0, 0, (DISPMANX_TRANSFORM_T)0);
    vc_dispmanx_update_submit_sync(dispman_update);

    if ( !renderer->dispman_element ) {
        fprintf(stderr, "FPVEGLRenderer: Can't setup dispmanx element\n");
        vc_dispman_display_close(renderer->dispman_display);
        return 0;
    }

    return 1;
}

static void fpv_egl_cleanup_window(FPVEGLRenderer * renderer) {
    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);
    vc_dispmanx_element_remove(renderer->dispman_element);
    vc_dispmanx_update_submit_sync(dispman_update);
    vc_dispman_display_close(renderer->dispman_display);
}

static int fpv_egl_init_vg(FPVEGLRenderer * renderer) {
    renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if ( !renderer->display ) {
        fprintf(stderr, "FPVEGLRenderer: Can't get EGL display\n");
        return 0;
    }
    if ( !eglInitialize(renderer->display, NULL, NULL) ) {
        fprintf(stderr, "FPVEGLRenderer: Can't init EGL\n");
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
        fprintf(stderr, "FPVEGLRenderer: Can't setup display\n");
        return 0;
    }

    if ( !eglBindAPI(EGL_OPENVG_API) ) {
        fprintf(stderr, "FPVEGLRenderer: Can't init OpenVG API\n");
        return 0;
    }

    const EGL_DISPMANX_WINDOW_T nativeWindow = {
        .element = renderer->dispman_element;
        .width = renderer->width,
        .height = renderer->height
    };

    renderer->surface = eglCreateWindowSurface(renderer->display, config, &nativeWindow, NULL);
    if ( !renderer->surface ) {
        fprintf(stderr, "FPVEGLRenderer: Can't setup EGL surface\n");
        return 0;
    }

    renderer->context = eglCreateContext(renderer->display, config, EGL_NO_CONTEXT, NULL);
    if ( !renderer->context ) {
        fprintf(stderr, "FPVEGLRenderer: Can't setup EGL context\n");
        return 0;
    }

    if ( !eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context) ) {
        fprintf(stderr, "FPVEGLRenderer: Can't init EGL context\n");
        return 0;
    }

    vgSetI(VG_FILTER_FORMAT_LINEAR, VG_TRUE);
    vgSetI(VG_IMAGE_QUALITY, VG_IMAGE_QUALITY_BETTER);

}

static void fpv_egl_cleanup_vg(FPVEGLRenderer * renderer) {
    eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(renderer->display);
}

static int fpv_egl_init_font(FPVEGLRenderer * renderer) {
    if ( !FT_Init_FreeType(&renderer->ft) ||
         !FT_New_Face(renderer->ft, FONT_PATH, 0, &renderer->font_face) ||
         !FT_Set_Pixel_Sizes(renderer->font_face, 0, FONT_SIZE * (float)renderer->height) ||
         !FT_Stroker_New(&renderer->ft, &renderer->font_stroker) ) {
        fprintf(stderr, "FPVEGLRenderer: Can't init fonts\n");
        return 0;
    }

    FT_Stroker_Set(renderer->font_stroker, FONT_SIZE * (float)renderer->height * 0.1, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

    
    renderer->font = vgCreateFont(0);
    renderer->font_border = vgCreateFont(0);

}

static void fpv_egl_cleanup_font(FPVEGLRenderer * renderer) {
    FT_Done_FreeType(renderer->ft);
}
