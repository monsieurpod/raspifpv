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
#include <glib.h>
#include <stdio.h>
#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "telemetry_tx.h"

static const int DEFAULT_VIDEO_WIDTH = 1280;
static const int DEFAULT_VIDEO_HEIGHT = 720;
static const int DEFAULT_VIDEO_FRAMERATE = 30;
static const int DEFAULT_VIDEO_BITRATE = 1048576;

static const char * GST_PIPELINE_SOURCE = "v4l2src ! video/x-raw, width=%d, height=%d, framerate=%d/1 ! queue ! videoconvert ! omxh264enc target-bitrate=%d control-rate=1";
static const char * GST_PIPELINE_TRANSMIT = "rtph264pay !  udpsink host=%s port=%d";

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

static FPVTelemetryTX* init_telemetry_tx(GKeyFile *keyfile) {
    char *address = keyfile ? g_key_file_get_string(keyfile, "Networking", "multicast_address", NULL) : NULL;
    int port = keyfile ? g_key_file_get_integer(keyfile, "Networking", "telemetry_port", NULL) : 0;
    FPVTelemetryTX *telemetry_tx = fpv_telemetry_tx_new(address ? address : RASPIFPV_MULTICAST_ADDR, port ? port : RASPIFPV_PORT_TELEMETRY);

    // Setup telemetry
    if ( telemetry_tx && keyfile ) {
        int spi_bus = keyfile ? g_key_file_get_integer(keyfile, "Telemetry", "spi_bus", NULL) : 0;
        int spi_device = keyfile ? g_key_file_get_integer(keyfile, "Telemetry", "spi_device", NULL) : 0;
        if ( spi_bus != 0 || spi_device != 0 ) {
            if ( !fpv_telemetry_tx_set_spi(telemetry_tx, spi_bus, spi_device) ) {
                g_print("Couldn't connect to SPI bus %d, device %d. Please check your configuration.", spi_bus, spi_device);
                exit(1);
            }
        }

        GError *error = NULL;
        int voltage_adc_channel = keyfile ? g_key_file_get_integer(keyfile, "Telemetry", "voltage_adc_channel", &error) : 0;
        int has_voltage_adc_channel = error != NULL;
        error = NULL;
        double voltage_sensor_max = keyfile ? g_key_file_get_double(keyfile, "Telemetry", "voltage_sensor_max", &error) : 0;
        int has_voltage_sensor_max = error != NULL;
        if ( has_voltage_adc_channel || has_voltage_sensor_max ) {
            fpv_telemetry_tx_get_voltage_sensor(telemetry_tx, has_voltage_adc_channel ? NULL : &voltage_adc_channel, has_voltage_sensor_max ? NULL : &voltage_sensor_max);
            fpv_telemetry_tx_set_voltage_sensor(telemetry_tx, voltage_adc_channel, voltage_sensor_max);
        }

        error = NULL;
        int current_adc_channel = keyfile ? g_key_file_get_integer(keyfile, "Telemetry", "current_adc_channel", &error) : 0;
        int has_current_adc_channel = error != NULL;
        error = NULL;
        double current_sensor_max = keyfile ? g_key_file_get_double(keyfile, "Telemetry", "current_sensor_max", &error) : 0;
        int has_current_sensor_max = error != NULL;
        if ( has_current_adc_channel || has_current_sensor_max ) {
            fpv_telemetry_tx_get_current_sensor(telemetry_tx, has_current_adc_channel ? NULL : &current_adc_channel, has_current_sensor_max ? NULL : &current_sensor_max);
            fpv_telemetry_tx_set_current_sensor(telemetry_tx, current_adc_channel, current_sensor_max);
        }

        error = NULL;
        int rssi_adc_channel = keyfile ? g_key_file_get_integer(keyfile, "Telemetry", "rssi_adc_channel", &error) : 0;
        int has_rssi_adc_channel = error != NULL;
        error = NULL;
        double rssi_sensor_max = keyfile ? g_key_file_get_double(keyfile, "Telemetry", "rssi_sensor_max", &error) : 0;
        int has_rssi_sensor_max = error != NULL;
        error = NULL;
        double rssi_sensor_min = keyfile ? g_key_file_get_double(keyfile, "Telemetry", "rssi_sensor_min", &error) : 0;
        int has_rssi_sensor_min = error != NULL;
        if ( has_rssi_adc_channel || has_rssi_sensor_max ) {
            fpv_telemetry_tx_get_rssi_sensor(telemetry_tx, has_rssi_adc_channel ? NULL : &rssi_adc_channel, has_rssi_sensor_min ? NULL : &rssi_sensor_min, has_rssi_sensor_max ? NULL : &rssi_sensor_max);
            fpv_telemetry_tx_set_rssi_sensor(telemetry_tx, rssi_adc_channel, rssi_sensor_min, rssi_sensor_max);
        }
    }

    return telemetry_tx;
}

