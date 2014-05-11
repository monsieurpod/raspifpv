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

#ifndef __CAIRO_TELEMETRY_RENDERER_H
#define __CAIRO_TELEMETRY_RENDERER_H

#include "telemetry_rx.h"
#include <stdint.h>

typedef struct _FPVCairoTelemetryRenderer FPVCairoTelemetryRenderer;

FPVCairoTelemetryRenderer * fpv_cairo_telemetry_renderer_new(FPVTelemetryRX * telemetry_rx);
void fpv_cairo_telemetry_renderer_dispose(FPVCairoTelemetryRenderer * renderer);

void fpv_cairo_telemetry_renderer_render(FPVCairoTelemetryRenderer * renderer, FPVEGLTelemetryRenderer_t * context, uint64_t timestamp);

void fpv_cairo_telemetry_renderer_set_frame_size(FPVCairoTelemetryRenderer * renderer, int width, int height);
void fpv_cairo_telemetry_renderer_get_frame_size(FPVCairoTelemetryRenderer * renderer, int * width, int * height);

void fpv_cairo_telemetry_renderer_set_show_altitude(FPVCairoTelemetryRenderer * renderer, int show_altitude);
int fpv_cairo_telemetry_renderer_get_show_altitude(FPVCairoTelemetryRenderer * renderer);

#endif