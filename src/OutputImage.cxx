//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>

#include "OutputImage.H"

OutputImage::OutputImage() {
	W = 0;
	H = 0;
	initialized = 0;
}

int
OutputImage::init(int w1, int h1) {
	W = w1;
	H = h1;
	line = 0;
	initialized = 1;

	return init_internal();
}

int
OutputImage::set_pixel(int x, int r, int g, int b) {
	if (!initialized || x < 0 || x >= W)
		return 1;
	else
		return set_pixel_internal(x, r, g, b);
}

int
OutputImage::next_line() {
	if (!initialized || line++ >= H)
		return 1;
	else
		return next_line_internal();
}

int
OutputImage::done() {
	if (!initialized)
		return 1;
	else
		next_line();
		return done_internal();
}
