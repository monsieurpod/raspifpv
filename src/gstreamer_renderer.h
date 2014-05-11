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

#ifndef __GSTREAMER_RENDERER_H
#define __GSTREAMER_RENDERER_H

#include <glib.h>

typedef struct _FPVGStreamerRenderer FPVGStreamerRenderer;

FPVGStreamerRenderer * fpv_gstreamer_renderer_new(GMainLoop * loop, char * multicast_addr, int port);
void fpv_gstreamer_renderer_dispose(FPVGStreamerRenderer * renderer);

void fpv_gstreamer_renderer_start(FPVGStreamerRenderer * gstrx);
void fpv_gstreamer_renderer_stop(FPVGStreamerRenderer * gstrx);

#endif