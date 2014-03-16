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

#ifndef __RENDER_H
#define __RENDER_H

#include "telemetry_rx.h"
#include <cairo.h>
#include <stdint.h>

typedef struct _FPVRenderer FPVRenderer;

FPVRenderer * fpv_renderer_new(FPVTelemetryRX * telemetry_rx);
void fpv_renderer_dispose(FPVRenderer * renderer);

void fpv_renderer_render(FPVRenderer * renderer, cairo_t * context, uint64_t timestamp);

void fpv_renderer_set_frame_size(FPVRenderer * renderer, int width, int height);
void fpv_renderer_get_frame_size(FPVRenderer * renderer, int * width, int * height);

void fpv_renderer_set_show_altitude(FPVRenderer * renderer, int show_altitude);
int fpv_renderer_get_show_altitude(FPVRenderer * renderer);

#endif