static GstPipeline* init_gst_pipeline(GKeyFile *keyfile) {

    int video_width = keyfile ? g_key_file_get_integer(keyfile, "Video", "video_width", NULL) : 0;
    int video_height = keyfile ? g_key_file_get_integer(keyfile, "Video", "video_height", NULL) : 0;
    int video_framerate = keyfile ? g_key_file_get_integer(keyfile, "Video", "video_framerate", NULL) : 0;
    int video_bitrate = keyfile ? g_key_file_get_integer(keyfile, "Video", "video_bitrate", NULL) : 0;
    char * multicast_addr = keyfile ? g_key_file_get_string(keyfile, "Networking", "multicast_address", NULL) : NULL;
    int port = keyfile ? g_key_file_get_integer(keyfile, "Networking", "video_port", NULL) : 0;
    char * source_pipeline = keyfile ? g_key_file_get_string(keyfile, "Video", "sender_source_pipeline", NULL) : NULL;
    
    if ( !multicast_addr ) multicast_addr = RASPIFPV_MULTICAST_ADDR;
    if ( !port ) port = RASPIFPV_PORT_VIDEO;
    if ( !source_pipeline ) source_pipeline = (char*)GST_PIPELINE_SOURCE;

    // Make sure source pipeline is valid
    char * s;
    int placeholder_count = 0;
    for ( s=source_pipeline; s=strstr(s, "%d"); placeholder_count++, s++ );
    if ( placeholder_count != 4 ) {
        g_print("Error: sender_source_pipeline must have four '%%d' placeholders in order: width, height, framerate, bitrate");
        exit(1);
    }

    // Parse and create pipeline
    char pipeline_description[1024];
    if ( strlen(source_pipeline)+30+3+strlen(GST_PIPELINE_TRANSMIT)+20 > sizeof(pipeline_description) ) {
        g_print("Error: sender_source_pipeline is too long");
        exit(1);
    }

    snprintf(pipeline_description, sizeof(pipeline_description), source_pipeline, 
        video_width ? video_width : DEFAULT_VIDEO_WIDTH, 
        video_height ? video_height : DEFAULT_VIDEO_HEIGHT,
        video_framerate ? video_framerate : DEFAULT_VIDEO_FRAMERATE,
        video_bitrate ? video_bitrate : DEFAULT_VIDEO_BITRATE);
    strcat(pipeline_description, " ! ");
    snprintf(pipeline_description+strlen(pipeline_description), sizeof(pipeline_description)-strlen(pipeline_description), GST_PIPELINE_TRANSMIT, multicast_addr, port);

    GError *error = NULL;
    GstPipeline *pipeline = GST_PIPELINE(gst_parse_launch(pipeline_description, &error));
    if ( !pipeline ) {
        g_error("Could not create pipeline %s: %s", pipeline_description, error->message);
    }

    printf("Sending video to %s:%d\n", multicast_addr, port);

    return pipeline;
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
    FPVTelemetryTX *telemetry_tx = init_telemetry_tx(keyfile);
    if ( !telemetry_tx ) {
        exit(1);
    }

    // Init main loop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Init GStreamer
    gst_init(&argc, &argv);
    GstPipeline *pipeline = init_gst_pipeline(keyfile);
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message", G_CALLBACK(on_message), loop);
    gst_object_unref(GST_OBJECT(bus));

    // Start telemetry
    int started = fpv_telemetry_tx_sender_start(telemetry_tx);
    g_assert(started);

    // Start video pipeline
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

    // Run main loop
    g_main_loop_run (loop);

    // Stop video pipeline and clean up
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    gst_object_unref (pipeline);
    g_main_destroy(loop);
    fpv_telemetry_tx_dispose(telemetry_tx);
    if ( keyfile ) g_key_file_free(keyfile);
    
    return 0;
}
