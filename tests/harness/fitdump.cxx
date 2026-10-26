// Golden dump: full fit through the original C++ Panorama + GSL solver.
// Usage: fitdump <gipfel.dat> <projection 0|1>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "Panorama.H"
#include "Hill.H"
#include "ProjectionRectilinear.H"
#include "ProjectionCylindrical.H"

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "usage: fitdump <gipfel.dat> <projection>\n");
		return 1;
	}

	Panorama pan;
	if (pan.load_data(argv[1])) {
		fprintf(stderr, "load failed\n");
		return 1;
	}
	pan.set_projection((ProjectionLSQ::Projection_t) atoi(argv[2]));

	pan.set_view_lat(47.36);
	pan.set_view_long(11.15);
	pan.set_view_height(2100.0);

	// ground truth
	pan.set_center_angle(20.0);
	pan.set_nick_angle(0.0);
	pan.set_tilt_angle(1.0);
	pan.set_scale(3000.0);

	// observed positions = projection of first 6 visible hills (snapshotted
	// as copies so the perturbation below does not overwrite them)
	Hills *vis = pan.get_visible_mountains();
	Hills known;
	int n = vis->get_num() < 6 ? vis->get_num() : 6;
	for (int i = 0; i < n; i++) {
		Hill *c = new Hill(*vis->get(i));
		// NOTE: Hill's copy ctor in the original does not copy a_nick
		// (uninitialized garbage -> LSQR stagnates). The real app always
		// fits hills living in the main list where a_nick is set, so set it
		// here to reproduce the real-app behavior.
		c->a_nick = vis->get(i)->a_nick;
		known.add(c);
	}

	// perturb the initial guess
	pan.set_center_angle(pan.get_center_angle() + 0.5);
	pan.set_nick_angle(0.3);
	pan.set_tilt_angle(0.5);
	pan.set_scale(2800.0);

	int ret = pan.comp_params(&known);

	ViewParams *p = &pan.parms;
	printf("ret %d\n", ret);
	printf("a_center %.17g\n", p->a_center);
	printf("a_nick %.17g\n", p->a_nick);
	printf("a_tilt %.17g\n", p->a_tilt);
	printf("scale %.17g\n", p->scale);
	printf("k0 %.17g\n", p->k0);
	printf("k1 %.17g\n", p->k1);
	printf("x0 %.17g\n", p->x0);

	// final image coordinates of the known hills (bypass visibility check)
	for (int i = 0; i < known.get_num(); i++) {
		Hill *m = known.get(i);
		double x = NAN, y = NAN;
		if (atoi(argv[2]) == 1) {
			ProjectionCylindrical pc;
			pc.get_coordinates(m->alph, m->a_nick, &pan.parms, &x, &y);
		} else {
			ProjectionRectilinear pr;
			pr.get_coordinates(m->alph, m->a_nick, &pan.parms, &x, &y);
		}
		printf("hill %d %.17g %.17g\n", i, x, y);
	}

	return 0;
}
