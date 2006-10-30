//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ProjectionSphaeric.H"

#define BEST_UNDEF 10000000.0

int
ProjectionSphaeric::comp_params(const Hill *m1, const Hill *m2, ViewParams *parms) {
  const Hill *m_tmp;
  double tmp_x, tmp_y;
  double val;
  ViewParams best, tmp;
  double best_val = BEST_UNDEF;
  double d_m1_2, d_m2_2, d_m1_m2_2;
  int i, j;

  if (m1->x < m2->x) {
    m_tmp = m1;
    m1 = m2;
    m2 = m_tmp;
  }

  d_m1_2 = pow(m1->x, 2.0) + pow(m1->y, 2.0);
  d_m2_2 = pow(m2->x, 2.0) + pow(m2->y, 2.0);
  d_m1_m2_2 = pow(m1->x - m2->x, 2.0) + pow(m1->y - m2->y, 2.0);

  tmp.scale = comp_scale(m1, m2, d_m1_m2_2);

  for(i=0; i<2; i++) { // we need to try four possible solutions ...
    for(j=0; j<2; j++) { 
      tmp.a_center = comp_dir_view(m1, m2, d_m1_2, d_m2_2, 
                                   tmp.scale, i==0?1.0:-1.0);
      tmp.a_nick = comp_nick_view(m1, m2, d_m1_2, tmp.scale, tmp.a_center,
                                  j==0?1.0:-1.0);

      // use the point with greater distance from center for tilt computation 
      if (d_m1_2 > d_m2_2) {
        tmp.a_tilt = comp_tilt_view(m1, tmp.scale, tmp.a_center, tmp.a_nick);
      } else {
        tmp.a_tilt = comp_tilt_view(m2, tmp.scale, tmp.a_center, tmp.a_nick);
      }

      if (isnan(tmp.a_center) || isnan(tmp.scale) || 
          isnan(tmp.a_nick) || isnan(tmp.a_tilt)) {
        ;
      } else {
        get_coordinates(m1->a_view, m1->a_nick, &tmp, &tmp_x, &tmp_y);
        val = sqrt(pow(tmp_x - m1->x, 2.0) + pow(tmp_y - m1->y, 2.0)); 
        get_coordinates(m2->a_view, m2->a_nick, &tmp, &tmp_x, &tmp_y);
        val += sqrt(pow(tmp_x - m2->x, 2.0) + pow(tmp_y - m2->y, 2.0)); 

        if (val < best_val) {	
          best_val = val;
          best = tmp;
        }
      }
    }
  }

  if (best_val < BEST_UNDEF) {
    *parms = best;
    return 0;
  } else {
    return 1;
  }
}

void 
ProjectionSphaeric::get_coordinates(double a_view, double a_nick,
	const ViewParams *parms, double *x, double *y) {
  double x_tmp, y_tmp;

  x_tmp = a_view * parms->scale;
  y_tmp = - (a_nick - parms->a_nick) * parms->scale;

  // rotate by a_tilt;
  *x = x_tmp * cos(parms->a_tilt) - y_tmp * sin(parms->a_tilt);
  *y = x_tmp * sin(parms->a_tilt) + y_tmp * cos(parms->a_tilt);
}


double
ProjectionSphaeric::comp_scale(const Hill *m1, const Hill *m2, double d_m1_m2_2) {
  double sign1 = 1.0;
  double nick_m1 = m1->a_nick;
  double nick_m2 = m2->a_nick;
  double dir_m1 = m1->alph;
  double dir_m2 = m2->alph;

  return  (pow((d_m1_m2_2 / ((dir_m2 * dir_m2) - (2.0 * ((dir_m2 * dir_m1) + (nick_m2 * nick_m1))) + (nick_m2 * nick_m2) + (dir_m1 * dir_m1) + (nick_m1 * nick_m1))), (1.0 / 2.0)) * sign1); 
}

