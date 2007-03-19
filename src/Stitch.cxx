//
// Copyright 2007 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <gsl/gsl_multifit.h>

#include <Fl/Fl.H>

#include "OutputImage.H"
#include "Stitch.H"

#define MIN(A,B) ((A)<(B)?(A):(B))
#define MAX(A,B) ((A)>(B)?(A):(B))
#define MAX_VALUE 65025

static double pi_d = asin(1.0) * 2.0;
static double deg2rad = pi_d / 180.0;

Stitch::Stitch() {
	for (int i=0; i<MAX_PICS; i++) {
		gipf[i] = NULL;
		single_images[i] = NULL;
		for (int l=0; l<3; l++) {
			color_adjust[i][l] = 1.0;
		}
	}

	merged_image = NULL;
	num_pics = 0;
	V1 = 0.0;
	V2 = 0.0;
}

Stitch::~Stitch() {
	for (int i=0; i<MAX_PICS; i++) {
		if (gipf[i]) {
			delete(gipf[i]);
		} else {
			break;
		}
	}
}


int
Stitch::load_image(char *file) {
	for (int i=0; i<MAX_PICS; i++) {
		if (gipf[i] == NULL) {
			gipf[i] = new GipfelWidget(0, 0, 800, 600);
			if (gipf[i]->load_image(file) != 0) {
				delete gipf[i];
				gipf[i] = NULL;
			} else {
				num_pics++;
			}
			break;
		}
	}

	return 0;
}

OutputImage*
Stitch::set_output(OutputImage *img) {
	OutputImage *ret = merged_image;
	merged_image = img;
	return ret;
}

OutputImage*
Stitch::set_output(const char *file, OutputImage *img) {
	OutputImage *ret = NULL;

	for (int i=0; i<MAX_PICS; i++) {
		if (gipf[i] != NULL) {
			const char *img_file = gipf[i]->get_image_filename();
			if (img_file && strcmp(file, img_file) == 0) {
				ret = single_images[i];
				single_images[i] = img;
				break;
			}
		}
	}

	return ret;
}

static int
var_offset(int pic, int color) {
	return 2 + pic* 3 + color;
}

static int
similar_color(double *c1, double *c2) {
	for (int i=0; i<3; i++) {
		if (c1[i] > MAX_VALUE * 0.9 || c1[i] < MAX_VALUE * 0.1 ||
			c2[i] > MAX_VALUE * 0.9 || c2[i] < MAX_VALUE * 0.1) {
			return 0;
		}
	}

	if (fabs(c1[1] / c1[0] - c2[1] / c2[0]) < 0.05 && 
		fabs(c1[1] / c1[2] - c2[1] / c2[2]) < 0.05 && 
		fabs(c1[2] / c1[0] - c2[2] / c2[0]) < 0.05 ) {
		return 1;
	} else {
		return 0;
	}
}

double
Stitch::vignetting_parameter(int pic, double a_view, double a_nick) {
	return pow(cos(gipf[pic]->get_angle_off(a_view, a_nick)), 4.0);
}

