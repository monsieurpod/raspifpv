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

#include "cairo_telemetry_renderer.h"
#include "geometry.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

struct _FPVCairoRenderer {
    int width;
    int height;
    FPVTelemetryRX *telemetry_rx;
    int show_altitude;
};

typedef enum {
    ALIGNMENT_LEFT,
    ALIGNMENT_CENTER,
    ALIGNMENT_RIGHT
} Alignment;

FPVCairoRenderer * fpv_cairo_telemetry_renderer_new(FPVTelemetryRX * telemetry_rx) {
    FPVCairoRenderer *renderer = (FPVCairoRenderer*)calloc(1, sizeof(FPVCairoRenderer));
    renderer->telemetry_rx = telemetry_rx;
    renderer->show_altitude = 0;
    return renderer;
}

void fpv_cairo_telemetry_renderer_dispose(FPVCairoRenderer * renderer) {
    free(renderer);
}

void fpv_cairo_telemetry_renderer_set_frame_size(FPVCairoRenderer * renderer, int width, int height) {
    renderer->width = width;
    renderer->height = height;
}

void fpv_cairo_telemetry_renderer_get_frame_size(FPVCairoRenderer * renderer, int * width, int * height) {
    if ( width ) *width = renderer->width;
    if ( height ) *height = renderer->height;
}

void fpv_cairo_telemetry_renderer_set_show_altitude(FPVCairoRenderer * renderer, int show_altitude) {
    renderer->show_altitude = show_altitude;
}

int fpv_cairo_telemetry_renderer_get_show_altitude(FPVCairoRenderer * renderer) {
    return renderer->show_altitude;
}

static void render_text(cairo_t * cr, float x, float y, const char * text, Alignment alignment) {
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text, &extents);
    x = alignment == ALIGNMENT_LEFT ? x : alignment == ALIGNMENT_CENTER ? (x - extents.width) / 2.0 : x - extents.width;

    // Draw outline
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
    cairo_move_to(cr, x - 1, y - 1);
    cairo_show_text(cr, text);
    cairo_move_to(cr, x + 1, y + 1);
    cairo_show_text(cr, text);
    cairo_move_to(cr, x - 1, y + 1);
    cairo_show_text(cr, text);
    cairo_move_to(cr, x + 1, y - 1);
    cairo_show_text(cr, text);

    // Draw fill
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}

void fpv_cairo_telemetry_renderer_render(FPVCairoRenderer * renderer, cairo_t * cr, uint64_t timestamp) {
    telemetry_rx_t telemetry = fpv_telemetry_rx_get(renderer->telemetry_rx);

    double home_distance;
    double home_angle_horiz;
    double home_angle_vert;

    // Calculate geometry
    if ( telemetry.home_location.latitude == 0.0 ) {
        home_distance = 0.0;
        home_angle_horiz = (((int)(timestamp/1e7) % 300) / 300.0) * 2.0 * M_PI;
        home_angle_vert = 0.0;
    } else {
        home_distance = geom_distance_between_coordinates(telemetry.location.latitude, telemetry.location.longitude, telemetry.home_location.latitude, telemetry.home_location.longitude);
        home_angle_horiz = (fmod((geom_bearing_between_coordinates(telemetry.location.latitude, telemetry.location.longitude, telemetry.home_location.latitude, telemetry.home_location.longitude) - telemetry.bearing), 360.0)) * (M_PI/180.0);
        home_angle_vert = telemetry.location.altitude == telemetry.home_location.altitude ? 0.0 : (fmod(atan(home_distance / (telemetry.location.altitude - telemetry.home_location.altitude)) + M_PI, M_PI) - (M_PI / 2.0));
    }

    // Prepare arrow transform based on geometry
    GEOMMatrix4 perspective = geom_matrix4_perspective(M_PI / 2, 4.0 / 3.0, 0.01, 10.0);
    GEOMMatrix4 translate = geom_matrix4_translation(0, 0.7, 1);
    GEOMMatrix4 rotate = geom_matrix4_multiply(geom_matrix4_multiply(geom_matrix4_multiply(
                            geom_matrix4_translation(0, 0, 0.5), 
                            geom_matrix4_rotation_y(home_angle_horiz + M_PI)),
                            geom_matrix4_rotation_x(home_angle_vert)),
                            geom_matrix4_translation(0, 0, -0.5));
    GEOMMatrix4 screen = geom_matrix4_multiply(geom_matrix4_multiply(
                            geom_matrix4_translation(renderer->width / 2.0, 0, 0),
                            geom_matrix4_scale(renderer->width * 0.07, renderer->width * 0.07, renderer->width * 0.07)),
                            geom_matrix4_translation(0, 1.0, 0));
    GEOMMatrix4 transform = geom_matrix4_multiply(geom_matrix4_multiply(geom_matrix4_multiply(
                            screen, 
                            perspective), 
                            translate), 
                            rotate);

    // Prepare rendering
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, renderer->height * 0.03);

    // Render arrow
    const double shaft_width = 0.8;
    const double arrow_width = 1.5;
    const double arrow_length_ratio = 0.6;
    GEOMPoint3 points[] = {
        {0, 0, 0},
        {arrow_width / 2, 0, arrow_length_ratio},
        {shaft_width / 2, 0, arrow_length_ratio},
        {shaft_width / 2, 0, 1.0},
        {-shaft_width / 2, 0, 1.0},
        {-shaft_width / 2, 0, arrow_length_ratio},
        {-arrow_width / 2, 0, arrow_length_ratio}};

    GEOMPoint3 initial_point = geom_matrix4_transform(transform, points[0]);
    cairo_move_to(cr, initial_point.x, initial_point.y);
    int i;
    for ( i=1; i<(sizeof(points)/sizeof(GEOMPoint3)); i++ ) {
        GEOMPoint3 point = geom_matrix4_transform(transform, points[i]);
        cairo_line_to(cr, point.x, point.y);
    }
    cairo_close_path(cr);

    // Render outline
    cairo_set_line_width(cr, renderer->width * 0.005);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
    cairo_stroke_preserve(cr);

    // Render fill
    cairo_set_line_width(cr, renderer->width * 0.003);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_stroke(cr);

    // Render labels
    if ( telemetry.location.latitude > 0 ) {
        char text[128];
        snprintf(text, sizeof(text), "%d m", (int)home_distance);
        render_text(cr, renderer->width / 2.0, renderer->height * 0.14, text, ALIGNMENT_CENTER);
    }

    if ( telemetry.voltage > 0 ) {
        char text[128];
        snprintf(text, sizeof(text), "%0.2fV / %0.2fA", telemetry.voltage, telemetry.current);
        render_text(cr, renderer->height * 0.05, renderer->height * 0.05, text, ALIGNMENT_LEFT);
    }

    if ( telemetry.rssi > 0 ) {
        char text[128];
        snprintf(text, sizeof(text), "%0.2fdB RSSI", telemetry.rssi);
        render_text(cr, renderer->width - renderer->height * 0.05, renderer->height * 0.05, text, ALIGNMENT_RIGHT);
    }

    if ( renderer->show_altitude && telemetry.location.altitude > 0 ) {
        char text[128];
        snprintf(text, sizeof(text), "%d m alt", (int)telemetry.location.altitude);
        render_text(cr, renderer->width * 0.75, renderer->height * 0.14, text, ALIGNMENT_CENTER);
    }
}
