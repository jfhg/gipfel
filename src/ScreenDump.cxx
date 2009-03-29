//
// Copyright 2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <FL/Fl.H>
#include <FL/x.H>
#include <FL/fl_draw.H>

#include "ScreenDump.H"

ScreenDump::ScreenDump(Fl_Widget *widget) {
	Fl_Offscreen offscreen;
	int x, y;

	x = widget->x();
	y = widget->y();
	w = widget->w();
	h = widget->h();

	Fl::flush();
	widget->resize(0, 0, w, h);

	offscreen = fl_create_offscreen(w, h);
	fl_begin_offscreen(offscreen);
	widget->redraw();
	widget->draw();
	fl_color(FL_YELLOW);
	fl_draw("created with gipfel", w - 80, h - 10);
	rgb = fl_read_image(NULL, 0, 0, w, h);
	fl_end_offscreen();
	fl_delete_offscreen(offscreen);

	widget->resize(x, y, w, h);
}

ScreenDump::~ScreenDump() {
	if (rgb)
		delete[] rgb;
}

int
ScreenDump::save(OutputImage *out) {
	out->init(w, h);

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			unsigned char *px = &rgb[(y * w + x) * 3];
			out->set_pixel(x, px[0] * 255, px[1] * 255, px[2] * 255);
		}
		out->next_line();
	}

	return out->done();
}
