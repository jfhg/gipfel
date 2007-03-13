//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
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


static double pi_d = asin(1.0) * 2.0;
static double deg2rad = pi_d / 180.0;

Stitch::Stitch() {
	for (int i=0; i<MAX_PICS; i++) {
		gipf[i] = NULL;
		single_images[i] = NULL;
	}
	merged_image = NULL;
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

int
Stitch::vignette_calib(GipfelWidget::sample_mode_t m,
	int w, int h, double view_start, double view_end) {

	view_start = view_start * deg2rad;
	view_end = view_end * deg2rad;

	double step_view = (view_end - view_start) / w;
	uchar r, g, b;
	int y_off = h / 2;
	int merged_pixel_set;
	double radius = (double) w / (view_end -view_start);

	double V1 = 6.220359, V2 = -13.874930, V3 = 9.992582;
	int max_samples = 10000, n_samples = 0;
	gsl_matrix *X, *cov;
	gsl_vector *yv,  *c;
	double chisq;

	X = gsl_matrix_alloc(max_samples, 1);
	yv = gsl_vector_alloc(max_samples);
	c = gsl_vector_alloc(1);
	cov = gsl_matrix_alloc (1, 1);

	if (merged_image) {
		merged_image->init(w, h);
	}

	for (int y=0; y<h; y++) {
		double a_nick = atan((double)(y_off - y)/radius);

		for (int x=0; x<w; x++) {
			double a_view;
			a_view = view_start + x * step_view;
			merged_pixel_set = 0;
			double l1 = 0.0, l2 = 0.0;
			double a1, a2;
			uchar c1[3], c2[3];

			for (int i=0; i<MAX_PICS; i++) {

				if (gipf[i] == NULL) {
					break;
				} else if (gipf[i]->get_pixel(m, a_view, a_nick,
						&c2[0], &c2[1], &c2[2]) == 0) {
					l2 = (double) c2[0] + c2[1] + c2[2];
					a2 = fabs(gipf[i]->get_angle_off(a_view, a_nick));

					if (l1 > 0.0 && n_samples < max_samples &&
						fabs(a1 - a2) > 0.1 &&
						fabs(((double) c1[1]) / ((double) c1[0]) -
							((double) c2[1]) / ((double) c2[0])) < 0.2 && 
						fabs(((double) c1[2]) / ((double) c1[0]) -
							((double) c2[2]) / ((double) c2[0])) < 0.2) {

						gsl_matrix_set(X, n_samples, 0, l1 * a1 * a1 - l2 * a2 * a2);
						gsl_vector_set(yv, n_samples, l2 - l1);
						n_samples++;

						if (merged_image) {
							merged_image->set_pixel(x, 255, 0, 0);	
							merged_pixel_set++;
						}
					}
						
					l1 = l2;
					a1 = a2;
					c1[0] = c2[0];
					c1[1] = c2[1];
					c1[2] = c2[2];

					if (!merged_pixel_set && merged_image) {
						merged_image->set_pixel(x, r, g, b);
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
           = gsl_multifit_linear_alloc (n_samples, 1);

	gsl_multifit_linear (X, yv, c, cov, &chisq, work);
         gsl_multifit_linear_free (work);

	fprintf(stderr, "===>  v2 %lf, (i %d)\n", 
		gsl_vector_get(c,0), n_samples);

	return 0;
}


int
Stitch::resample(GipfelWidget::sample_mode_t m,
	int w, int h, double view_start, double view_end) {

	view_start = view_start * deg2rad;
	view_end = view_end * deg2rad;

	double step_view = (view_end - view_start) / w;
	uchar r, g, b;
	int y_off = h / 2;
	int merged_pixel_set;
	double radius = (double) w / (view_end -view_start);
	double V1 = 0.0, V2 = 0.16435;

	if (merged_image) {
		merged_image->init(w, h);
	}
	for (int i=0; i<MAX_PICS; i++) {
		if (single_images[i]) {
			single_images[i]->init(w, h);
		}
	}

	for (int y=0; y<h; y++) {
		double a_nick = atan((double)(y_off - y)/radius);
		double a_max = 0.0;

		for (int x=0; x<w; x++) {
			double a_view;
			a_view = view_start + x * step_view;
			merged_pixel_set = 0;
			for (int i=0; i<MAX_PICS; i++) {
				if (gipf[i] == NULL) {
					break;
				} else if (gipf[i]->get_pixel(m, a_view, a_nick,
						&r, &g, &b) == 0) {
					double l2 = (double) r + g + b;
					double a2 = fabs(gipf[i]->get_angle_off(a_view, a_nick));
					double devign = ( 1 + a2 * V1 + a2 * a2 * V2);
if (a2 > a_max) {
fprintf(stderr, "==> %lf %lf\n", devign, a2);
a_max = a2;
}

#if 1 
					r = (uchar) MIN(rint((double) r * devign), 255);
					g = (uchar) MIN(rint((double) g * devign), 255);
					b = (uchar) MIN(rint((double) b * devign), 255);
#else 

					r = (uchar) MIN(rint((double) 250 * devign), 255);
					g = (uchar) MIN(rint((double) 250 * devign), 255);
					b = (uchar) MIN(rint((double) 250 * devign), 255);
#endif


					if (single_images[i]) {
						single_images[i]->set_pixel(x, r
							, g, b);
					}
					if (!merged_pixel_set && merged_image) {
						merged_image->set_pixel(x, r, g,
							b);
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

