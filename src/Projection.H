//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef PROJECTION_H
#define PROJECTION_H

#include "Hill.H"
#include "ViewParams.H"

class Projection {
	protected:
		double pi_d;

	public:
		typedef enum {
			TANGENTIAL = 0,
			SPHAERIC   = 1
		} Projection_t;

		Projection();

		virtual void get_coordinates(double a_view, double a_nick,
			const ViewParams *parms, double *x, double *y);

		virtual int comp_params(const Hills *h, ViewParams *parms);
};
#endif
