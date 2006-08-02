// 
// DataImage routines.
//
// Copyright 2006 by Johannes Hofmann
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//

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
	row = 0;
	size(w, h);
	return 0;
}
	


int
PreviewOutputImage::set_pixel_internal(int x, char r, char g, char b) {
	if (!data) {
		return 1;
	}

	long index = (row * w() * d + (x * d));
	*(data+index+0) = r;
	*(data+index+1) = g;
	*(data+index+2) = b;

	return 0;
}

int
PreviewOutputImage::next_line_internal() {
	row++;
	if (row % (h() / 100) == 0) {
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

