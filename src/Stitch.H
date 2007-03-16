//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef STITCH_H
#define STITCH_H

#include "GipfelWidget.H"
#include "OutputImage.H"

#define MAX_PICS 256



class Stitch {
	private:
		GipfelWidget *gipf[MAX_PICS];
		int num_pics;
		double color_adjust[MAX_PICS][3];
		double V1, V2;
		OutputImage *single_images[MAX_PICS];
		OutputImage *merged_image;

		int color_correct(int c, double a, int pic, int color);

	public:
		Stitch();

		~Stitch();

		int load_image(char *file);

		OutputImage * set_output(OutputImage *img);

		OutputImage * set_output(const char *file, OutputImage *img);

		int resample(GipfelWidget::sample_mode_t m,
			int w, int h, double view_start, double view_end);

		int vignette_calib(GipfelWidget::sample_mode_t m,
			int w, int h, double view_start, double view_end);
};

#endif
