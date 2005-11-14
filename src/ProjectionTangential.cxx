// 
// ProjectionTangential routines.
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
extern "C" {
#include <ccmath.h>
}

#include "ProjectionTangential.H"

static int opt_step(double *tan_nick_view, 
		    double *tan_dir_view,
		    double *n_scale,
		    double tan_dir_m1,
		    double tan_nick_m1,
		    double tan_dir_m2,
		    double tan_nick_m2,
		    double d_m1_2, double d_m2_2, double d_m1_m2_2);

static double
comp_tilt(double tan_nick_view, double tan_dir_view, double n_scale,
	  double tan_nick_m, double tan_dir_m,
	  double x, double y, double pi_d);

int
ProjectionTangential::comp_params(const Hill *m1, const Hill *m2, ViewParams *parms) {
  const Hill *tmp;
  double a_center_tmp, scale_tmp, a_nick_tmp;

  scale_tmp = comp_scale(m1->alph, m2->alph, m1->x, m2->x);
  if (isnan(scale_tmp) || scale_tmp < 100.0) {
    // try again with mountains swapped
    tmp = m1;
    m1 = m2;
    m2 = tmp;
    scale_tmp = comp_scale(m1->alph, m2->alph, m1->x, m2->x);
  }

  a_center_tmp = comp_center_angle(m1->alph, m2->alph, m1->x, m2->x);
  a_nick_tmp   = atan ((m1->y + tan(m1->a_nick) * parms->scale) / 
		       (parms->scale - m1->y * tan(m1->a_nick)));

  if (isnan(a_center_tmp) || isnan(scale_tmp) ||
      scale_tmp < 100.0 || isnan(a_nick_tmp)) {
    return 1;
  } else {
    
    parms->a_center = a_center_tmp;
    parms->scale    = scale_tmp;
    parms->a_nick   = a_nick_tmp;

    optimize(m1, m2, parms);

    return 0;
  }
}

int
ProjectionTangential::optimize(const Hill *m1, const Hill *m2, ViewParams *parms) {
  int i;
  double tan_nick_view, tan_dir_view, n_scale;
  double tan_nick_m1, tan_dir_m1;
  double tan_nick_m2, tan_dir_m2;
  double d_m1_2, d_m2_2, d_m1_m2_2;

  d_m1_2 = pow(m1->x, 2.0) + pow(m1->y, 2.0);
  d_m2_2 = pow(m2->x, 2.0) + pow(m2->y, 2.0);
  d_m1_m2_2 = pow(m1->x - m2->x, 2.0) + pow(m1->y - m2->y, 2.0);

  tan_nick_view = tan(parms->a_nick);
  tan_dir_view = tan(parms->a_center);
  n_scale = parms->scale;
  tan_dir_m1 = tan(m1->alph);
  tan_nick_m1 = tan(m1->a_nick);
  tan_dir_m2 = tan(m2->alph);
  tan_nick_m2 = tan(m2->a_nick);

  for (i=0; i<5; i++) {
    opt_step(&tan_nick_view, &tan_dir_view, &n_scale, 
             tan_dir_m1, tan_nick_m1, tan_dir_m2, tan_nick_m2,
	     d_m1_2, d_m2_2, d_m1_m2_2);
  }

  if (isnan(tan_dir_view) || isnan(tan_nick_view) || isnan(n_scale)) {
    fprintf(stderr, "No solution found.\n");
    return 1;
  }

  parms->a_center = atan(tan_dir_view);
  parms->a_nick = atan(tan_nick_view);

  if (parms->a_center > 2.0 * pi_d) {
    parms->a_center = parms->a_center - 2.0 * pi_d;
  } else if (parms->a_center < 0.0) {
    parms->a_center = parms->a_center + 2.0 * pi_d;
  }

  // atan(tan_dir_view) is not the only possible solution.
  // Choose the one which is close to m1->alph.
  if (fabs(parms->a_center - m1->alph) > pi_d/2.0) {
   parms->a_center = parms->a_center + pi_d;
  }
  
  parms->scale = n_scale;

  // use the point with greater distance from center for tilt computation 
  if (d_m1_2 > d_m2_2) {
    parms->a_tilt = comp_tilt(tan_nick_view, tan_dir_view, n_scale, 
                       tan_nick_m1, tan_dir_m1,
		       (double) m1->x, (double) m1->y, pi_d);
  } else {
    parms->a_tilt = comp_tilt(tan_nick_view, tan_dir_view, n_scale, 
	 tan_nick_m2, tan_dir_m2,
	 (double) m2->x, (double) m2->y, pi_d);
  }

  return 0;
}

