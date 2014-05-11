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

#include "gstreamer_renderer.h"
#include <bcm_host.h>
#include <EGL/egl.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const int LAYER_NUMBER = 0;

struct _FPVGStreamerRenderer {
    GstPipeline * pipeline;
    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
};

static const char * GST_PIPELINE_RECEIVE = "udpsrc %s port=%d caps=\"application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264\" ! rtph264depay";
static const char * GST_PIPELINE_DECODE = "h264parse ! omxh264dec";
static const char * GST_PIPELINE_SINK = "eglglessink sync=false name=eglsink";

typedef struct
{
  EGL_DISPMANX_WINDOW_T w;
  DISPMANX_DISPLAY_HANDLE_T d;
} RPIWindowData;

#pragma mark - Forward declarations

static int fpv_gstreamer_renderer_init_window(FPVGStreamerRenderer * renderer);
static gboolean on_message(GstBus * bus, GstMessage * message, gpointer user_data);

#pragma mark -

FPVGStreamerRenderer * fpv_gstreamer_renderer_new(GMainLoop * loop, char * multicast_addr, int port) {
    FPVGStreamerRenderer *renderer = (FPVGStreamerRenderer*)calloc(1, sizeof(FPVGStreamerRenderer));
    
    char multicast_str[256] = "";
    if ( multicast_addr && strlen(multicast_addr) > 0 ) {
        snprintf(multicast_str, sizeof(multicast_str), "multicast-group=%s", multicast_addr);
    }
    
    // Parse and create pipeline
    char pipeline_description[1024];
    snprintf(pipeline_description, sizeof(pipeline_description), GST_PIPELINE_RECEIVE, multicast_str, port);
    strcat(pipeline_description, " ! ");
    strcat(pipeline_description, GST_PIPELINE_DECODE);
    strcat(pipeline_description, " ! ");
    strcat(pipeline_description, GST_PIPELINE_SINK);

    g_debug("Pipeline: %s", pipeline_description);

    GError *error = NULL;
    renderer->pipeline = GST_PIPELINE(gst_parse_launch(pipeline_description, &error));
    if ( !renderer->pipeline ) {
        g_error("Could not create pipeline %s: %s", pipeline_description, error->message);
        g_free(renderer);
        return NULL;
    }

    fpv_gstreamer_renderer_init_window(renderer);

    printf("Listening for video at %s:%d\n", multicast_addr && strlen(multicast_addr) > 0 ? multicast_addr : "0.0.0.0", port);
    
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(renderer->pipeline));
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(on_message), loop);
    gst_object_unref(GST_OBJECT(bus));
    
    return renderer;
}

void fpv_gstreamer_renderer_dispose(FPVGStreamerRenderer * renderer) {
    gst_object_unref(renderer->pipeline);
    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);
    vc_dispmanx_element_remove(dispman_update, renderer->dispman_element);
    vc_dispmanx_update_submit_sync(dispman_update);
    vc_dispmanx_display_close(renderer->dispman_display);
}

void fpv_gstreamer_renderer_start(FPVGStreamerRenderer * renderer) {
    gst_element_set_state(GST_ELEMENT(renderer->pipeline), GST_STATE_PLAYING);
}

void fpv_gstreamer_renderer_stop(FPVGStreamerRenderer * renderer) {
    gst_element_set_state(GST_ELEMENT(renderer->pipeline), GST_STATE_NULL);
}

static int fpv_gstreamer_renderer_init_window(FPVGStreamerRenderer * renderer) {
    bcm_host_init();
    
    uint32_t display_width;
    uint32_t display_height;
    int ret = graphics_get_display_size(0, &display_width, &display_height);
    if ( ret < 0 ) {
        fprintf(stderr, "FPVGStreamerRenderer: Can't open display\n");
        return 0;
    }

    VC_RECT_T dst_rect = { .x = 0, .y = 0, .width = display_width, .height = display_height };
    VC_RECT_T src_rect = { .x = 0, .y = 0, .width = display_width << 16, .height = display_height << 16 };

    DISPMANX_DISPLAY_HANDLE_T display = vc_dispmanx_display_open(0);
    if ( !display ) {
        fprintf(stderr, "FPVGStreamerRenderer: Can't open display\n");
        return 0;
    }
    DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
    DISPMANX_ELEMENT_HANDLE_T element = vc_dispmanx_element_add(update, display, LAYER_NUMBER, 
                                            &dst_rect, 0, &src_rect, DISPMANX_PROTECTION_NONE, 
                                            0, 0, (DISPMANX_TRANSFORM_T)0);
    vc_dispmanx_update_submit_sync(update);

    if ( !element ) {
        fprintf(stderr, "FPVGStreamerRenderer: Can't setup dispmanx element\n");
        vc_dispmanx_display_close(display);
        return 0;
    }
    
    GstElement *sink = gst_bin_get_by_name(GST_BIN(renderer->pipeline), "eglsink");
    g_assert(sink);
    
    RPIWindowData * window_data = g_slice_new0(RPIWindowData);
    window_data->d = display;
    window_data->w.element = element;
    window_data->w.width = display_width;
    window_data->w.height = display_height;
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), (guintptr)window_data);
    
    renderer->dispman_display = display;
    renderer->dispman_element = element;

    return 1;
}

static gboolean on_message(GstBus * bus, GstMessage * message, gpointer user_data) {
    GMainLoop *loop = (GMainLoop*)user_data;

    switch ( GST_MESSAGE_TYPE(message) ) {
        case GST_MESSAGE_ERROR: {
            GError *error = NULL;
            gchar *debug = NULL;
            gst_message_parse_error(message, &error, &debug);
            g_critical("Got error: %s (%s)", error->message, GST_STR_NULL(debug));
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError *error = NULL;
            gchar *debug = NULL;
            gst_message_parse_warning(message, &error, &debug);
            g_critical("Got warning: %s (%s)", error->message, GST_STR_NULL(debug));
            break;
        }
        case GST_MESSAGE_EOS: {
            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }

    return TRUE;
}