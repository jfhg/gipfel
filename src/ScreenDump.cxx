#include <FL/Fl.H>
#include <FL/x.H>
#include <FL/fl_draw.H>

#include "ScreenDump.H"

ScreenDump::ScreenDump(Fl_Widget *widget) {
	Fl_Offscreen offscreen;

	w = widget->w();
	h = widget->h();

	Fl::flush();
	offscreen = fl_create_offscreen(w, h);
	fl_begin_offscreen(offscreen);
	widget->redraw();
	widget->draw();
	rgb = fl_read_image(NULL, 0, 0, w, h);
	fl_end_offscreen();
	fl_delete_offscreen(offscreen);
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
