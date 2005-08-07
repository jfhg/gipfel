// 
// Copyright 2005 by Johannes Hofmann
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ProjectionSphaeric.H"

int
ProjectionSphaeric::comp_params(Hill *m1, Hill *m2, ViewParams *parms) {
  Hill *tmp;
  double a_center_tmp, scale_tmp, a_nick_tmp;
  double d_m1_2, d_m2_2, d_m1_m2_2;

  if (m1->x > m2->x) {
    tmp = m1;
    m1 = m2;
    m2 = tmp;
  }

  d_m1_2 = pow(m1->x, 2.0) + pow(m1->y, 2.0);
  d_m2_2 = pow(m2->x, 2.0) + pow(m2->y, 2.0);
  d_m1_m2_2 = pow(m1->x - m2->x, 2.0) + pow(m1->y - m2->y, 2.0);

  scale_tmp    = comp_scale(m1, m2, d_m1_m2_2);
  a_center_tmp = comp_dir_view(m1, m2, d_m1_2, d_m2_2, scale_tmp);
  a_nick_tmp   = comp_nick_view(m1, m2, d_m1_2, scale_tmp, a_center_tmp);

  if (isnan(a_center_tmp) || isnan(scale_tmp) || isnan(a_nick_tmp)) {
    return 1;
  } else {
    
    parms->a_center = a_center_tmp;
    parms->scale    = scale_tmp;
    parms->a_nick   = a_nick_tmp;

    // use the point with greater distance from center for tilt computation 
    if (d_m1_2 > d_m2_2) {
      parms->a_tilt = comp_tilt_view(m1, scale_tmp, a_center_tmp, a_nick_tmp);
    } else {
      parms->a_tilt = comp_tilt_view(m2, scale_tmp, a_center_tmp, a_nick_tmp);
    }

    return 0;
  }
}

void 
ProjectionSphaeric::set_coordinates(Hill *m, const ViewParams *parms) {
  double x_tmp, y_tmp;

  x_tmp = m->a_view * parms->scale;
  y_tmp = - (m->a_nick - parms->a_nick) * parms->scale;

  // rotate by a_tilt;
  m->x = (int) rint(x_tmp * cos(parms->a_tilt) - y_tmp * sin(parms->a_tilt));
  m->y = (int) rint(x_tmp * sin(parms->a_tilt) + y_tmp * cos(parms->a_tilt));
}

double
ProjectionSphaeric::comp_scale(Hill *m1, Hill *m2, double d_m1_m2_2) {
  double sign1 = 1.0;
  double nick_m1 = m1->a_nick;
  double nick_m2 = m2->a_nick;
  double dir_m1 = m1->alph;
  double dir_m2 = m2->alph;

  return  (pow((d_m1_m2_2 / ((dir_m2 * dir_m2) - (2.0 * ((dir_m2 * dir_m1) + (nick_m2 * nick_m1))) + (nick_m2 * nick_m2) + (dir_m1 * dir_m1) + (nick_m1 * nick_m1))), (1.0 / 2.0)) * sign1); 
}

