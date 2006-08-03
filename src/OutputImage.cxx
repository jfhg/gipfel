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

#include "OutputImage.H"

OutputImage::OutputImage() {
	W = 0;
	H = 0;
	initialized = 0;
}

OutputImage::~OutputImage() {
}

int
OutputImage::init(int w1, int h1) {
	W = w1;
	H = h1;
	line = 0;
	initialized = 1;

	return init_internal(w1, h1);
}


int
OutputImage::init_internal(int w1, int h1) {
	return 0;
}

int
OutputImage::set_pixel(int x, char r, char g, char b) {
	if (!initialized || x < 0 || x >= W) {
		return 1;
	} else {
		return set_pixel_internal(x, r, g, b);
	}
}

int
OutputImage::set_pixel_internal(int x, char r, char g, char b) {
	return 0;
}

int
OutputImage::next_line() {
	if (!initialized || line++ >= H) {
		return 1;
	} else {
		return next_line_internal();
	}
}

int
OutputImage::next_line_internal() {
	return 0;
}

int
OutputImage::done() {
	if (!initialized) {
		return 1;
	} else {
		next_line();
		return done_internal();
	}
}

int
OutputImage::done_internal() {
	return 0;
}

