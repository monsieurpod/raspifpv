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
#include <gst/gst.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char * oculus_rift_frag_shader = 
    "#extension GL_ARB_texture_rectangle : enable                                     \n"
    "precision mediump float;                                                         \n"
    "varying vec2 v_texcoord;                                                         \n"
    "uniform sampler2D tex;                                                           \n"
    "                                                                                 \n"
    "const vec4 kappa = vec4(1.0,1.7,0.7,15.0);                                       \n"
    "                                                                                 \n"
    "const float screen_width = 1920.0;                                               \n"
    "const float screen_height = 1080.0;                                              \n"
    "                                                                                 \n"
    "const float scaleFactor = 0.9;                                                   \n"
    "                                                                                 \n"
    "const vec2 leftCenter = vec2(0.25, 0.5);                                         \n"
    "const vec2 rightCenter = vec2(0.75, 0.5);                                        \n"
    "                                                                                 \n"
    "const float separation = -0.05;                                                  \n"
    "                                                                                 \n"
    "const bool stereo_input = false;                                                 \n"
    "                                                                                 \n"
    "// Scales input texture coordinates for distortion.                              \n"
    "vec2 hmdWarp(vec2 LensCenter, vec2 texCoord, vec2 Scale, vec2 ScaleIn) {         \n"
    "    vec2 theta = (texCoord - LensCenter) * ScaleIn;                              \n"
    "    float rSq = theta.x * theta.x + theta.y * theta.y;                           \n"
    "    vec2 rvector = theta * (kappa.x + kappa.y * rSq +                            \n"
    "          kappa.z * rSq * rSq + kappa.w * rSq * rSq * rSq);                      \n"
    "    vec2 tc = LensCenter + Scale * rvector;                                      \n"
    "    return tc;                                                                   \n"
    "}                                                                                \n"
    "                                                                                 \n"
    "bool validate(vec2 tc, int eye) {                                                \n"
    "    if ( stereo_input ) {                                                        \n"
    "        //keep within bounds of texture                                          \n"
    "        if ((eye == 1 && (tc.x < 0.0 || tc.x > 0.5)) ||                          \n"
    "            (eye == 0 && (tc.x < 0.5 || tc.x > 1.0)) ||                          \n"
    "            tc.y < 0.0 || tc.y > 1.0) {                                          \n"
    "            return false;                                                        \n"
    "        }                                                                        \n"
    "    } else {                                                                     \n"
    "        if ( tc.x < 0.0 || tc.x > 1.0 ||                                         \n"
    "             tc.y < 0.0 || tc.y > 1.0 ) {                                        \n"
    "             return false;                                                       \n"
    "        }                                                                        \n"
    "    }                                                                            \n"
    "    return true;                                                                 \n"
    "}                                                                                \n"
    "                                                                                 \n"
    "void main() {                                                                    \n"
    "    float as = float(screen_width / 2.0) / float(screen_height);                 \n"
    "    vec2 Scale = vec2(0.5, as);                                                  \n"
    "    vec2 ScaleIn = vec2(2.0 * scaleFactor, 1.0 / as * scaleFactor);              \n"
    "                                                                                 \n"
    "    vec2 texCoord = v_texcoord;                                                  \n"
    "                                                                                 \n"
    "    vec2 tc = vec2(0);                                                           \n"
    "    vec4 color = vec4(0);                                                        \n"
    "                                                                                 \n"
    "    if ( texCoord.x < 0.5 ) {                                                    \n"
    "        texCoord.x += separation;                                                \n"
    "        texCoord = hmdWarp(leftCenter, texCoord, Scale, ScaleIn );               \n"
    "                                                                                 \n"
    "        if ( !stereo_input ) {                                                   \n"
    "            texCoord.x *= 2.0;                                                   \n"
    "        }                                                                        \n"
    "                                                                                 \n"
    "        color = texture2D(tex, texCoord);                                        \n"
    "                                                                                 \n"
    "        if ( !validate(texCoord, 0) ) {                                          \n"
    "            color = vec4(0);                                                     \n"
    "        }                                                                        \n"
    "    } else {                                                                     \n"
    "        texCoord.x -= separation;                                                \n"
    "        texCoord = hmdWarp(rightCenter, texCoord, Scale, ScaleIn);               \n"
    "                                                                                 \n"
    "        if ( !stereo_input ) {                                                   \n"
    "            texCoord.x = (texCoord.x - 0.5) * 2.0;                               \n"
    "        }                                                                        \n"
    "                                                                                 \n"
    "        color = texture2D(tex, texCoord);                                        \n"
    "                                                                                 \n"
    "        if ( !validate(texCoord, 1) ) {                                          \n"
    "            color = vec4(0);                                                     \n"
    "        }                                                                        \n"
    "    }                                                                            \n"
    "                                                                                 \n"
    "    gl_FragColor = color;                                                        \n"
    "}                                                                                \n";

const char * shader_source_tmp_path = "/tmp/raspifpv-shader.frag";

struct _FPVGStreamerRenderer {
    GstPipeline * pipeline;
};

static const char * GST_PIPELINE_RECEIVE = "udpsrc %s port=%d caps=\"application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264\" ! rtph264depay";
static const char * GST_PIPELINE_DECODE = "h264parse ! omxh264dec";
static const char * GST_PIPELINE_SHADER = "glshader name=shader";
static const char * GST_PIPELINE_SINK = "glimagesink sync=false name=sink";

#pragma mark -
#pragma mark Forward declarations

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
    strcat(pipeline_description, GST_PIPELINE_SHADER);
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

    // Load shader (write to temporary file)
    GstElement *shader = GST_ELEMENT(gst_bin_get_by_name(GST_BIN(renderer->pipeline), "shader"));
    g_assert(shader);

    FILE * fd = fopen(shader_source_tmp_path, "w");
    fprintf(fd, "%s", oculus_rift_frag_shader);
    fclose(fd);
    g_object_set(shader, "location", shader_source_tmp_path, NULL);
    
    printf("Listening for video at %s:%d\n", multicast_addr && strlen(multicast_addr) > 0 ? multicast_addr : "0.0.0.0", port);
    
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(renderer->pipeline));
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(on_message), loop);
    gst_object_unref(GST_OBJECT(bus));
    
    return renderer;
}

void fpv_gstreamer_renderer_dispose(FPVGStreamerRenderer * renderer) {
    gst_object_unref(renderer->pipeline);
    free(renderer);
}

void fpv_gstreamer_renderer_start(FPVGStreamerRenderer * renderer) {
    gst_element_set_state(GST_ELEMENT(renderer->pipeline), GST_STATE_PLAYING);
}

void fpv_gstreamer_renderer_stop(FPVGStreamerRenderer * renderer) {
    gst_element_set_state(GST_ELEMENT(renderer->pipeline), GST_STATE_NULL);
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