double
ProjectionSphaeric::comp_dir_view(Hill *m1, Hill *m2, double d_m1_2, double d_m2_2, double scale) {
  double sign3 = 1.0;
  double dir_view;
  double nick_m1 = m1->a_nick;
  double nick_m2 = m2->a_nick;
  double dir_m1 = m1->alph;
  double dir_m2 = m2->alph;

  dir_view = (((pow((16.0 * pow(scale, 4.0) * (((scale * scale) * ((2.0 * (((scale * scale) * ((dir_m1 * ((dir_m1 * ((nick_m1 * ((nick_m1 * ((nick_m2 * ((4.0 * nick_m1) - (6.0 * nick_m2))) - (nick_m1 * nick_m1))) + (nick_m2 * ((4.0 * (nick_m2 * nick_m2)) + (6.0 * (dir_m2 * dir_m2)) + (dir_m1 * dir_m1))))) - pow(nick_m2, 4.0))) + (nick_m1 * nick_m2 * dir_m2 * ((nick_m1 * ((12.0 * nick_m2) - (8.0 * nick_m1))) - (8.0 * (nick_m2 * nick_m2)))))) + ((dir_m2 * dir_m2) * ((nick_m1 * ((nick_m2 * (dir_m2 * dir_m2)) - pow(nick_m1, 3.0))) - pow(nick_m2, 4.0))))) + ((d_m1_2 + d_m2_2) * ((((nick_m1 * nick_m1) + (nick_m2 * nick_m2)) * ((dir_m1 * dir_m1) + (dir_m2 * dir_m2))) + pow(nick_m1, 4.0) + pow(nick_m2, 4.0))))) + (4.0 * ((dir_m1 * ((nick_m1 * ((nick_m1 * dir_m2 * (((scale * scale) * ((nick_m1 * nick_m1) + (dir_m1 * dir_m1) + (dir_m2 * dir_m2))) - d_m1_2 - d_m2_2)) - (dir_m1 * nick_m2 * (d_m2_2 + d_m1_2)))) + ((nick_m2 * nick_m2) * dir_m2 * (((scale * scale) * ((nick_m2 * nick_m2) + (dir_m1 * dir_m1) + (dir_m2 * dir_m2))) - d_m1_2 - d_m2_2)))) - (nick_m1 * nick_m2 * (dir_m2 * dir_m2) * (d_m1_2 + d_m2_2)))) + ((scale * scale) * ((nick_m1 * ((nick_m1 * ((nick_m2 * (((dir_m2 * dir_m2) * ((8.0 * nick_m1) - (12.0 * nick_m2))) + (nick_m1 * ((nick_m1 * ((6.0 * nick_m1) - (15.0 * nick_m2))) + (20.0 * (nick_m2 * nick_m2)))) - (15.0 * pow(nick_m2, 3.0)))) - ((dir_m1 * dir_m1) * ((dir_m1 * dir_m1) + (6.0 * (dir_m2 * dir_m2)))) - pow(dir_m2, 4.0) - pow(nick_m1, 4.0))) + (nick_m2 * ((8.0 * dir_m2 * ((dir_m2 * ((nick_m2 * nick_m2) - (dir_m1 * dir_m2))) - pow(dir_m1, 3.0))) + (6.0 * pow(nick_m2, 4.0)))))) - ((nick_m2 * nick_m2) * (((dir_m1 * dir_m1) * ((6.0 * (dir_m2 * dir_m2)) + (dir_m1 * dir_m1))) + pow(dir_m2, 4.0) + pow(nick_m2, 4.0))))) + (nick_m1 * nick_m2 * (d_m1_2 + d_m2_2) * ((8.0 * ((dir_m1 * dir_m2) - (nick_m1 * nick_m1) - (nick_m2 * nick_m2))) + (12.0 * nick_m1 * nick_m2))))) + (((d_m1_2 * ((2.0 * d_m2_2) - d_m1_2)) - (d_m2_2 * d_m2_2)) * ((nick_m1 * (nick_m1 - (2.0 * nick_m2))) + (nick_m2 * nick_m2))))), (1.0 / 2.0)) * sign3 / 2.0) + (2.0 * (scale * scale) * (((scale * scale) * ((dir_m1 * ((nick_m1 * (nick_m1 - (2.0 * nick_m2))) + (nick_m2 * nick_m2) - (dir_m2 * (dir_m2 + dir_m1)) + (dir_m1 * dir_m1))) + (dir_m2 * ((dir_m2 * dir_m2) + (nick_m1 * (nick_m1 - (2.0 * nick_m2))) + (nick_m2 * nick_m2))))) + ((dir_m2 - dir_m1) * (d_m1_2 - d_m2_2))))) / (4.0 * pow(scale, 4.0) * ((dir_m1 * (dir_m1 - (2.0 * dir_m2))) + (dir_m2 * dir_m2) + (nick_m1 * (nick_m1 - (2.0 * nick_m2))) + (nick_m2 * nick_m2))));

  return dir_view;
}


double
ProjectionSphaeric::comp_nick_view(Hill *m1, Hill *m2, double d_m1_2, double scale, double dir_view) {
  double nick_view;
  double sign1 = -1.0;
  double nick_m1 = m1->a_nick;
  double dir_m1 = m1->alph;

  nick_view = ((pow(((d_m1_2 / (scale * scale)) - pow((dir_view - dir_m1), 2.0)), (1.0 / 2.0)) * sign1) + nick_m1);

  return nick_view;
}

double
ProjectionSphaeric::comp_tilt_view(Hill *m, double scale, double dir_view, double nick_view) {
  double sin_a_tilt1, sin_a_tilt2, sin_a_tilt, res;
  double x_tmp = (m->alph - dir_view) * scale;
  double y_tmp = (m->a_nick - nick_view) * scale;
  double x = m->x;
  double y = m->y;


  sin_a_tilt1 = - (y * - pow(x*x + y*y - y_tmp*y_tmp, 0.5) - x * y_tmp) /
    (x*x + y*y);

  sin_a_tilt2 = - (y * pow(x*x + y*y - y_tmp*y_tmp, 0.5) - x * y_tmp) / 
    (x*x + y*y);

  sin_a_tilt = fabs(sin_a_tilt1) < fabs(sin_a_tilt2)?sin_a_tilt1:sin_a_tilt2;

  res = asin(sin_a_tilt);

  if (res > pi_d / 4.0) {
    res = res - pi_d / 2.0;
  } else if (res < -pi_d / 4.0) {
    res = res + pi_d / 2.0;
  }

  return res;
  
}
