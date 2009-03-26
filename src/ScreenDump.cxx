#include <stdlib.h>

#include <FL/Fl.H>
#include <FL/x.H>
#include <FL/fl_draw.H>

#include "JPEGOutputImage.H"
#include "ScreenDump.H"

ScreenDump::ScreenDump(GipfelWidget *gipf) {
	Fl_Offscreen offscreen;

	w = gipf->w();
	h = gipf->h();

	Fl::flush();
	offscreen = fl_create_offscreen(w, h);
	fl_begin_offscreen(offscreen);
	gipf->draw();
	rgb = fl_read_image(NULL, 0, 0, w, h);
	fl_end_offscreen();
	fl_delete_offscreen(offscreen);
}

ScreenDump::~ScreenDump() {
	free(rgb);
}

int
ScreenDump::save(const char *file) {
	JPEGOutputImage out(file, 95);

	out.init(w, h);

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			unsigned char *px = &rgb[(y * w + x) * 3];
			out.set_pixel(x, px[0] * 255, px[1] * 255, px[2] * 255);
		}
		out.next_line();
	}

	return out.done();
}
