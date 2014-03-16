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

#ifndef __TELEMETRY_COMMON_H
#define __TELEMETRY_COMMON_H

#include <rpc/types.h>
#include <rpc/xdr.h>

enum {
    TELEMETRY_TYPE_POSITION,
    TELEMETRY_TYPE_POWER,
    TELEMETRY_TYPE_SIGNAL
};

struct telemetry_position_t {
    double latitude;
    double longitude;
    double altitude;
    double bearing;
};

struct telemetry_power_t {
    double voltage;
    double current;
};

struct telemetry_signal_t {
    double rssi;
};

typedef struct telemetry_update_t {
    unsigned char type;
    union {
        struct telemetry_position_t position;
        struct telemetry_power_t power;
        struct telemetry_signal_t signal;
    } content;
} FPVTelemetryUpdate;

int xdr_telemetry_update(XDR * xdrs, struct telemetry_update_t *header);

#endif