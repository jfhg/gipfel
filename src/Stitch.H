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
		OutputImage *merged_image;

	public:
		Stitch();

		~Stitch();

		int load_image(char *file);

		OutputImage * set_output(OutputImage *img);

		int resample(GipfelWidget::sample_mode_t m,
			int w, int h, double view_start, double view_end);
};

#endif
