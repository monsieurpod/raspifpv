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

#include "telemetry_rx.h"
#include "common.h"
#include "telemetry_common.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

struct _FPVTelemetryRX {
    telemetry_rx_t telemetry;
    pthread_t thread;
    struct sockaddr_in sourceaddr;
    int running;
    FPVTelemetryRXCallback callback;
};

static void * thread_entry(void *userinfo) {
    FPVTelemetryRX *rx = (FPVTelemetryRX*)userinfo;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &(struct timeval) { .tv_sec = 0, .tv_usec = 100 }, sizeof(struct timeval));

    bind(sock, (struct sockaddr*)&rx->sourceaddr, sizeof(rx->sourceaddr));

    struct ip_mreq group;
    memset(&group, 0, sizeof(group));
    group.imr_multiaddr.s_addr = rx->sourceaddr.sin_addr.s_addr;
    group.imr_interface.s_addr = htonl(INADDR_ANY);
    if ( setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group)) < 0 ) {
        fprintf(stderr, "Unable to join multicast group for telemetry: %s\n", strerror(errno));
        rx->running = 0;
        return NULL;
    }

    char recvbuffer[1024];
    FPVTelemetryUpdate update;

    while ( rx->running ) {
        int result = recvfrom(sock, recvbuffer, sizeof(recvbuffer), 0, NULL, NULL);
        if ( result == -1 ) {
            continue;
        }

        XDR xdrs;
        xdrmem_create(&xdrs, recvbuffer, result, XDR_DECODE);
        if ( xdr_telemetry_update(&xdrs, &update) ) {
            switch ( update.type ) {
                case TELEMETRY_TYPE_POSITION:
                    rx->telemetry.location.latitude = update.content.position.latitude;
                    rx->telemetry.location.longitude = update.content.position.longitude;
                    rx->telemetry.location.altitude = update.content.position.altitude;
                    rx->telemetry.bearing = update.content.position.bearing;
                    if ( rx->telemetry.home_location.latitude == 0 ) {
                        rx->telemetry.home_location = rx->telemetry.location;
                    }
                    break;
                case TELEMETRY_TYPE_POWER:
                    rx->telemetry.voltage = update.content.power.voltage;
                    rx->telemetry.current = update.content.power.current;
                    break;
                case TELEMETRY_TYPE_SIGNAL:
                    rx->telemetry.rssi = update.content.signal.rssi;
                    break;
            }
            if ( rx->callback ) {
                rx->callback(rx, &update);
            }
        }
        xdr_destroy(&xdrs);
    }

    close(sock);
    sock = 0;
    rx->running = 0;
    return NULL;
}

FPVTelemetryRX * fpv_telemetry_rx_new(char * address, int port) {
    FPVTelemetryRX * rx = (FPVTelemetryRX*)calloc(1, sizeof(FPVTelemetryRX));
    if ( !inet_pton(AF_INET, address, &(rx->sourceaddr.sin_addr)) ) {
        fprintf(stderr, "Invalid telemetry address '%s'", address);
        free(rx);
        return NULL;
    }
    rx->sourceaddr.sin_port = htons(port);
    return rx;
}

void fpv_telemetry_rx_dispose(FPVTelemetryRX * rx) {
    if ( rx->running ) {
        fpv_telemetry_rx_listener_stop(rx);
    }
    free(rx);
}

void fpv_telemetry_rx_set_callback(FPVTelemetryRX * rx, FPVTelemetryRXCallback callback) {
    rx->callback = callback;
}

telemetry_rx_t fpv_telemetry_rx_get(FPVTelemetryRX * rx) {
    return rx->telemetry;
}

int fpv_telemetry_rx_listener_start(FPVTelemetryRX * rx) {
    if ( rx->running ) {
        fprintf(stderr, "FPVTelemetryRX listener already running\n");
        return -1;
    }

    rx->running = 1;
    int result = pthread_create(&rx->thread, NULL, thread_entry, rx);
    if ( result != 0 ) {
        fprintf(stderr, "Unable to launch FPVTelemetryRX listener thread: %s\n", strerror(result));
    }

    char addrstr[128];
    printf("Listening for telemetry at %s:%d\n", inet_ntop(AF_INET, &(rx->sourceaddr.sin_addr), addrstr, sizeof(addrstr)), ntohs(rx->sourceaddr.sin_port));

    return result == 0;
}

void fpv_telemetry_rx_listener_stop(FPVTelemetryRX * rx) {
    rx->running = 0;
    pthread_join(rx->thread, NULL);
}

