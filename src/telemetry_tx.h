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

#ifndef __TELEMETRY_TX_H
#define __TELEMETRY_TX_H

#include "telemetry_common.h"

typedef struct _FPVTelemetryTX FPVTelemetryTX;

FPVTelemetryTX * fpv_telemetry_tx_new(char * address, int port);
void fpv_telemetry_tx_dispose(FPVTelemetryTX * tx);

int fpv_telemetry_tx_sender_start(FPVTelemetryTX * tx);
void fpv_telemetry_tx_sender_stop(FPVTelemetryTX * tx);

int fpv_telemetry_tx_set_spi(FPVTelemetryTX * tx, int bus, int device);
void fpv_telemetry_tx_set_voltage_sensor(FPVTelemetryTX * tx, int adc_channel, double max_volts);
void fpv_telemetry_tx_set_current_sensor(FPVTelemetryTX * tx, int adc_channel, double max_amps);
void fpv_telemetry_tx_set_rssi_sensor(FPVTelemetryTX * tx, int adc_channel, double min_rssi, double max_rssi);

void fpv_telemetry_tx_get_spi(FPVTelemetryTX * tx, int *bus, int *device);
void fpv_telemetry_tx_get_voltage_sensor(FPVTelemetryTX * tx, int *adc_channel, double *max_volts);
void fpv_telemetry_tx_get_current_sensor(FPVTelemetryTX * tx, int *adc_channel, double *max_amps);
void fpv_telemetry_tx_get_rssi_sensor(FPVTelemetryTX * tx, int *adc_channel, double *min_rssi, double *max_rssi);

#endif