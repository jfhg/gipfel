//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef PROJECTIONLSQ_H
#define PROJECTIONLSQ_H

#include "Hill.H"
#include "ViewParams.H"

class ProjectionLSQ {
	private:
		double comp_scale(double alph_a, double alph_b, double d1, double d2);

		int lsq(const Hills *m, ViewParams *parms, int distortion_correct);

	protected:
		static double pi;
		double sec(double a);

	public:
		typedef enum {
			RECTILINEAR = 0,
			CYLINDRICAL = 1
		} Projection_t;

		ProjectionLSQ();
		virtual ~ProjectionLSQ() {};

		void get_coordinates(double a_view, double a_nick,
			const ViewParams *parms, double *x, double *y);

		int comp_params(const Hills *h, ViewParams *parms);

		virtual double get_view_angle();

#define ARGS double c_view, double c_nick, double c_tilt, double scale, double k0, double k1, double x0, double m_view, double m_nick
		virtual double mac_x(ARGS);
		virtual double mac_y(ARGS);
		virtual double mac_x_dc_view(ARGS);
		virtual double mac_x_dc_nick(ARGS);
		virtual double mac_x_dc_tilt(ARGS);
		virtual double mac_x_dscale(ARGS);
		virtual double mac_x_dk0(ARGS);
		virtual double mac_x_dk1(ARGS);
		virtual double mac_x_dx0(ARGS);
		virtual double mac_y_dc_view(ARGS);
		virtual double mac_y_dc_nick(ARGS);
		virtual double mac_y_dc_tilt(ARGS);
		virtual double mac_y_dscale(ARGS);
		virtual double mac_y_dk0(ARGS);
		virtual double mac_y_dk1(ARGS);
		virtual double mac_y_dx0(ARGS);
#undef ARGS

};
#endif
