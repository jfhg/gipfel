//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <Fl/Fl.H>
#include <Fl/fl_draw.h>

#include "PreviewOutputImage.H"

PreviewOutputImage::PreviewOutputImage(int X, int Y, int W, int H): Fl_Widget(X, Y, W, H) {
	d = 3;
	data = NULL;
}

PreviewOutputImage::~PreviewOutputImage() {
	if (data) {
		free(data);
	}
}

int
PreviewOutputImage::init_internal(int w, int h) {
	data = (uchar*) malloc(w * h * d);
	memset(data, 0, w * h * d);
	size(w, h);
	return 0;
}
	


int
PreviewOutputImage::set_pixel_internal(int x, char r, char g, char b) {
	if (!data) {
		return 1;
	}

	long index = (line * w() * d + (x * d));
	*(data+index+0) = r;
	*(data+index+1) = g;
	*(data+index+2) = b;

	return 0;
}

int
PreviewOutputImage::next_line_internal() {
	if (line % 10 == 0) {
		redraw();
		Fl::check();
	}
	return 0;
}

int
PreviewOutputImage::done_internal() {
	return 0;
}

void
PreviewOutputImage::draw() {
	if (!data) {
		return;
	}
	fl_push_clip(x(), y(), w(), h());

	fl_draw_image(data, x(), y(), w(), h(), d);

	fl_pop_clip();
}