void 
ProjectionTangential::set_coordinates(Hill *m, const ViewParams *parms) {
  double x_tmp, y_tmp;

  x_tmp = tan(m->a_view) * parms->scale;
  y_tmp = - (tan(m->a_nick - parms->a_nick) * parms->scale);

  // rotate by a_tilt;
  m->x = (int) rint(x_tmp * cos(parms->a_tilt) - y_tmp * sin(parms->a_tilt));
  m->y = (int) rint(x_tmp * sin(parms->a_tilt) + y_tmp * cos(parms->a_tilt));
}

double
ProjectionTangential::comp_center_angle(double a1, double a2, double d1, double d2) {
  double sign1 = 1.0;
  double tan_acenter, tan_a1, tan_a2, a_center;

  tan_a1 = tan(a1);
  tan_a2 = tan(a2);

  tan_acenter = (((pow(((pow((1.0 + (tan_a1 * tan_a2)), 2.0) * ((d1 * d1) + (d2 * d2))) + (2.0 * d1 * d2 * ((2.0 * ((tan_a2 * tan_a1) - (tan_a2 * tan_a2))) - ((tan_a1 * tan_a1) * (2.0 + (tan_a2 * tan_a2))) - 1.0))), (1.0 / 2.0)) * sign1) + ((1.0 - (tan_a1 * tan_a2)) * (d1 - d2))) / (2.0 * ((d2 * tan_a2) - (d1 * tan_a1))));

  a_center = atan(tan_acenter);

  if (a_center > 2.0 * pi_d) {
     a_center = a_center - 2.0 * pi_d;
  } else if (a_center < 0.0) {
     a_center = a_center + 2.0 * pi_d;
  }

  // atan(tan_dir_view) is not the only possible solution.
  // Choose the one which is close to m1->alph.
  if (fabs(a_center - a1) > pi_d/2.0) {
    a_center = a_center + pi_d;
  }

  return a_center; 
}

double
ProjectionTangential::comp_scale(double a1, double a2, double d1, double d2) {
  double sign1 = 1.0;
  double sc, tan_a1, tan_a2;

  tan_a1 = tan(a1);
  tan_a2 = tan(a2);
  
  sc = ((((1.0 + (tan_a1 * tan_a2)) * (d1 - d2)) - (sign1 * pow((((1.0 + pow((tan_a1 * tan_a2), 2.0)) * ((d1 * d1) + (d2 * d2))) + (2.0 * ((tan_a1 * tan_a2 * pow((d1 + d2), 2.0)) - (d1 * d2 * (((tan_a1 * tan_a1) * (2.0 + (tan_a2 * tan_a2))) + 1.0 + (2.0 * (tan_a2 * tan_a2))))))), (1.0 / 2.0)))) / (2.0 * (tan_a1 - tan_a2)));

  return sc;
}

