//
// Copyright 2007 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>

#include <gsl/gsl_multifit.h>

#include <Fl/Fl.H>

#include "OutputImage.H"
#include "Stitch.H"

#define MAX_VALUE 65025

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
		double a_nick = (double)20.0 - y * step_view;

		for (int x = 0; x < w; x++) {
			double a_view;
			a_view = view_start + x * step_view;
			merged_pixel_set = 0;
			for (int i = 0; i < num_pics; i++) {
				if (gipf[i]->get_pixel(m, a_view, a_nick,
						&r, &g, &b) == 0) {

					r = std::max(std::min(r, MAX_VALUE), 0);
					g = std::max(std::min(g, MAX_VALUE), 0);
					b = std::max(std::min(b, MAX_VALUE), 0);

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
