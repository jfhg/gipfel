#include <stdlib.h>

#include <FL/Fl.H>
#include <FL/x.H>
#include <FL/fl_draw.H>

#include "JPEGOutputImage.H"
#include "ScreenDump.H"

ScreenDump::ScreenDump(GipfelWidget *g) {
	Fl_Offscreen offscreen;

	gipf = g;
	Fl::flush();
	offscreen = fl_create_offscreen(gipf->w(), gipf->h());
	fl_begin_offscreen(offscreen);
	gipf->draw();
	rgb = fl_read_image(NULL, 0, 0, gipf->w(), gipf->h());
	fl_end_offscreen();
	fl_delete_offscreen(offscreen);
}

ScreenDump::~ScreenDump() {
	free(rgb);
}

int
ScreenDump::save(const char *file) {
	JPEGOutputImage out(file, 95);

	out.init(gipf->w(), gipf->h());

	for (int y = 0; y < gipf->h(); y++) {
		for (int x = 0; x < gipf->w(); x++) {
			unsigned char *px = &rgb[(y * gipf->w() + x) * 3];
			out.set_pixel(x, px[0] * 255, px[1] * 255, px[2] * 255);
		}
		out.next_line();
	}

	out.done();
}