static int
get_matrix(double m[], 
	   double tan_nick_view, double tan_dir_view, double n_scale,
	   double tan_dir_m1, double tan_nick_m1,
	   double tan_dir_m2, double tan_nick_m2) {
  
  m[0] = pow(n_scale,2.0)*(1.0/pow((tan_nick_m1*tan_nick_view + 1.0),2.0)*(2.0*tan_nick_m1 - 2.0 * tan_nick_view) + 2.0*tan_nick_m1*pow((tan_nick_m1 - tan_nick_view), 2.0)/pow((tan_nick_m1*tan_nick_view + 1.0), 3.0));

  m[1] = pow(n_scale, 2.0) *(1.0/pow((tan_dir_m1*tan_dir_view + 1.0), 2.0) * (2.0*tan_dir_m1 - 2.0*tan_dir_view) + 2.0*tan_dir_m1*pow((tan_dir_m1 - tan_dir_view),2.0) / pow((tan_dir_m1*tan_dir_view + 1.0), 3.0));

  m[2] = -2.0*n_scale*(pow((tan_dir_m1 - tan_dir_view), 2.0)/pow((tan_dir_m1*tan_dir_view + 1.0), 2.0) + pow((tan_nick_m1 - tan_nick_view), 2.0)/pow((tan_nick_m1*tan_nick_view + 1.0), 2.0));

  m[3] = pow(n_scale, 2.0)*(1.0/pow((tan_nick_m2*tan_nick_view + 1.0), 2.0)*(2.0*tan_nick_m2 - 2.0*tan_nick_view) + 2.0*tan_nick_m2*pow((tan_nick_m2 - tan_nick_view), 2.0)/pow((tan_nick_m2*tan_nick_view + 1.0), 3.0));

  m[4] = pow(n_scale, 2.0)*(1.0/pow((tan_dir_m2*tan_dir_view + 1.0), 2.0)*(2.0*tan_dir_m2 - 2.0*tan_dir_view) + 2.0*tan_dir_m2*pow((tan_dir_m2 - tan_dir_view), 2.0)/pow((tan_dir_m2*tan_dir_view + 1.0), 3.0));

  m[5] = -2.0*n_scale*(pow((tan_dir_m2 - tan_dir_view), 2.0)/pow((tan_dir_m2*tan_dir_view + 1.0), 2.0) + pow((tan_nick_m2 - tan_nick_view), 2.0)/pow((tan_nick_m2*tan_nick_view + 1.0), 2.0));

  m[6] = 2.0*(n_scale*(tan_nick_m1 - tan_nick_view)/(tan_nick_m1*tan_nick_view + 1.0) - n_scale*(tan_nick_m2 - tan_nick_view)/(tan_nick_m2*tan_nick_view + 1.0))*(n_scale/(tan_nick_m1*tan_nick_view + 1.0) - n_scale/(tan_nick_m2*tan_nick_view + 1.0) + tan_nick_m1*n_scale*(tan_nick_m1 - tan_nick_view)/pow((tan_nick_m1*tan_nick_view + 1.0), 2.0) - tan_nick_m2*n_scale*(tan_nick_m2 - tan_nick_view)/pow((tan_nick_m2*tan_nick_view + 1.0),2.0));

  m[7] = 2.0*(n_scale*(tan_dir_m1 - tan_dir_view)/(tan_dir_m1*tan_dir_view + 1.0) - n_scale*(tan_dir_m2 - tan_dir_view)/(tan_dir_m2*tan_dir_view + 1.0))*(n_scale/(tan_dir_m1*tan_dir_view + 1.0) - n_scale/(tan_dir_m2*tan_dir_view + 1.0) + tan_dir_m1*n_scale*(tan_dir_m1 - tan_dir_view)/pow((tan_dir_m1*tan_dir_view + 1.0), 2.0) - tan_dir_m2*n_scale*(tan_dir_m2 - tan_dir_view)/pow((tan_dir_m2*tan_dir_view + 1.0), 2.0));

  m[8] = - 2.0*(n_scale*(tan_dir_m1 - tan_dir_view)/(tan_dir_m1*tan_dir_view + 1.0) - n_scale*(tan_dir_m2 - tan_dir_view)/(tan_dir_m2*tan_dir_view + 1.0))*((tan_dir_m1 - tan_dir_view)/(tan_dir_m1*tan_dir_view + 1.0) - (tan_dir_m2 - tan_dir_view)/(tan_dir_m2*tan_dir_view + 1.0)) - 2.0*(n_scale*(tan_nick_m1 - tan_nick_view)/(tan_nick_m1*tan_nick_view + 1.0) - n_scale*(tan_nick_m2 - tan_nick_view)/(tan_nick_m2*tan_nick_view + 1.0))*((tan_nick_m1 - tan_nick_view)/(tan_nick_m1*tan_nick_view + 1.0) - (tan_nick_m2 - tan_nick_view)/(tan_nick_m2*tan_nick_view + 1.0));

  return 0;
}

