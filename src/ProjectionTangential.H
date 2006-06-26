// 
// "$Id: Panorama.H,v 1.20 2005/06/22 19:47:20 hofmann Exp $"
//
// Copyright 2005 by Johannes Hofmann
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

#ifndef PROJECTIONTANGENTIAL_H
#define PROJECTIONTANGENTIAL_H

#include "Hill.H"
#include "Projection.H"

class ProjectionTangential : public Projection {
 private:
  double comp_center_angle(double alph_a, double alph_b, double d1, double d2);

  double comp_scale(double alph_a, double alph_b, double d1, double d2);

  double angle_dist(double a1, double a2);

  int optimize(const Hill *m1, const Hill *m2, ViewParams *parms);

 public:
  void get_coordinates(double a_view, double a_nick, const ViewParams *parms,
		double *x, double *y);
  
  int comp_params(const Hill *m1, const Hill *m2, ViewParams *parms);
};
#endif
