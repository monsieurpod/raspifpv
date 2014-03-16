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

#ifndef __GEOMETRY_H
#define __GEOMETRY_H

double geom_distance_between_coordinates(double lat1, double lon1, double lat2, double lon2);
double geom_bearing_between_coordinates(double lat1, double lon1, double lat2, double lon2);

typedef struct {
    double a;    double b;    double c;    double d;
    double e;    double f;    double g;    double h;
    double i;    double j;    double k;    double l;
    double m;    double n;    double o;    double p;
} GEOMMatrix4;

typedef struct {
    double x;
    double y;
    double z;
} GEOMPoint3;

GEOMMatrix4 geom_matrix4_identity();
GEOMMatrix4 geom_matrix4_translation(double x, double y, double z);
GEOMMatrix4 geom_matrix4_rotation_x(double angle);
GEOMMatrix4 geom_matrix4_rotation_y(double angle);
GEOMMatrix4 geom_matrix4_rotation_z(double angle);
GEOMMatrix4 geom_matrix4_scale(double x, double y, double z);
GEOMMatrix4 geom_matrix4_perspective(double fov_y, double aspect, double near, double far);

GEOMMatrix4 geom_matrix4_multiply(GEOMMatrix4 a, GEOMMatrix4 b);
GEOMPoint3  geom_matrix4_transform(GEOMMatrix4 m, GEOMPoint3 p);

#endif