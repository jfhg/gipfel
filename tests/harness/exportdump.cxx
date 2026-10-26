// Golden dump: export-hills pipeline through the original C++ Panorama +
// ImageMetaData (mirrors GipfelWidget::load_image + export_hills, no FLTK).
// Usage: exportdump <img> <gipfel.dat>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <exiv2/exiv2.hpp>

#include "Panorama.H"
#include "Hill.H"
#include "ImageMetaData.H"

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "usage: exportdump <img> <gipfel.dat>\n");
		return 1;
	}

	int w = 0, h = 0;
	{
		auto im = Exiv2::ImageFactory::open(argv[1]);
		im->readMetadata();
		w = im->pixelWidth();
		h = im->pixelHeight();
	}

	ImageMetaData md;
	md.load_image(argv[1]);

	Panorama pan;
	pan.set_view_long(md.longitude());
	pan.set_view_lat(md.latitude());
	pan.set_view_height(md.height());
	pan.set_projection((ProjectionLSQ::Projection_t) md.projection_type());

	int have = 1;
	double v;
	v = md.direction();
	if (std::isnan(v)) { pan.set_center_angle(0.0); have = 0; } else pan.set_center_angle(v);
	v = md.nick();
	if (std::isnan(v)) { pan.set_nick_angle(0.0); have = 0; } else pan.set_nick_angle(v);
	v = md.tilt();
	if (std::isnan(v)) { pan.set_tilt_angle(0.0); have = 0; } else pan.set_tilt_angle(v);
	v = md.focal_length_35mm();
	if (std::isnan(v) || v == 0.0) { pan.set_scale(35.0 * (double)(w > h ? w : h) / 35.0); have = 0; }
	else pan.set_scale(v * (double)(w > h ? w : h) / 35.0);

	md.distortion_params(&pan.parms.k0, &pan.parms.k1, &pan.parms.x0);
	if (std::isnan(pan.parms.k0)) pan.parms.k0 = 0.0;
	if (std::isnan(pan.parms.k1)) pan.parms.k1 = 0.0;
	if (std::isnan(pan.parms.x0)) pan.parms.x0 = 0.0;

	if (pan.load_data(argv[2])) return 1;
	pan.set_height_dist_ratio(0.07);

	if (!have) {
		fprintf(stderr, "No gipfel info available.\n");
		return 0;
	}

	printf("#\n# name\theight\tx\ty\tdistance\tflags\n#\n");
	Hills *mnts = pan.get_visible_mountains();
	for (int i = 0; i < mnts->get_num(); i++) {
		Hill *m = mnts->get(i);
		int _x = (int) rint(m->x) + w / 2;
		int _y = (int) rint(m->y) + h / 2;

		if (m->flags & Hill::DUPLIC || m->flags & Hill::HIDDEN)
			continue;
		if (_x < 0 || _x > w || _y < 0 || _y > h)
			continue;

		printf("%s\t%d\t%d\t%d\t%d\n", m->name, (int) rint(m->height), _x, _y,
			(int) rint(pan.get_real_distance(m)));
	}
	return 0;
}
