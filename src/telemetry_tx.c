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

#include "telemetry_tx.h"
#include "common.h"
#include "telemetry_common.h"
#include "spi.h"
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static const float UPDATE_INTERVAL = 0.1;
static const int ADC_MAX = 1023;
static const double DEFAULT_SENSOR_MAX_VOLTS = 51.8;
static const double DEFAULT_SENSOR_MAX_AMPS = 89.4;
static const double DEFAULT_SENSOR_MIN_RSSI = -20.0;
static const double DEFAULT_SENSOR_MAX_RSSI = 0;
static const void * NO_SPI = (void*)1;

struct _FPVTelemetryTX {
    pthread_t thread;
    int running;
    struct sockaddr_in destaddr;
    SPIInterface *spi;

    int spi_bus;
    int spi_device;

    int voltage_channel;
    double max_volts;

    int current_channel;
    double max_amps;

    int rssi_channel;
    double min_rssi;
    double max_rssi;
};

#pragma mark - Forward declarations

static double fpv_telemetry_tx_read_channel(FPVTelemetryTX * tx, int channel);
static int fpv_telemetry_tx_check_power(FPVTelemetryTX * tx, FPVTelemetryUpdate *update);
static int fpv_telemetry_tx_check_rssi(FPVTelemetryTX * tx, FPVTelemetryUpdate *update);
static int fpv_telemetry_tx_check_position(FPVTelemetryTX * tx, FPVTelemetryUpdate *update);
static void fpv_telemetry_tx_send_update(int socket, struct sockaddr_in * destaddr, FPVTelemetryUpdate *update);
static void * fpv_telemetry_tx_thread_entry(void *userinfo);

#pragma mark -

FPVTelemetryTX * fpv_telemetry_tx_new(char * address, int port) {
    FPVTelemetryTX * tx = (FPVTelemetryTX*)calloc(1, sizeof(FPVTelemetryTX));
    tx->spi_bus = 0;
    tx->spi_device = 0;
    tx->voltage_channel = 0;
    tx->current_channel = 1;
    tx->rssi_channel = 2;
    tx->max_amps = DEFAULT_SENSOR_MAX_AMPS;
    tx->max_volts = DEFAULT_SENSOR_MAX_VOLTS;
    tx->destaddr.sin_family = AF_INET;
    if ( !inet_pton(AF_INET, address, &(tx->destaddr.sin_addr)) ) {
        fprintf(stderr, "Invalid telemetry address '%s'", address);
        free(tx);
        return NULL;
    }
    tx->destaddr.sin_port = htons(port);
    return tx;
}

void fpv_telemetry_tx_dispose(FPVTelemetryTX * tx) {
    if ( tx->spi && tx->spi != NO_SPI ) {
        spi_dispose(tx->spi);
    }
    if ( tx->running ) {
        fpv_telemetry_tx_sender_stop(tx);
    }
    free(tx);
}

int fpv_telemetry_tx_sender_start(FPVTelemetryTX * tx) {
    if ( tx->running ) {
        fprintf(stderr, "FPVTelemetryTX sender already running\n");
        return -1;
    }

    tx->running = 1;
    int result = pthread_create(&tx->thread, NULL, fpv_telemetry_tx_thread_entry, tx);
    if ( result != 0 ) {
        fprintf(stderr, "Unable to launch FPVTelemetryTX sender thread: %s\n", strerror(result));
    }

    char addrstr[128];
    printf("Sending telemetry to %s:%d\n", inet_ntop(AF_INET, &(tx->destaddr.sin_addr), addrstr, sizeof(addrstr)), ntohs(tx->destaddr.sin_port));

    return result == 0;
}

void fpv_telemetry_tx_sender_stop(FPVTelemetryTX * tx) {
    tx->running = 0;
    pthread_join(tx->thread, NULL);
}

int fpv_telemetry_tx_set_spi(FPVTelemetryTX * tx, int bus, int device) {
    if ( tx->spi && tx->spi != NO_SPI ) {
        spi_dispose(tx->spi);
    }
    tx->spi_bus = bus;
    tx->spi_device = device;
    tx->spi = spi_new(bus, device);
    if ( !tx->spi ) {
        tx->spi = (SPIInterface*)NO_SPI;
        fprintf(stderr, "SPI-based telemetry transmission will be disabled\n");
        return 0;
    }
    return 1;
}

void fpv_telemetry_tx_set_voltage_sensor(FPVTelemetryTX * tx, int adc_channel, double max_volts) {
    tx->voltage_channel = adc_channel;
    tx->max_volts = max_volts;
}

void fpv_telemetry_tx_set_current_sensor(FPVTelemetryTX * tx, int adc_channel, double max_amps) {
    tx->current_channel = adc_channel;
    tx->max_amps = max_amps;
}

