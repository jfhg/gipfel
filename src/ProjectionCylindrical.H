//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef PROJECTIONCYLINDRICAL_H
#define PROJECTIONCYLINDRICAL_H

#include "ProjectionLSQ.H"

class ProjectionCylindrical : public ProjectionLSQ {
	public:

#define ARGS double c_view, double c_nick, double c_tilt, double scale, double k0, double k1, double m_view, double m_nick

		virtual double mac_x(ARGS);
		virtual double mac_y(ARGS);
        virtual double mac_x_dc_view(ARGS);
		virtual double mac_x_dc_nick(ARGS);
		virtual double mac_x_dc_tilt(ARGS);
		virtual double mac_x_dscale(ARGS);
		virtual double mac_x_dk0(ARGS);
		virtual double mac_x_dk1(ARGS);
		virtual double mac_y_dc_view(ARGS);
		virtual double mac_y_dc_nick(ARGS);
		virtual double mac_y_dc_tilt(ARGS);
		virtual double mac_y_dscale(ARGS);
		virtual double mac_y_dk0(ARGS);
        virtual double mac_y_dk1(ARGS);

#undef ARGS

};
#endif
