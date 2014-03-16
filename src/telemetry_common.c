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

#include "telemetry_common.h"

int xdr_telemetry_update(XDR * xdrs, struct telemetry_update_t *header) {
    if ( !xdr_u_char(xdrs, &header->type) ) return 0;
    switch ( header->type ) {
        case TELEMETRY_TYPE_POSITION:
            return xdr_double(xdrs, &header->content.position.latitude)
                && xdr_double(xdrs, &header->content.position.longitude)
                && xdr_double(xdrs, &header->content.position.altitude)
                && xdr_double(xdrs, &header->content.position.bearing);
            break;
        case TELEMETRY_TYPE_POWER:
            return xdr_double(xdrs, &header->content.power.voltage)
                && xdr_double(xdrs, &header->content.power.current);
            break;
        case TELEMETRY_TYPE_SIGNAL:
            return xdr_double(xdrs, &header->content.signal.rssi);
            break;
        default:
            return 0;
    }
}