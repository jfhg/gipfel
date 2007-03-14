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
	V1 = 0.213895;
	V2 = 0.089561;

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

typedef struct {
	int c1[3];
	int c2[3];
} sample_t;

static int
get_color_adjust(const sample_t *samples, int n_samples,
	const double *color_adjust_1, double *color_adjust_2) {

	gsl_matrix *X, *cov;
	gsl_vector *yv,  *r;
	double chisq;
	
	X = gsl_matrix_alloc(n_samples, 1);
	yv = gsl_vector_alloc(n_samples);
	r = gsl_vector_alloc(1);
	cov = gsl_matrix_alloc (1, 1);

	for (int c = 0; c<3; c++) {
		for (int i = 0; i< n_samples; i++) {
			gsl_matrix_set(X, i, 0, samples[i].c2[c]);
			gsl_vector_set(yv, i, samples[i].c1[c] * color_adjust_1[c]); 
		}

		
        gsl_multifit_linear_workspace * work 
           = gsl_multifit_linear_alloc (n_samples, 1);

        gsl_multifit_linear (X, yv, r, cov, &chisq, work);
		gsl_multifit_linear_free (work);

		color_adjust_2[c] = gsl_vector_get(r,0);
	}

	return 0;

}



int
Stitch::color_calib(GipfelWidget::sample_mode_t m,
	int w, int h, double view_start, double view_end) {

	sample_t * sample_points[MAX_PICS][MAX_PICS];

	memset(sample_points, 0, sizeof(sample_points));

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

	for(int j=0; j<3; j++) {
		color_adjust[0][j] = 1.0;
	}

	for (int i=1; i<MAX_PICS; i++) {
		for(int j=0; j<3; j++) {
			color_adjust[i][j] = 0.0;
		}
	}

	for (int y=0; y<h; y++) {
		double a_nick = atan((double)(y_off - y)/radius);

		for (int x=0; x<w; x++) {
			double a_view;
			a_view = view_start + x * step_view;
			merged_pixel_set = 0;
			uchar c1[3], c2[3];

			for (int p1=0; p1<MAX_PICS; p1++) {

				if (gipf[p1] == NULL) {
					break;
				} else if (gipf[p1]->get_pixel(m, a_view, a_nick,
						&c1[0], &c1[1], &c1[2]) == 0) {

					double a1 = gipf[p1]->get_angle_off(a_view, a_nick);
					double devign = ( 1 + a1 * a1 * V2 + a1*a1*a1*a1 * V1);
					for (int l=0; l<3;l++) {
						c1[l]=((uchar)MIN(rint((double) c1[l] * devign), 255));
					}

					for (int p2=0; p2<MAX_PICS; p2++) {

						if (p1 == p2) {
							continue;
						} else if (gipf[p2] == NULL) {
							break;
						} else if (gipf[p2]->get_pixel(m, a_view, a_nick,
								&c2[0], &c2[1], &c2[2]) == 0) {
							double a2 = gipf[p2]->get_angle_off(a_view, a_nick);
							devign = ( 1 + a2 * a2 * V2 + a2*a2*a2*a2 * V1);
							for (int l=0; l<3;l++) {
								c2[l]=((uchar)MIN(rint((double) c2[l] * devign), 255));
							}

							if (fabs(a1-a2) < 0.02 &&
								fabs(((double) c1[1]) / ((double) c1[0]) -
									((double) c2[1]) / ((double) c2[0])) < 1.0 && 
								fabs(((double) c1[2]) / ((double) c1[0]) -
									((double) c2[2]) / ((double) c2[0])) < 1.0) {

								if (sample_points[p1][p2] == NULL) {
									fprintf(stderr, "allocated sample_points [%d][%d]\n", p1, p2);
									sample_points[p1][p2] = (sample_t*)
									calloc(500, sizeof(sample_t));
								}

								for (int k=0; k < 500; k++) {
									if (sample_points[p1][p2][k].c1[0] == 0) {
										for (int l=0; l < 3; l++) {
											sample_points[p1][p2][k].c1[l] = c1[l];
											sample_points[p1][p2][k].c2[l] = c2[l];
										}
										break;
									}

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

	for (int n=0; n<20; n++) {	
		for (int i=0; i<MAX_PICS; i++) {
			if (color_adjust[i][0] > 0.0) {
				for (int j=1; j<MAX_PICS; j++) {
					if (sample_points[i][j] != NULL && color_adjust[j][0] <= 0.0) {
						get_color_adjust(sample_points[i][j], 500, color_adjust[i], color_adjust[j]);


fprintf(stderr, "===> Adjust %d: %lf %lf %lf\n", j, color_adjust[j][0], color_adjust[j][1], color_adjust[j][2]);
					}
				}

			}
		}
	}


	return 0;
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

	int max_samples = 10000, n_samples = 0;
	gsl_matrix *X, *cov;
	gsl_vector *yv,  *c;
	double chisq;

	X = gsl_matrix_alloc(max_samples, 2);
	yv = gsl_vector_alloc(max_samples);
	c = gsl_vector_alloc(2);
	cov = gsl_matrix_alloc (2, 2);

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
					l2 = (double) c2[0] * color_adjust[i][0] +
						(double) c2[1] * color_adjust[i][1] +
						(double) c2[2] * color_adjust[i][2];
					a2 = gipf[i]->get_angle_off(a_view, a_nick);

					if (l1 > 0.0) {

						if (n_samples < max_samples &&
							fabs(a1 - a2) > 0.02 &&
							c1[0] < 200 && c1[1] < 200 && c1[2] < 200 &&
							c2[0] < 200 && c2[1] < 200 && c2[2] < 200 &&
							fabs(((double) c1[1]) / ((double) c1[0]) -
								((double) c2[1]) / ((double) c2[0])) < 0.05 && 
							fabs(((double) c1[2]) / ((double) c1[0]) -
								((double) c2[2]) / ((double) c2[0])) < 0.05) {

							gsl_matrix_set(X, n_samples, 0, l2 *  a2 * a2 *a2 * a2 - l1 * a1 *a1 *a1 * a1);
							gsl_matrix_set(X, n_samples, 1, l2 * a2 * a2 - l1 * a1 * a1);
							gsl_vector_set(yv, n_samples, l1 - l2);
							n_samples++;

							if (merged_image) {
								double d = fabs(V2 * (l2 * a2 * a2 - l1 * a1 * a1) + V1 * (l2 *  a2 * a2 *a2 - l1 * a1 * a1 *a1) - (l1 -l2)); 

								merged_image->set_pixel(x, (uchar) rint(d * 2), 0, 0);	
								merged_pixel_set++;
							}


						}
					}
						
					l1 = l2;
					a1 = a2;
					c1[0] = c2[0];
					c1[1] = c2[1];
					c1[2] = c2[2];

					if (!merged_pixel_set && merged_image) {
						merged_image->set_pixel(x, c1[0], c1[1], c1[2]);
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
           = gsl_multifit_linear_alloc (n_samples, 2);

	gsl_multifit_linear (X, yv, c, cov, &chisq, work);
         gsl_multifit_linear_free (work);

	V1 = gsl_vector_get(c,0);
	V2 = gsl_vector_get(c,1);

	fprintf(stderr, "===>  v1 %lf v2 %lf, (i %d)\n", 
		V1, V2, n_samples);

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
					double a2 = gipf[i]->get_angle_off(a_view, a_nick);
					double devign = ( 1 + a2 * a2 * V2 + a2*a2*a2*a2 * V1);

#if 1 
					r = (uchar) MIN(rint(((double) r) * devign * color_adjust[i][0]), 255);
					g = (uchar) MIN(rint(((double) g) * devign * color_adjust[i][1]), 255);
					b = (uchar) MIN(rint(((double) b) * devign * color_adjust[i][2]), 255);
#else 

					r = (uchar) MIN(rint((double) 100 * devign), 255);
					g = (uchar) MIN(rint((double) 100 * devign), 255);
					b = (uchar) MIN(rint((double) 100 * devign), 255);
#endif


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

