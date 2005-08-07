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

#ifndef PROJECTIONSPHAERIC_H
#define PROJECTIONSPHAERIC_H

#include "Hill.H"
#include "Projection.H"

class ProjectionSphaeric : public Projection {
 private:

  double comp_scale(Hill *m1, Hill *m2, double d_m1_m2_2);
  double comp_dir_view(Hill *m1, Hill *m2, double d_m1_2, double d_m2_2, double scale);
  double comp_nick_view(Hill *m1, Hill *m2, double d_m1_2, double scale, double dir_view);
  double comp_tilt_view(Hill *m, double scale, double dir_view, double nick_view);

 public:
  void set_coordinates(Hill *m, const ViewParams *parms);
  
  int comp_params(Hill *m1, Hill *m2, ViewParams *parms);
};
#endif
