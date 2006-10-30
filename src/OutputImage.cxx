
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

