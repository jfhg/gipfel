//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
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

#include "ProjectionLSQ.H"

double ProjectionLSQ::pi = asin(1.0) * 2.0;

ProjectionLSQ::ProjectionLSQ() {
}

double
ProjectionLSQ::sec(double a) {
	return 1.0 / cos(a);
}

double
ProjectionLSQ::get_view_angle() {
	return 0.0;
}

int
ProjectionLSQ::comp_params(const Hills *h, ViewParams *parms) {
	const Hill *m1, *m2;
	double scale_tmp;
	int distortion_correct = 0;

	if (h->get_num() < 2) {
		fprintf(stderr, "Please position at least 2 hills\n");
		return 1;
	} else if (h->get_num() > 3) {
		fprintf(stderr, "Performing calibration\n");
		distortion_correct = 1;
		parms->k0 = 0.0;
		parms->k1 = 0.0;
		parms->x0 = 0.0;
	}

	m1 = h->get(0);
	m2 = h->get(1);

	scale_tmp = comp_scale(m1->alph, m2->alph, m1->x, m2->x);

	if (isnan(scale_tmp) || scale_tmp < 50.0) {
		fprintf(stderr, "Could not determine initial scale value (%f)\n",
			scale_tmp);
		return 1;
	}

	parms->a_center = m1->alph;
	parms->scale    = scale_tmp;
	parms->a_nick   = 0.0;
	parms->a_tilt   = 0.0;

	lsq(h, parms, 0);

	if (distortion_correct) {
		lsq(h, parms, 1);
	}

	return 0;
}

struct data {
	ProjectionLSQ *p;
	int distortion_correct;
	const Hills *h;
	const ViewParams *old_params;
};

#define CALL(A) dat->p->A(c_view, c_nick, c_tilt, scale, k0, k1, x0, m->alph, m->a_nick) 

