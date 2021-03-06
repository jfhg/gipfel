//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef PROJECTIONRECTILINEAR_H
#define PROJECTIONRECTILINEAR_H

#include "ProjectionLSQ.H"

class ProjectionRectilinear : public ProjectionLSQ {
	public:

		virtual double get_view_angle() {return 1.0471976;}; /* 60 deg */

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