static int opt_step(double *tan_nick_view, 
		    double *tan_dir_view,
		    double *n_scale,
		    double tan_dir_m1,
		    double tan_nick_m1,
		    double tan_dir_m2,
		    double tan_nick_m2,
		    double d_m1_2, double d_m2_2, double d_m1_m2_2) {
  double a[9];
  double b[3];
  double a_x0[3], f_x0 [3], x0[3];
  int ret;

  get_matrix(a, *tan_nick_view, *tan_dir_view, *n_scale, 
	     tan_dir_m1, tan_nick_m1, tan_dir_m2, tan_nick_m2);

  f_x0[0] = d_m1_2 - (pow((*tan_nick_view-tan_nick_m1),2.0)/pow((tan_nick_m1**tan_nick_view+1), 2.0)+pow((*tan_dir_view-tan_dir_m1),2.0)/pow((tan_dir_m1**tan_dir_view+1),2.0))*pow(*n_scale, 2.0);

  f_x0[1] = d_m2_2 - (pow((*tan_nick_view-tan_nick_m2),2.0)/pow((tan_nick_m2**tan_nick_view+1),2.0)+pow((*tan_dir_view-tan_dir_m2),2.0)/pow((tan_dir_m2**tan_dir_view+1),2.0))*pow(*n_scale, 2.0);

  f_x0[2] = d_m1_m2_2 - (pow((- (((*tan_dir_view - tan_dir_m1) * *n_scale) / (tan_dir_m1 * *tan_dir_view + 1.0)) + (((*tan_dir_view - tan_dir_m2) * *n_scale) / (tan_dir_m2 * *tan_dir_view + 1))), 2.0) + pow((- (((*tan_nick_view - tan_nick_m1) * *n_scale) / (tan_nick_m1 * *tan_nick_view + 1)) + ((*tan_nick_view - tan_nick_m2) * *n_scale) / (tan_nick_m2 * *tan_nick_view + 1)), 2.0));

  x0[0] = *tan_nick_view;
  x0[1] = *tan_dir_view;
  x0[2] = *n_scale;

  rmmult(a_x0, a, x0, 3, 3, 1);
    
  b[0] = a_x0[0] - f_x0[0];
  b[1] = a_x0[1] - f_x0[1];
  b[2] = a_x0[2] - f_x0[2];

  ret = solv(a, b, 3);

  *tan_nick_view = b[0];
  *tan_dir_view  = b[1];
  *n_scale       = b[2];

  return 0;
}


static double
comp_tilt(double tan_nick_view, double tan_dir_view, double n_scale,
	  double tan_nick_m, double tan_dir_m,
	  double x, double y, double pi_d) {
  double y_tmp, x_tmp, sin_a_tilt1, sin_a_tilt2, sin_a_tilt, res;

  y_tmp = - (((tan_nick_view - tan_nick_m) * n_scale) / 
	     (tan_nick_m * tan_nick_view + 1));
  x_tmp = - (((tan_dir_view - tan_dir_m) * n_scale) / 
	     (tan_dir_m * tan_dir_view + 1));


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