static int
lsq_f (const gsl_vector * x, void *data, gsl_vector * f) {
	struct data *dat = (struct data *) data;
	double c_view, c_nick, c_tilt, scale, k0, k1, x0;

	c_view = gsl_vector_get (x, 0);
	c_nick = gsl_vector_get (x, 1);
	c_tilt = gsl_vector_get (x, 2);
	scale = gsl_vector_get (x, 3);
	if (dat->distortion_correct) {
		k0  = gsl_vector_get (x, 4);
		k1 = gsl_vector_get (x, 5);
		x0 = gsl_vector_get (x, 6);
	} else {
		k0 = dat->old_params->k0;
		k1 = dat->old_params->k1;
		x0 = dat->old_params->x0;
	}

	for (int i=0; i<dat->h->get_num(); i++) {
		Hill *m = dat->h->get(i);

		double mx = CALL(mac_x);
		double my = CALL(mac_y);

		gsl_vector_set (f, i*2, mx - m->x);
		gsl_vector_set (f, i*2+1, my - m->y);
	}

	return GSL_SUCCESS;
}

 
static int
lsq_df (const gsl_vector * x, void *data, gsl_matrix * J) {
	struct data *dat = (struct data *) data;
    double c_view, c_nick, c_tilt, scale, k0, k1, x0;

    c_view = gsl_vector_get (x, 0);
    c_nick = gsl_vector_get (x, 1);
    c_tilt = gsl_vector_get (x, 2);
    scale = gsl_vector_get (x, 3);
    if (dat->distortion_correct) {
        k0  = gsl_vector_get (x, 4);
        k1 = gsl_vector_get (x, 5);
        x0 = gsl_vector_get (x, 6);
    } else {
        k0 = dat->old_params->k0;
        k1 = dat->old_params->k1;
        x0 = dat->old_params->x0;
    }            

	for (int i=0; i<dat->h->get_num(); i++) {
		Hill *m = dat->h->get(i);

		gsl_matrix_set (J, 2*i, 0, CALL(mac_x_dc_view));
		gsl_matrix_set (J, 2*i, 1, CALL(mac_x_dc_nick));
		gsl_matrix_set (J, 2*i, 2, CALL(mac_x_dc_tilt));
		gsl_matrix_set (J, 2*i, 3, CALL(mac_x_dscale));
		if (dat->distortion_correct) {
			gsl_matrix_set (J, 2*i, 4, CALL(mac_x_dk0));
			gsl_matrix_set (J, 2*i, 5, CALL(mac_x_dk1));
			gsl_matrix_set (J, 2*i, 6, CALL(mac_x_dx0));
		}

		gsl_matrix_set (J, 2*i+1, 0, CALL(mac_y_dc_view));
		gsl_matrix_set (J, 2*i+1, 1, CALL(mac_y_dc_nick));
		gsl_matrix_set (J, 2*i+1, 2, CALL(mac_y_dc_tilt));
		gsl_matrix_set (J, 2*i+1, 3, CALL(mac_y_dscale));
		if (dat->distortion_correct) {
			gsl_matrix_set (J, 2*i+1, 4, CALL(mac_y_dk0));
			gsl_matrix_set (J, 2*i+1, 5, CALL(mac_y_dk1));
			gsl_matrix_set (J, 2*i+1, 6, CALL(mac_y_dx0));
		}
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
ProjectionLSQ::lsq(const Hills *h, ViewParams *parms,
	int distortion_correct) {

	const gsl_multifit_fdfsolver_type *T;
	gsl_multifit_fdfsolver *s;
	gsl_multifit_function_fdf f;
	struct data dat;
	double x_init[8];
	gsl_vector_view x;
	int status;
	int num_params = distortion_correct?7:4;

	dat.p = this;
	dat.distortion_correct = distortion_correct;
	dat.h = h;
	dat.old_params = parms;

	x_init[0] = parms->a_center;
	x_init[1] = parms->a_nick;
	x_init[2] = parms->a_tilt;
	x_init[3] = parms->scale;
	x_init[4] = parms->k0;
	x_init[5] = parms->k1;
	x_init[6] = parms->x0;

	x = gsl_vector_view_array (x_init, num_params);

   f.f = &lsq_f;
   f.df = &lsq_df;
   f.fdf = &lsq_fdf;
   f.n = h->get_num() * 2;
   f.p = num_params;
   f.params = &dat;

	T = gsl_multifit_fdfsolver_lmsder;
	s = gsl_multifit_fdfsolver_alloc (T, h->get_num() * 2, num_params);
	gsl_multifit_fdfsolver_set (s, &f, &x.vector);

    for (int i=0; i<100; i++) {

		status = gsl_multifit_fdfsolver_iterate (s);
		if (status) {
			break;
		}
	} 

	parms->a_center = gsl_vector_get(s->x, 0);
	parms->a_nick = gsl_vector_get(s->x, 1);
	parms->a_tilt = gsl_vector_get(s->x, 2);
	parms->scale = gsl_vector_get(s->x, 3);

	if (distortion_correct) {
		parms->k0 = gsl_vector_get(s->x, 4);
		parms->k1 = gsl_vector_get(s->x, 5);
		parms->x0 = gsl_vector_get(s->x, 6);
	}

	gsl_multifit_fdfsolver_free (s);

	return 0;
}

void 
ProjectionLSQ::get_coordinates(double alph, double a_nick,
	const ViewParams *parms, double *x, double *y) {

	// Normalize alph - parms->a_center to [-pi/2, pi/2]
	if (alph - parms->a_center > pi) {
		alph -= 2.0 * pi;
	} else if (alph - parms->a_center < -pi) {
		alph += 2.0 * pi;
	}

	*x = mac_x(parms->a_center, parms->a_nick, parms->a_tilt, parms->scale,
		parms->k0, parms->k1, parms->x0, alph, a_nick); 
	*y = mac_y(parms->a_center, parms->a_nick, parms->a_tilt, parms->scale,
		parms->k0, parms->k1, parms->x0, alph, a_nick); 
}

double
ProjectionLSQ::comp_scale(double a1, double a2, double d1, double d2) {
	return (fabs(d1 - d2) / fabs(a1 - a2));
}

#define ARGS double c_view, double c_nick, double c_tilt, double scale, double k0, double k1, double x0, double m_view, double m_nick

double ProjectionLSQ::mac_x(ARGS) {return NAN;}
double ProjectionLSQ::mac_y(ARGS) {return NAN;}
double ProjectionLSQ::mac_x_dc_view(ARGS) {return NAN;}
double ProjectionLSQ::mac_x_dc_nick(ARGS) {return NAN;}
double ProjectionLSQ::mac_x_dc_tilt(ARGS) {return NAN;}
double ProjectionLSQ::mac_x_dscale(ARGS) {return NAN;}
double ProjectionLSQ::mac_x_dk0(ARGS) {return NAN;}
double ProjectionLSQ::mac_x_dk1(ARGS) {return NAN;}
double ProjectionLSQ::mac_x_dx0(ARGS) {return NAN;}
double ProjectionLSQ::mac_y_dc_view(ARGS) {return NAN;}
double ProjectionLSQ::mac_y_dc_nick(ARGS) {return NAN;}
double ProjectionLSQ::mac_y_dc_tilt(ARGS) {return NAN;}
double ProjectionLSQ::mac_y_dscale(ARGS) {return NAN;}
double ProjectionLSQ::mac_y_dk0(ARGS) {return NAN;}
double ProjectionLSQ::mac_y_dk1(ARGS) {return NAN;}
double ProjectionLSQ::mac_y_dx0(ARGS) {return NAN;}
