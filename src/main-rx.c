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

#include <gst/gst.h>
#include <gst/video/video.h>
#include <cairo.h>
#include <cairo-gobject.h>
#include <glib.h>
#include <stdio.h>
#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "render.h"
#include "telemetry_rx.h"

static const char * GST_PIPELINE_RECEIVE = "udpsrc multicast-group=%s port=%d caps=\"application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264\" ! rtph264depay";
static const char * GST_PIPELINE_OVERLAY = "autovideoconvert ! cairooverlay name=overlay";

#if defined(TARGET_RPI)
static const char * GST_PIPELINE_DECODE = "h264parse ! omxh264dec";
static const char * GST_PIPELINE_SINK = "eglglessink sync=false";
#elif defined(TARGET_DARWIN)
static const char * GST_PIPELINE_DECODE = "avdec_h264";
static const char * GST_PIPELINE_SINK = "osxvideosink fullscreen=true";
#else
static const char * GST_PIPELINE_DECODE = "avdec_h264";
static const char * GST_PIPELINE_SINK = "autovideosink";
#endif

static void prepare_overlay(GstElement * overlay, GstCaps * caps, gpointer user_data) {
    GstVideoInfo vinfo;
    if ( !gst_video_info_from_caps(&vinfo, caps) ) {
        return;
    }

    FPVRenderer * renderer = (FPVRenderer*)user_data;
    fpv_renderer_set_frame_size(renderer, GST_VIDEO_INFO_WIDTH(&vinfo), GST_VIDEO_INFO_HEIGHT(&vinfo));
}

static void draw_overlay(GstElement * overlay, cairo_t * cr, guint64 timestamp, guint64 duration, gpointer user_data) {
    FPVRenderer * renderer = (FPVRenderer*)user_data;
    fpv_renderer_render(renderer, cr, timestamp);
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

static GstPipeline* init_gst_pipeline(GKeyFile * keyfile, FPVRenderer *renderer) {

    char * multicast_addr = keyfile ? g_key_file_get_string(keyfile, "Networking", "multicast_address", NULL) : NULL;
    int port = keyfile ? g_key_file_get_integer(keyfile, "Networking", "video_port", NULL) : 0;
    char * decode_pipeline = keyfile ? g_key_file_get_string(keyfile, "Video", "receiver_decode_pipeline", NULL) : NULL;
    char * sink_pipeline = keyfile ? g_key_file_get_string(keyfile, "Video", "receiver_sink_pipeline", NULL) : NULL;

    if ( !multicast_addr ) multicast_addr = RASPIFPV_MULTICAST_ADDR;
    if ( !port ) port = RASPIFPV_PORT_VIDEO;
    if ( !decode_pipeline ) decode_pipeline = (char*)GST_PIPELINE_DECODE;
    if ( !sink_pipeline ) sink_pipeline = (char*)GST_PIPELINE_SINK;

    // Parse and create pipeline
    GError *error = NULL;
    char pipeline_description[1024];
    if ( strlen(GST_PIPELINE_RECEIVE)+20+3+strlen(decode_pipeline)+3+strlen(GST_PIPELINE_OVERLAY)+3+strlen(sink_pipeline) > sizeof(pipeline_description) ) {
        g_print("Error: receiver_...._pipeline combination is too long");
        exit(1);
    }
    snprintf(pipeline_description, sizeof(pipeline_description), GST_PIPELINE_RECEIVE, multicast_addr, port);
    strcat(pipeline_description, " ! ");
    strcat(pipeline_description, decode_pipeline);
    strcat(pipeline_description, " ! ");
    strcat(pipeline_description, GST_PIPELINE_OVERLAY);
    strcat(pipeline_description, " ! ");
    strcat(pipeline_description, sink_pipeline);

    g_debug("Pipeline: %s", pipeline_description);

    GstPipeline *pipeline = GST_PIPELINE(gst_parse_launch(pipeline_description, &error));
    if ( !pipeline ) {
        g_error("Could not create pipeline %s: %s", pipeline_description, error->message);
    }

    // Configure overlay
    GstElement *overlay = gst_bin_get_by_name(GST_BIN(pipeline), "overlay");
    g_assert(overlay);
    g_signal_connect(overlay, "draw", G_CALLBACK(draw_overlay), renderer);
    g_signal_connect(overlay, "caps-changed", G_CALLBACK(prepare_overlay), renderer);

    printf("Listening for video at %s:%d\n", multicast_addr, port);

    return pipeline;
}

static FPVTelemetryRX* init_telemetry_rx(GKeyFile *keyfile) {

    char *address = keyfile ? g_key_file_get_string(keyfile, "Networking", "multicast_address", NULL) : NULL;
    int port = keyfile ? g_key_file_get_integer(keyfile, "Networking", "telemetry_port", NULL) : 0;
    
    FPVTelemetryRX *telemetry_rx = fpv_telemetry_rx_new(address ? address : RASPIFPV_MULTICAST_ADDR, port ? port : RASPIFPV_PORT_TELEMETRY);

    return telemetry_rx;
}

static FPVRenderer* init_renderer(GKeyFile *keyfile, FPVTelemetryRX * telemetry_rx) {
    FPVRenderer *renderer = fpv_renderer_new(telemetry_rx);

    if ( renderer && keyfile ) {
        GError *error = NULL;
        gboolean show_altitude = g_key_file_get_boolean(keyfile, "Telemetry", "show_altitude", &error);
        if ( !error ) {
            fpv_renderer_set_show_altitude(renderer, show_altitude);
        }
    }

    return renderer;
}

static char *config_path = NULL;
static GOptionEntry options[] = {
    { "config", 0, 0, G_OPTION_ARG_FILENAME, &config_path, "Config file path (default " RASPIFPV_DEFAULT_CONFIG_PATH ")", "PATH"},
    NULL
};

int main(int argc, char ** argv) {

    // Parse options
    GError *error = NULL;
    GOptionContext *context = g_option_context_new("- FPV ground station software");
    g_option_context_add_main_entries(context, options, NULL);
    if ( !g_option_context_parse(context, &argc, &argv, &error) ) {
        g_print("Option parsing failed: %s\n", error->message);
        exit(1);
    }

    // Load configuration
    GKeyFile *keyfile = g_key_file_new();
    if ( !g_key_file_load_from_file(keyfile, config_path ? config_path : RASPIFPV_DEFAULT_CONFIG_PATH, 0, &error) ) {
        g_key_file_free(keyfile);
        keyfile = NULL;
        if ( config_path ) {
            g_print("Couldn't load config %s: %s\n", config_path, error->message);
            exit(1);
        }
    }

    // Init telemetry
    FPVTelemetryRX * telemetry_rx = init_telemetry_rx(keyfile);
    if ( !telemetry_rx ) {
        exit(1);
    }

    // Init renderer
    FPVRenderer * renderer = init_renderer(keyfile, telemetry_rx);
    if ( !renderer ) {
        exit(1);
    }

    // Init main loop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Init GStreamer
    gst_init(&argc, &argv);
    GstPipeline *pipeline = init_gst_pipeline(keyfile, renderer);
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(on_message), loop);
    gst_object_unref(GST_OBJECT(bus));

    // Start telemetry
    int started = fpv_telemetry_rx_listener_start(telemetry_rx);
    g_assert(started);

    // Start video pipeline
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

    // Run main loop
    g_main_loop_run(loop);

    // Stop video pipeline and clean up
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    gst_object_unref (pipeline);
    g_main_destroy(loop);
    fpv_telemetry_rx_dispose(telemetry_rx);
    if ( keyfile ) g_key_file_free(keyfile);

    return 0;
}
