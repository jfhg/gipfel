//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdio.h>
#include <math.h>

#include "ProjectionCylindrical.H"

#include "ProjectionCylindrical_funcs.cxx"

int
ProjectionCylindrical::comp_params(const Hills *h, ViewParams *parms) {
	Hills h_monotone(h);

	h_monotone.sort(Hills::SORT_X);

	// ensure that alpha is increasing with x.
	for (int i = 1; i < h_monotone.get_num(); i++)
		if (h_monotone.get(i)->alph < h_monotone.get(i - 1)->alph)
			h_monotone.get(i)->alph += asin(1.0) * 4.0; // += 2pi

	return ProjectionLSQ::comp_params(&h_monotone, parms);
}