void fpv_telemetry_tx_set_rssi_sensor(FPVTelemetryTX * tx, int adc_channel, double min_rssi, double max_rssi) {
    tx->rssi_channel = adc_channel;
    tx->min_rssi = min_rssi;
    tx->max_rssi = max_rssi;
}

void fpv_telemetry_tx_get_spi(FPVTelemetryTX * tx, int *bus, int *device) {
    if ( bus ) *bus = tx->spi_bus;
    if ( device ) *device = tx->spi_device;
}

void fpv_telemetry_tx_get_voltage_sensor(FPVTelemetryTX * tx, int *adc_channel, double *max_volts) {
    if ( adc_channel ) *adc_channel = tx->voltage_channel;
    if ( max_volts ) *max_volts = tx->max_volts;
}

void fpv_telemetry_tx_get_current_sensor(FPVTelemetryTX * tx, int *adc_channel, double *max_amps) {
    if ( adc_channel ) *adc_channel = tx->current_channel;
    if ( max_amps ) *max_amps = tx->max_amps;
}

void fpv_telemetry_tx_get_rssi_sensor(FPVTelemetryTX * tx, int *adc_channel, double *min_rssi, double *max_rssi) {
    if ( adc_channel ) *adc_channel = tx->rssi_channel;
    if ( min_rssi ) *min_rssi = tx->min_rssi;
    if ( max_rssi ) *max_rssi = tx->max_rssi;
}


static double fpv_telemetry_tx_read_channel(FPVTelemetryTX * tx, int channel) {
    if ( tx->spi == NO_SPI ) return 0.0;

    if ( !tx->spi ) {
        tx->spi = spi_new(tx->spi_bus, tx->spi_device);
        if ( !tx->spi ) {
            fprintf(stderr, "SPI-based telemetry transmission will be disabled\n");
            tx->spi = (SPIInterface*)NO_SPI;
            return 0.0;
        }
    }

    uint8_t outbuf[3];
    uint8_t inbuf[3] = {1, (8+channel) << 4, 0};
    spi_transaction(tx->spi, inbuf, outbuf, sizeof(outbuf));
    int output = ((outbuf[1] & 3) << 8) + outbuf[2];
    return (double)output / (double)ADC_MAX;
}

static int fpv_telemetry_tx_check_power(FPVTelemetryTX * tx, FPVTelemetryUpdate *update) {
    double voltage = fpv_telemetry_tx_read_channel(tx, tx->voltage_channel) / tx->max_volts;
    double current = fpv_telemetry_tx_read_channel(tx, tx->current_channel) / tx->max_amps;
    if ( voltage > 0.0 || current > 0.0 ) {
        update->type = TELEMETRY_TYPE_POWER;
        update->content.power.voltage = voltage;
        update->content.power.current = current;
        return 1;
    }
    return 0;
}

static int fpv_telemetry_tx_check_rssi(FPVTelemetryTX * tx, FPVTelemetryUpdate *update) {
    return 0;
}

static int fpv_telemetry_tx_check_position(FPVTelemetryTX * tx, FPVTelemetryUpdate *update) {
    return 0;
}

static void fpv_telemetry_tx_send_update(int socket, struct sockaddr_in * destaddr, FPVTelemetryUpdate *update) {
    XDR xdrs;
    char sendbuffer[1024];
    xdrmem_create(&xdrs, sendbuffer, sizeof(sendbuffer), XDR_ENCODE);
    if ( xdr_telemetry_update(&xdrs, update) ) {
        int length = xdr_getpos(&xdrs);
        sendto(socket, sendbuffer, length, 0, (struct sockaddr*)destaddr, sizeof(*destaddr));
    }
    xdr_destroy(&xdrs);
}

static void * fpv_telemetry_tx_thread_entry(void *userinfo) {
    FPVTelemetryTX *tx = (FPVTelemetryTX*)userinfo;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    u_char loop = 0;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    FPVTelemetryUpdate update;

    int update_interval = UPDATE_INTERVAL * 1e6;

    while ( tx->running ) {
        memset(&update, 0, sizeof(update));
        if ( fpv_telemetry_tx_check_power(tx, &update) ) {
            fpv_telemetry_tx_send_update(sock, &tx->destaddr, &update);
        }
        memset(&update, 0, sizeof(update));
        if ( fpv_telemetry_tx_check_rssi(tx, &update) ) {
            fpv_telemetry_tx_send_update(sock, &tx->destaddr, &update);
        }
        memset(&update, 0, sizeof(update));
        if ( fpv_telemetry_tx_check_position(tx, &update) ) {
            fpv_telemetry_tx_send_update(sock, &tx->destaddr, &update);
        }
        usleep(update_interval);
    }

    close(sock);
    sock = 0;
    tx->running = 0;
    return NULL;
}

