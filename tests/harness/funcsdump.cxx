// Golden dump of the maxima-generated projection functions (C reference).
// The `#define` trick turns the generated class methods into free functions.
#include <stdio.h>
#include <math.h>

static double sec(double a) { return 1.0 / cos(a); }

// gen_golden.sh copies the generated files with the class scope replaced:
// ProjectionRectilinear::rect_mac_x -> rect_mac_x, ProjectionCylindrical::rect_mac_x -> cyl_mac_x
#include "rect_funcs.cxx"
#include "cyl_funcs.cxx"

int main(void) {
	for (int k = 0; k < 200; k++) {
		double kk = (double) k;
		double c_view = -3.0 + 0.03 * kk;
		double c_nick = 0.10 * sin(0.11 * kk);
		double c_tilt = 0.05 * cos(0.07 * kk);
		double scale = 2000.0 + 37.0 * fmod(kk, 19.0);
		double k0 = 0.001 * sin(0.31 * kk);
		double k1 = 0.0001 * cos(0.17 * kk);
		double x0 = 50.0 * sin(0.13 * kk);
		double m_view = c_view + 0.5 * sin(0.23 * kk);
		double m_nick = 0.05 * cos(0.19 * kk);

		printf("%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g ",
			rect_mac_x(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_y(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_x_dc_view(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_x_dc_nick(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_x_dc_tilt(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_x_dscale(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_x_dk0(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_x_dk1(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_x_dx0(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick));
		printf("%.17g %.17g %.17g %.17g %.17g %.17g %.17g ",
			rect_mac_y_dc_view(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_y_dc_nick(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_y_dc_tilt(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_y_dscale(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_y_dk0(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_y_dk1(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			rect_mac_y_dx0(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick));

		printf("%.17g %.17g ",
			cyl_mac_x(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			cyl_mac_y(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick));
		printf("%.17g %.17g %.17g %.17g ",
			cyl_mac_x_dc_view(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			cyl_mac_x_dc_nick(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			cyl_mac_x_dc_tilt(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			cyl_mac_x_dscale(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick));
		printf("%.17g %.17g %.17g %.17g\n",
			cyl_mac_y_dc_view(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			cyl_mac_y_dc_nick(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			cyl_mac_y_dc_tilt(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick),
			cyl_mac_y_dscale(c_view, c_nick, c_tilt, scale, k0, k1, x0, m_view, m_nick));
	}
	return 0;
}
