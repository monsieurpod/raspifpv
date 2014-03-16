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

#include "geometry.h"
#include <math.h>
#include <assert.h>

double geom_distance_between_coordinates(double lat1, double lon1, double lat2, double lon2) {
    // Implements Haversine formula (http://www.movable-type.co.uk/scripts/latlong.html); result in metres
    double R = 6371;
    double dLat = (lat2 - lat1) * (M_PI/180.0);
    double dLon = (lon2 - lon1) * (M_PI/180.0);
    lat1 = lat1 * (M_PI/180.0);
    lat2 = lat2 * (M_PI/180.0);

    double a = sin(dLat / 2.0) * sin(dLat / 2.0) + sin(dLon / 2.0) * sin(dLon / 2.0) * cos(lat1) * cos(lat2);
    double c = 2 * atan2(sqrt(a), sqrt(1.0 - a));
    return R * c * 1000.0;
}

double geom_bearing_between_coordinates(double lat1, double lon1, double lat2, double lon2) {
    // Calculate forward azimuth (http://www.movable-type.co.uk/scripts/latlong.html); result in degrees, 0 degrees is north
    double dLon = (lon2 - lon1) * (M_PI/180.0);
    lat1 = lat1 * (M_PI/180.0);
    lat2 = lat2 * (M_PI/180.0);
    double y = sin(dLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);
    return fmod((atan2(y, x) * (180.0/M_PI)) + 360.0, 360.0);
}


/*
 * The following is a partial port of the Euclid graphics maths module
 *
 * Copyright (c) 2006 Alex Holkner
 * Alex.Holkner@mail.google.com
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA 
 */

GEOMMatrix4 geom_matrix4_identity() {
    GEOMMatrix4 m;
    m.a = m.f = m.k = m.p = 1.0;
    m.b = m.c = m.d = m.e = m.g = m.h = m.i = m.j = m.l = m.m = m.n = m.o = 0.0;
    return m;
}

GEOMMatrix4 geom_matrix4_translation(double x, double y, double z) {
    GEOMMatrix4 m = geom_matrix4_identity();
    m.d = x;
    m.h = y;
    m.l = z;
    return m;
}

GEOMMatrix4 geom_matrix4_rotation_x(double angle) {
    GEOMMatrix4 m = geom_matrix4_identity();
    double s = sin(angle);
    double c = cos(angle);
    m.f = m.k = c;
    m.g = -s;
    m.j = s;
    return m;
}

GEOMMatrix4 geom_matrix4_rotation_y(double angle) {
    GEOMMatrix4 m = geom_matrix4_identity();
    double s = sin(angle);
    double c = cos(angle);
    m.a = m.k = c;
    m.c = s;
    m.i = -s;
    return m;   
}

GEOMMatrix4 geom_matrix4_rotation_z(double angle) {
    GEOMMatrix4 m = geom_matrix4_identity();
    double s = sin(angle);
    double c = cos(angle);
    m.a = m.f = c;
    m.b = -s;
    m.e = s;
    return m;
}

GEOMMatrix4 geom_matrix4_scale(double x, double y, double z) {
    GEOMMatrix4 m = geom_matrix4_identity();
    m.a = x;
    m.f = y;
    m.k = z;
    return m;
}

GEOMMatrix4 geom_matrix4_perspective(double fov_y, double aspect, double near, double far) {
    double f = 1.0 / tan(fov_y / 2.0);
    GEOMMatrix4 m = geom_matrix4_identity();
    assert(near != 0.0 && near != far);
    m.a = f / aspect;
    m.f = f;
    m.k = (far + near) / (near - far);
    m.l = 2.0 * far * near / (near - far);
    m.o = -1.0;
    m.p = 0.0;
    return m;
}

GEOMMatrix4 geom_matrix4_multiply(GEOMMatrix4 a, GEOMMatrix4 b) {
    GEOMMatrix4 m;
    m.a = a.a * b.a + a.b * b.e + a.c * b.i + a.d * b.m;
    m.b = a.a * b.b + a.b * b.f + a.c * b.j + a.d * b.n;
    m.c = a.a * b.c + a.b * b.g + a.c * b.k + a.d * b.o;
    m.d = a.a * b.d + a.b * b.h + a.c * b.l + a.d * b.p;
    m.e = a.e * b.a + a.f * b.e + a.g * b.i + a.h * b.m;
    m.f = a.e * b.b + a.f * b.f + a.g * b.j + a.h * b.n;
    m.g = a.e * b.c + a.f * b.g + a.g * b.k + a.h * b.o;
    m.h = a.e * b.d + a.f * b.h + a.g * b.l + a.h * b.p;
    m.i = a.i * b.a + a.j * b.e + a.k * b.i + a.l * b.m;
    m.j = a.i * b.b + a.j * b.f + a.k * b.j + a.l * b.n;
    m.k = a.i * b.c + a.j * b.g + a.k * b.k + a.l * b.o;
    m.l = a.i * b.d + a.j * b.h + a.k * b.l + a.l * b.p;
    m.m = a.m * b.a + a.n * b.e + a.o * b.i + a.p * b.m;
    m.n = a.m * b.b + a.n * b.f + a.o * b.j + a.p * b.n;
    m.o = a.m * b.c + a.n * b.g + a.o * b.k + a.p * b.o;
    m.p = a.m * b.d + a.n * b.h + a.o * b.l + a.p * b.p;
    return m;
}

GEOMPoint3  geom_matrix4_transform(GEOMMatrix4 m, GEOMPoint3 p) {
    GEOMPoint3 o;
    o.x = m.a * p.x + m.b * p.y + m.c * p.z + m.d;
    o.y = m.e * p.x + m.f * p.y + m.g * p.z + m.h;
    o.z = m.i * p.x + m.j * p.y + m.k * p.z + m.l;
    double w =   m.m * p.x + m.n * p.y + m.o * p.z + m.p;
    if ( w != 0.0 ) {
        o.x /= w;
        o.y /= w;
        o.z /= w;
    }
    return o;
}
