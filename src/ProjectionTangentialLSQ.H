//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef PROJECTIONTANGENTIALLSQ_H
#define PROJECTIONTANGENTIALLSQ_H

#include "Hill.H"
#include "Projection.H"

class ProjectionTangentialLSQ : public Projection {
	private:
		double comp_scale(double alph_a, double alph_b, double d1, double d2);

		int lsq(const Hills *m, ViewParams *parms);

	public:
		void get_coordinates(double a_view, double a_nick,
			const ViewParams *parms, double *x, double *y);

		int comp_params(const Hills *h, ViewParams *parms);
};
#endif