int
Stitch::vignette_calib(GipfelWidget::sample_mode_t m,
	int w, int h, double view_start, double view_end) {

	w = 1500;
	h = 400;

	view_start = view_start * deg2rad;
	view_end = view_end * deg2rad;

	double step_view = (view_end - view_start) / w;
	int y_off = h / 2;
	int merged_pixel_set;
	double radius = (double) w / (view_end -view_start);

	int max_samples = 20000 * 3, n_samples = 0;
	int ret;
	int n_vars = 2 ;
	gsl_matrix *X, *cov;
	gsl_vector *yv,  *c;
	double chisq;

	X = gsl_matrix_calloc(max_samples, n_vars);
	yv = gsl_vector_calloc(max_samples);
	c = gsl_vector_calloc(n_vars);
	cov = gsl_matrix_calloc (n_vars, n_vars);

	if (merged_image) {
		merged_image->init(w, h);
	}

	for (int y = 0; y < h; y++) {
		double a_nick = atan((double)(y_off - y)/radius);

		for (int x = 0; x < w; x++) {
			double a_view;
			a_view = view_start + x * step_view;
			merged_pixel_set = 0;
			int c1[3], c2[3];
			double c1d[3], c2d[3];

			for (int p1 = 0; p1 < num_pics; p1++) {
				if (get_pixel(m, p1, a_view, a_nick,
						&c1[0], &c1[1], &c1[2]) == 0) {

					for (int l = 0; l<3; l++) {	
						c1d[l] = (double) c1[l];
					}

					double a1 = vignetting_parameter(p1, a_view, a_nick);

					for (int p2 = p1 + 1; p2 < num_pics; p2++) {

						if (get_pixel(m, p2, a_view, a_nick,
								&c2[0], &c2[1], &c2[2]) == 0) {

							for (int l = 0; l<3; l++) {	
								c2d[l] = (double) c2[l];
							}


							if (n_samples < max_samples &&
								similar_color(c1d, c2d)) {

								double a2 = vignetting_parameter(p2, a_view, a_nick);

								for (int l = 0; l<3; l++) {	
									gsl_matrix_set(X, n_samples, 0, c1d[l] * a1);
									gsl_matrix_set(X, n_samples, 1, c1d[l] * a1 * a1);
									gsl_matrix_set(X, n_samples, 0, -c2d[l] * a2);
									gsl_matrix_set(X, n_samples, 1, -c2d[l] * a2 * a2);

									gsl_vector_set(yv, n_samples, c2d[l] - c1d[l]);
									n_samples++;
								}

								if (merged_image) {
									merged_image->set_pixel(x, 65025, 0, 0);	
									merged_pixel_set++;
								}
							}

						}
					}

					if (!merged_pixel_set && merged_image) {
						merged_image->set_pixel(x, c1[0], c1[1], c1[2]);
						merged_pixel_set++;
					}

				}
			}
		}
		if (merged_image) {
			merged_image->next_line();
		}
	}

	if (merged_image) {
		merged_image->done();
	}

	gsl_multifit_linear_workspace * work 
           = gsl_multifit_linear_alloc (max_samples, n_vars);

	ret = gsl_multifit_linear (X, yv, c, cov, &chisq, work);
	gsl_multifit_linear_free (work);

	V1 = gsl_vector_get(c, 0); 
	V2 = gsl_vector_get(c, 1); 
	fprintf(stderr, "==> V1 %f V2 %f\n", V1, V2);

	return 0;
}

int
Stitch::color_correct(int c, double a, int pic, int color) {
	double cd = (double) c;
	
	cd = cd * color_adjust[pic][color] * (1.0 + V1 * a + V2 * a * a);

	return MAX(MIN((int) rint(cd), 65025), 0);
}

int
Stitch::get_pixel(GipfelWidget::sample_mode_t m, int pic,
	double a_view, double a_nick, int *r, int *g, int *b) {

	if (gipf[pic]->get_pixel(m, a_view, a_nick, r, g, b) != 0) {
		return 1;
	}

	double a = vignetting_parameter(pic, a_view, a_nick);

	*r = color_correct(*r, a, pic, 0);
	*g = color_correct(*g, a, pic, 1);
	*b = color_correct(*b, a, pic, 2);

	return 0;
}

int
Stitch::resample(GipfelWidget::sample_mode_t m,
	int w, int h, double view_start, double view_end) {

	view_start = view_start * deg2rad;
	view_end = view_end * deg2rad;

	double step_view = (view_end - view_start) / w;
	int r, g, b;
	int y_off = h / 2;
	int merged_pixel_set;
	double radius = (double) w / (view_end -view_start);

	if (merged_image) {
		merged_image->init(w, h);
	}

	for (int i=0; i<MAX_PICS; i++) {
		if (single_images[i]) {
			single_images[i]->init(w, h);
		}
	}

	for (int y = 0; y < h; y++) {
		double a_nick = atan((double)(y_off - y)/radius);
		double a_max = 0.0;

		for (int x = 0; x < w; x++) {
			double a_view;
			a_view = view_start + x * step_view;
			merged_pixel_set = 0;
			for (int i = 0; i < num_pics; i++) {
				if (get_pixel(m, i, a_view, a_nick,
						&r, &g, &b) == 0) {

					if (single_images[i]) {
						single_images[i]->set_pixel(x, r, g, b);
					}
					if (!merged_pixel_set && merged_image) {
						merged_image->set_pixel(x, r, g, b);
						merged_pixel_set++;
					}
				}
			}
		}
		if (merged_image) {
			merged_image->next_line();
		}
		for (int i=0; i<MAX_PICS; i++) {
			if (single_images[i]) {
				single_images[i]->next_line();
			}
		}
	}

	if (merged_image) {
		merged_image->done();
	}
	for (int i=0; i<MAX_PICS; i++) {
		if (single_images[i]) {
			single_images[i]->done();
		}
	}

	return 0;
}

