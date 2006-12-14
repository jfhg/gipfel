//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <math.h>

#include "Projection.H"

Projection::Projection() {
	pi_d = asin(1.0) * 2.0;
};

void
Projection::get_coordinates(double a_view, double a_nick,
	const ViewParams *parms, double *x, double *y) {
	fprintf(stderr, "Error: Projection::set_coordinates()\n");
}

int 
Projection::comp_params(const Hills *h, ViewParams *parms) {
	fprintf(stderr, "Error: Projection::comp_params()\n");
	return 1;
}
