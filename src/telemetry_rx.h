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

#ifndef __TELEMETRY_RX_H
#define __TELEMETRY_RX_H

#include "telemetry_common.h"

typedef struct {
    double latitude;
    double longitude;
    double altitude;
} telemetry_coord;

typedef struct {
    telemetry_coord location;
    double bearing;
    telemetry_coord home_location;

    double voltage;
    double current;

    double rssi;
} telemetry_rx_t;

typedef struct _FPVTelemetryRX FPVTelemetryRX;

typedef void (*FPVTelemetryRXCallback)(FPVTelemetryRX * rx, FPVTelemetryUpdate * update, void * context);

FPVTelemetryRX * fpv_telemetry_rx_new(char * address, int port);
void fpv_telemetry_rx_dispose(FPVTelemetryRX * rx);

void fpv_telemetry_rx_set_callback(FPVTelemetryRX * rx, FPVTelemetryRXCallback callback, void * context);

telemetry_rx_t fpv_telemetry_rx_get(FPVTelemetryRX * rx);

int fpv_telemetry_rx_listener_start(FPVTelemetryRX * rx);
void fpv_telemetry_rx_listener_stop(FPVTelemetryRX * rx);

#endif