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

#ifndef __EGL_RENDERER_H
#define __EGL_RENDERER_H

#include <EGL/egl.h>
#include "telemetry_rx.h"

typedef struct _FPVEGLRenderer FPVEGLRenderer;

FPVEGLRenderer * fpv_egl_renderer_new(FPVTelemetryRX * telemetry_rx);
void fpv_egl_renderer_dispose(FPVEGLRenderer * renderer);

void fpv_egl_renderer_render(FPVEGLRenderer * renderer, egl_t * context, uint64_t timestamp);

void fpv_egl_renderer_set_show_altitude(FPVEGLRenderer * renderer, int show_altitude);
int fpv_egl_renderer_get_show_altitude(FPVEGLRenderer * renderer);

#endif