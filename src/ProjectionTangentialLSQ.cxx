//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlin.h>

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
ProjectionTangentialLSQ::comp_params(const Hills *h, ViewParams *parms) {
	const Hill *tmp, *m1, *m2;
	double a_center_tmp, scale_tmp, a_nick_tmp;

	if (h->get_num() < 3) {
		fprintf(stderr, "Please position at least 3 hills\n");
		return 1;
	}

	m1 = h->get(0);
	m2 = h->get(1);

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
		parms->a_tilt   = 0.0;
		parms->k0       = 0.0;
		parms->k1       = 0.0;
		parms->u0       = 0.0;
		parms->v0       = 0.0;
		if (angle_dist(parms->a_center, m1->alph) > pi_d/2.0) {
			parms->a_center = parms->a_center + pi_d;
		}

		lsq(h, parms);

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

struct data {
	const Hills *h;
};

#define CALL(A) A(c_view, c_nick, c_tilt, scale, k0, k1, u0, v0, m->a_view, m->a_nick) 

static int
lsq_f (const gsl_vector * x, void *data, gsl_vector * f) {
	struct data *dat = (struct data *) data;

	double c_view = gsl_vector_get (x, 0);
	double c_nick = gsl_vector_get (x, 1);
	double c_tilt = gsl_vector_get (x, 2);
	double scale = gsl_vector_get (x, 3);
	double k0  = gsl_vector_get (x, 4);
	double k1 = gsl_vector_get (x, 5);
	double u0 = gsl_vector_get (x, 6);
	double v0 = gsl_vector_get (x, 7);

	for (int i=0; i<dat->h->get_num(); i++) {
		Hill *m = dat->h->get(i);

		double x = CALL(mac_x);
		double y = CALL(mac_y);

		gsl_vector_set (f, i*2, x - m->x);
		gsl_vector_set (f, i*2+1, y - m->y);
	}

	return GSL_SUCCESS;
}

 
static int
lsq_df (const gsl_vector * x, void *data, gsl_matrix * J) {
	struct data *dat = (struct data *) data;

	double c_view = gsl_vector_get (x, 0);
	double c_nick = gsl_vector_get (x, 1);
	double c_tilt = gsl_vector_get (x, 2);
	double scale = gsl_vector_get (x, 3);
	double k0  = gsl_vector_get (x, 4);
	double k1 = gsl_vector_get (x, 5);
	double u0 = gsl_vector_get (x, 6);
	double v0 = gsl_vector_get (x, 7);

	for (int i=0; i<dat->h->get_num(); i++) {
		Hill *m = dat->h->get(i);

		gsl_matrix_set (J, 2*i, 0, CALL(mac_x_dc_view));
		gsl_matrix_set (J, 2*i, 1, CALL(mac_x_dc_nick));
		gsl_matrix_set (J, 2*i, 2, CALL(mac_x_dc_tilt));
		gsl_matrix_set (J, 2*i, 3, CALL(mac_x_dscale));
		gsl_matrix_set (J, 2*i, 4, CALL(mac_x_dk0));
		gsl_matrix_set (J, 2*i, 5, CALL(mac_x_dk1));
		gsl_matrix_set (J, 2*i, 6, CALL(mac_x_du0));
		gsl_matrix_set (J, 2*i, 7, CALL(mac_x_dv0));

		gsl_matrix_set (J, 2*i+1, 0, CALL(mac_y_dc_view));
		gsl_matrix_set (J, 2*i+1, 1, CALL(mac_y_dc_nick));
		gsl_matrix_set (J, 2*i+1, 2, CALL(mac_y_dc_tilt));
		gsl_matrix_set (J, 2*i+1, 3, CALL(mac_y_dscale));
		gsl_matrix_set (J, 2*i+1, 4, CALL(mac_y_dk0));
		gsl_matrix_set (J, 2*i+1, 5, CALL(mac_y_dk1));
		gsl_matrix_set (J, 2*i+1, 6, CALL(mac_y_du0));
		gsl_matrix_set (J, 2*i+1, 7, CALL(mac_y_dv0));
	}

	return GSL_SUCCESS;
}

static int
lsq_fdf (const gsl_vector * x, void *data, gsl_vector * f, gsl_matrix * J) {
	lsq_f (x, data, f);
	lsq_df (x, data, J);
     
	return GSL_SUCCESS;
}


int
ProjectionTangentialLSQ::lsq(const Hills *h, ViewParams *parms) {
	const gsl_multifit_fdfsolver_type *T;
	gsl_multifit_fdfsolver *s;
	gsl_multifit_function_fdf f;
	struct data dat;
	double x_init[8];
	gsl_vector_view x;
	int status;

	dat.h = h;

	x_init[0] = parms->a_center;
	x_init[1] = parms->a_nick;
	x_init[2] = parms->a_tilt;
	x_init[3] = parms->scale;
	x_init[4] = parms->k0;
	x_init[5] = parms->k1;
	x_init[6] = parms->u0;
	x_init[7] = parms->v0;

	x = gsl_vector_view_array (x_init, 8);

   f.f = &lsq_f;
   f.df = &lsq_df;
   f.fdf = &lsq_fdf;
   f.n = h->get_num() * 2;
   f.p = 6;
   f.params = &dat;

	T = gsl_multifit_fdfsolver_lmsder;
	s = gsl_multifit_fdfsolver_alloc (T, h->get_num() * 2, 8);
	gsl_multifit_fdfsolver_set (s, &f, &x.vector);

    for (int i=0; i<30; i++) {

		status = gsl_multifit_fdfsolver_iterate (s);
		fprintf(stderr, "gsl_multifit_fdfsolver_iterate: status %d\n", status);
		if (status) {
			break;
		}

	} 

	parms->a_center = gsl_vector_get(s->x, 0);
	parms->a_nick = gsl_vector_get(s->x, 1);
	parms->a_tilt = gsl_vector_get(s->x, 2);
	parms->scale = gsl_vector_get(s->x, 3);
	parms->k0 = gsl_vector_get(s->x, 4);
	parms->k1 = gsl_vector_get(s->x, 5);
	parms->u0 = gsl_vector_get(s->x, 6);
	parms->v0 = gsl_vector_get(s->x, 7);

	gsl_multifit_fdfsolver_free (s);

	fprintf(stderr, "k0 %f k1 %f u0 %f v0 %f\n",
		parms->k0, parms->k1, parms->u0, parms->v0);

	return 0;
}

void 
ProjectionTangentialLSQ::get_coordinates(double a_view, double a_nick,
	const ViewParams *parms, double *x, double *y) {

	*x = mac_x(parms->a_center, parms->a_nick, parms->a_tilt, parms->scale,
		parms->k0, parms->k1, parms->u0, parms->v0, a_view, a_nick); 
	*y = mac_y(parms->a_center, parms->a_nick, parms->a_tilt, parms->scale,
		parms->k0, parms->k1, parms->u0, parms->v0, a_view, a_nick); 
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
