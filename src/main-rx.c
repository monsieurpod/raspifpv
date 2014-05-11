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
#include <glib.h>
#include <stdio.h>
#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "gstreamer_renderer.h"
#include "egl_telemetry_renderer.h"
#include "telemetry_rx.h"

static FPVGStreamerRenderer* init_renderer(GKeyFile * keyfile, GMainLoop *loop) {
    char * multicast_addr = keyfile ? g_key_file_get_string(keyfile, "Networking", "multicast_address", NULL) : NULL;
    int port = keyfile ? g_key_file_get_integer(keyfile, "Networking", "video_port", NULL) : 0;
    
    if ( !multicast_addr ) multicast_addr = RASPIFPV_MULTICAST_ADDR;
    if ( !port ) port = RASPIFPV_PORT_VIDEO;
    
	FPVGStreamerRenderer *renderer = fpv_gstreamer_renderer_new(loop, multicast_addr, port);
    return renderer;
}

static FPVTelemetryRX* init_telemetry_rx(GKeyFile *keyfile) {
    char *address = keyfile ? g_key_file_get_string(keyfile, "Networking", "multicast_address", NULL) : NULL;
    int port = keyfile ? g_key_file_get_integer(keyfile, "Networking", "telemetry_port", NULL) : 0;
    
    FPVTelemetryRX *telemetry_rx = fpv_telemetry_rx_new(address ? address : RASPIFPV_MULTICAST_ADDR, port ? port : RASPIFPV_PORT_TELEMETRY);

    return telemetry_rx;
}

static FPVEGLTelemetryRenderer* init_telemetry_renderer(GKeyFile * keyfile, FPVTelemetryRX * telemetry) {
	FPVEGLTelemetryRenderer * renderer = fpv_egl_telemetry_renderer_new(telemetry);
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

    // Init telemetry receiver
    FPVTelemetryRX * telemetry_rx = init_telemetry_rx(keyfile);
    if ( !telemetry_rx ) {
        exit(1);
    }

    // Init telemetry renderer
    FPVEGLTelemetryRenderer * telemetry_renderer = init_telemetry_renderer(keyfile, telemetry_rx);
    if ( !telemetry_renderer ) {
        exit(1);
    }

    // Init main loop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Init renderer
    gst_init(&argc, &argv);
	FPVGStreamerRenderer *renderer = init_renderer(keyfile, loop);

    // Start telemetry receiver
    int started = fpv_telemetry_rx_listener_start(telemetry_rx);
    g_assert(started);

    // Start video pipeline
	fpv_gstreamer_renderer_start(renderer);

    // Run main loop
    g_main_loop_run(loop);

    // Stop video pipeline and clean up
	fpv_gstreamer_renderer_stop(renderer);
    g_main_destroy(loop);
	fpv_gstreamer_renderer_dispose(renderer);
	fpv_egl_telemetry_renderer_dispose(telemetry_renderer);
    fpv_telemetry_rx_dispose(telemetry_rx);
    if ( keyfile ) g_key_file_free(keyfile);

    return 0;
}
