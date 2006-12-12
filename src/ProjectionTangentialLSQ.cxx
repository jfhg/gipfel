//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ProjectionTangentialLSQ.H"


static double sec(double a) {
	return 1.0 / cos(a);
}

#include "lsq_funcs.c"

static double
comp_tilt(double tan_nick_view, double tan_dir_view, double n_scale,
	double tan_nick_m, double tan_dir_m,
	double x, double y, double pi_d);

int
ProjectionTangentialLSQ::comp_params(const Hill *m1, const Hill *m2, ViewParams *parms) {
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

double 
ProjectionTangentialLSQ::angle_dist(double a1, double a2) {
	double ret;

	a1 = fmod(a1, 2.0 * pi_d); 
	if (a1 < 0.0) {
		a1 = a1 + 2.0 * pi_d;
	}
	a2 = fmod(a2, 2.0 * pi_d); 
	if (a2 < 0.0) {
		a2 = a2 + 2.0 * pi_d;
	}

	ret = fabs(a1 - a2);
	if (ret > pi_d) {
		ret = 2.0 * pi_d - ret;
	} 

	return ret;
}

int
ProjectionTangentialLSQ::optimize(const Hill *m1, const Hill *m2, ViewParams *parms) {
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
#if 0
	for (i=0; i<5; i++) {
		opt_step(&tan_nick_view, &tan_dir_view, &n_scale, 
			tan_dir_m1, tan_nick_m1, tan_dir_m2, tan_nick_m2,
			d_m1_2, d_m2_2, d_m1_m2_2);
	}
#endif

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
	if (angle_dist(parms->a_center, m1->alph) > pi_d/2.0) {
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
ProjectionTangentialLSQ::get_coordinates(double a_view, double a_nick,
	const ViewParams *parms, double *x, double *y) {
	double x_tmp, y_tmp;

	x_tmp = tan(a_view) * parms->scale;
	y_tmp = - (tan(a_nick - parms->a_nick) * parms->scale);

	// rotate by a_tilt;
	*x = x_tmp * cos(parms->a_tilt) - y_tmp * sin(parms->a_tilt);
	*y = x_tmp * sin(parms->a_tilt) + y_tmp * cos(parms->a_tilt);
}

double
ProjectionTangentialLSQ::comp_center_angle(double a1, double a2, double d1, double d2) {
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
ProjectionTangentialLSQ::comp_scale(double a1, double a2, double d1, double d2) {
	double sign1 = 1.0;
	double sc, tan_a1, tan_a2;

	tan_a1 = tan(a1);
	tan_a2 = tan(a2);

	sc = ((((1.0 + (tan_a1 * tan_a2)) * (d1 - d2)) - (sign1 * pow((((1.0 + pow((tan_a1 * tan_a2), 2.0)) * ((d1 * d1) + (d2 * d2))) + (2.0 * ((tan_a1 * tan_a2 * pow((d1 + d2), 2.0)) - (d1 * d2 * (((tan_a1 * tan_a1) * (2.0 + (tan_a2 * tan_a2))) + 1.0 + (2.0 * (tan_a2 * tan_a2))))))), (1.0 / 2.0)))) / (2.0 * (tan_a1 - tan_a2)));

	return sc;
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