// using the sign3 parameter one can choose between the two possible solutions
// sign3 must be 1.0 or -1.0
double
ProjectionSphaeric::comp_dir_view(const Hill *m1, const Hill *m2, double d_m1_2, double d_m2_2, double scale, double sign3) {
  double dir_view;
  double nick_m1 = m1->a_nick;
  double nick_m2 = m2->a_nick;
  double dir_m1 = m1->alph;
  double dir_m2 = m2->alph;

  dir_view = (((pow((16.0 * pow(scale, 4.0) * (((scale * scale) * ((2.0 * (((scale * scale) * ((dir_m1 * ((dir_m1 * ((nick_m1 * ((nick_m1 * ((nick_m2 * ((4.0 * nick_m1) - (6.0 * nick_m2))) - (nick_m1 * nick_m1))) + (nick_m2 * ((4.0 * (nick_m2 * nick_m2)) + (6.0 * (dir_m2 * dir_m2)) + (dir_m1 * dir_m1))))) - pow(nick_m2, 4.0))) + (nick_m1 * nick_m2 * dir_m2 * ((nick_m1 * ((12.0 * nick_m2) - (8.0 * nick_m1))) - (8.0 * (nick_m2 * nick_m2)))))) + ((dir_m2 * dir_m2) * ((nick_m1 * ((nick_m2 * (dir_m2 * dir_m2)) - pow(nick_m1, 3.0))) - pow(nick_m2, 4.0))))) + ((d_m1_2 + d_m2_2) * ((((nick_m1 * nick_m1) + (nick_m2 * nick_m2)) * ((dir_m1 * dir_m1) + (dir_m2 * dir_m2))) + pow(nick_m1, 4.0) + pow(nick_m2, 4.0))))) + (4.0 * ((dir_m1 * ((nick_m1 * ((nick_m1 * dir_m2 * (((scale * scale) * ((nick_m1 * nick_m1) + (dir_m1 * dir_m1) + (dir_m2 * dir_m2))) - d_m1_2 - d_m2_2)) - (dir_m1 * nick_m2 * (d_m2_2 + d_m1_2)))) + ((nick_m2 * nick_m2) * dir_m2 * (((scale * scale) * ((nick_m2 * nick_m2) + (dir_m1 * dir_m1) + (dir_m2 * dir_m2))) - d_m1_2 - d_m2_2)))) - (nick_m1 * nick_m2 * (dir_m2 * dir_m2) * (d_m1_2 + d_m2_2)))) + ((scale * scale) * ((nick_m1 * ((nick_m1 * ((nick_m2 * (((dir_m2 * dir_m2) * ((8.0 * nick_m1) - (12.0 * nick_m2))) + (nick_m1 * ((nick_m1 * ((6.0 * nick_m1) - (15.0 * nick_m2))) + (20.0 * (nick_m2 * nick_m2)))) - (15.0 * pow(nick_m2, 3.0)))) - ((dir_m1 * dir_m1) * ((dir_m1 * dir_m1) + (6.0 * (dir_m2 * dir_m2)))) - pow(dir_m2, 4.0) - pow(nick_m1, 4.0))) + (nick_m2 * ((8.0 * dir_m2 * ((dir_m2 * ((nick_m2 * nick_m2) - (dir_m1 * dir_m2))) - pow(dir_m1, 3.0))) + (6.0 * pow(nick_m2, 4.0)))))) - ((nick_m2 * nick_m2) * (((dir_m1 * dir_m1) * ((6.0 * (dir_m2 * dir_m2)) + (dir_m1 * dir_m1))) + pow(dir_m2, 4.0) + pow(nick_m2, 4.0))))) + (nick_m1 * nick_m2 * (d_m1_2 + d_m2_2) * ((8.0 * ((dir_m1 * dir_m2) - (nick_m1 * nick_m1) - (nick_m2 * nick_m2))) + (12.0 * nick_m1 * nick_m2))))) + (((d_m1_2 * ((2.0 * d_m2_2) - d_m1_2)) - (d_m2_2 * d_m2_2)) * ((nick_m1 * (nick_m1 - (2.0 * nick_m2))) + (nick_m2 * nick_m2))))), (1.0 / 2.0)) * sign3 / 2.0) + (2.0 * (scale * scale) * (((scale * scale) * ((dir_m1 * ((nick_m1 * (nick_m1 - (2.0 * nick_m2))) + (nick_m2 * nick_m2) - (dir_m2 * (dir_m2 + dir_m1)) + (dir_m1 * dir_m1))) + (dir_m2 * ((dir_m2 * dir_m2) + (nick_m1 * (nick_m1 - (2.0 * nick_m2))) + (nick_m2 * nick_m2))))) + ((dir_m2 - dir_m1) * (d_m1_2 - d_m2_2))))) / (4.0 * pow(scale, 4.0) * ((dir_m1 * (dir_m1 - (2.0 * dir_m2))) + (dir_m2 * dir_m2) + (nick_m1 * (nick_m1 - (2.0 * nick_m2))) + (nick_m2 * nick_m2))));

  return dir_view;
}


double
ProjectionSphaeric::comp_nick_view(const Hill *m1, const Hill *m2, double d_m1_2, double scale, double dir_view, double sign1) {
  double nick_view;
  double nick_m1 = m1->a_nick;
  double dir_m1 = m1->alph;

  nick_view = ((pow(((d_m1_2 / (scale * scale)) - pow((dir_view - dir_m1), 2.0)), (1.0 / 2.0)) * sign1) + nick_m1);

  return nick_view;
}

double
ProjectionSphaeric::comp_tilt_view(const Hill *m, double scale, double dir_view, double nick_view) {
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
