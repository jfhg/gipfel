//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include "PreviewOutputImage.H"

PreviewOutputImage::PreviewOutputImage(int X, int Y, int W, int H):
	OutputImage(), Fl_Widget(X, Y, W, H) {
	d = 3;
	data = NULL;
}

PreviewOutputImage::~PreviewOutputImage() {
	if (data)
		free(data);
}

int
PreviewOutputImage::init_internal() {
	data = (uchar*) malloc(W * H * d);
	memset(data, 0, W * H * d);
	size(W, H);
	return 0;
}

int
PreviewOutputImage::set_pixel_internal(int x, int r, int g, int b) {
	if (!data) 
		return 1;

	long index = (line * w() * d + (x * d));
	*(data+index+0) = (unsigned char) (r / 255);
	*(data+index+1) = (unsigned char) (g / 255);
	*(data+index+2) = (unsigned char) (b / 255);

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

void
PreviewOutputImage::draw() {
	if (!data)
		return;

	fl_push_clip(x(), y(), w(), h());
	fl_draw_image(data, x(), y(), w(), h(), d);
	fl_pop_clip();
}
