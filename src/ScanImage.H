//
// Copyright 2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef ScanImage_H
#define ScanImage_H

#include <FL/Fl_Image.H>

class ScanImage {
	private:
		static int get_pixel_nearest(Fl_Image *img, double x, double y,
			int *r, int *g, int *b);
		static int get_pixel_bicubic(Fl_Image *img, double x, double y,
			int *r, int *g, int *b);
		static int get_pixel(Fl_Image *img, int x, int y,
			int *r, int *g, int *b);

	public:
		typedef enum {
			NEAREST  = 0,
			BICUBIC  = 1
		} mode_t;

		static int get_pixel(Fl_Image *img, mode_t mode,
			double x, double y, int *r, int *g, int *b);
};

#endif
