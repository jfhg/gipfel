// 
// Copyright 2006 by Johannes Hofmann
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//

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
Projection::comp_params(const Hill *m1, const Hill *m2, ViewParams *parms) {
  fprintf(stderr, "Error: Projection::comp_params()\n");
  return 1;
}
