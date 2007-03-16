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
	num_pics = 0;

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

	int max_samples = 5000 * 3, n_samples = 0;
	int ret;
	int n_vars = 2 + num_pics * 3 ;
	gsl_matrix *X, *cov;
	gsl_vector *yv,  *c;
	double chisq;

	X = gsl_matrix_alloc(max_samples, n_vars);
	gsl_matrix_set_zero(X);
	yv = gsl_vector_alloc(max_samples);
	c = gsl_vector_alloc(n_vars);
	cov = gsl_matrix_alloc (n_vars, n_vars);

	if (merged_image) {
		merged_image->init(w, h);
	}

	for (int y=0; y<h; y++) {
		double a_nick = atan((double)(y_off - y)/radius);

		for (int x=0; x<w; x++) {
			double a_view;
			a_view = view_start + x * step_view;
			merged_pixel_set = 0;
			double a1, a2;
			uchar c1[3], c2[3];
			double c1d[3], c2d[3];

			for (int p1=0; p1<num_pics; p1++) {
				if (gipf[p1] == NULL) {
					break;
				} else if (gipf[p1]->get_pixel(m, a_view, a_nick,
						&c1[0], &c1[1], &c1[2]) == 0) {

					for (int l = 0; l<3; l++) {	
						c1d[l] = (double) c1[l];
					}

					a1 = gipf[p1]->get_angle_off(a_view, a_nick);

					for (int p2=1; p2<num_pics; p2++) {
						if (p1 == p2) {
							continue;
						} else if (gipf[p2] == NULL) {
							break;
						} else if (gipf[p2]->get_pixel(m, a_view, a_nick,
								&c2[0], &c2[1], &c2[2]) == 0) {

							for (int l = 0; l<3; l++) {	
								c2d[l] = (double) c2[l];
							}

							a2 = gipf[p2]->get_angle_off(a_view, a_nick);

							if (n_samples < max_samples &&
								c1[0] < 200 && c1[1] < 200 && c1[2] < 200 &&
								c2[0] < 200 && c2[1] < 200 && c2[2] < 200 &&
								c1[0] > 20 && c1[1] > 20 && c1[2] > 20 &&
								c2[0] > 20 && c2[1] > 20 && c2[2] > 20 &&
								fabs(c1d[1] / c1d[0] - c2d[1] / c2d[0]) < 0.02 && 
								fabs(c1d[2] / c1d[0] - c2d[2] / c2d[0]) < 0.02 ) {

								for (int l = 0; l<3; l++) {	
									if (p1 == 0) {
										gsl_matrix_set(X, n_samples, 0, c1d[l] * a1 * a1);
										gsl_matrix_set(X, n_samples, 1, c1d[l] * a1 *a1 * a1 * a1);
										gsl_matrix_set(X, n_samples, var_offset(p1, l), c1d[l]);
										gsl_vector_set(yv, n_samples, c1d[l]);
										n_samples++;
									}

									gsl_matrix_set(X, n_samples, 0, c1d[l] * a1 * a1);
									gsl_matrix_set(X, n_samples, 0, - c2d[l] * a2 * a2);
									gsl_matrix_set(X, n_samples, 1, c1d[l] * a1 *a1 * a1 * a1);
									gsl_matrix_set(X, n_samples, 1, - c2d[l] * a2 * a2 * a2 * a2);
									gsl_matrix_set(X, n_samples, var_offset(p1, l), c1d[l]);
									gsl_matrix_set(X, n_samples, var_offset(p2, l), - c2d[l]);
									gsl_vector_set(yv, n_samples, 0.0);
									n_samples++;
								}

								if (merged_image) {
									merged_image->set_pixel(x, 255, 0, 0);	
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
           = gsl_multifit_linear_alloc (n_samples, n_vars);

	ret = gsl_multifit_linear (X, yv, c, cov, &chisq, work);
	gsl_multifit_linear_free (work);

	fprintf(stderr, "gsl_multifit_linear returned %d\n", ret);

	for (int p1=0; p1 < num_pics; p1++) {
		for (int l = 0; l<3; l++) {	
			color_adjust[p1][l] = gsl_vector_get(c, var_offset(p1, l)); 
		}
fprintf(stderr, "==> color_adjust(%d) %f %f %f\n", p1, color_adjust[p1][0], color_adjust[p1][1],color_adjust[p1][2]);
	}

	V1 = gsl_vector_get(c, 0); 
	V2 = gsl_vector_get(c, 1); 
fprintf(stderr, "==> V1 %f V2 %f\n", V1, V2);

	return 0;
}

uchar
Stitch::color_correct(uchar c, double a, int pic, int color) {
	double cd = (double) c;
	
	cd = cd * (color_adjust[pic][color] +
		V1 * a * a + 
		V2 * a * a * a * a);

	return (uchar) MIN(rint(cd), 255.0);
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
					double a = gipf[i]->get_angle_off(a_view, a_nick);

					r = color_correct(r, a, i, 0);
					g = color_correct(g, a, i, 1);
					b = color_correct(b, a, i, 2);

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

