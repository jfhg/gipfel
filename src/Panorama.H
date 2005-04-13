// 
// "$Id: Panorama.H,v 1.2 2005/04/13 19:09:19 hofmann Exp $"
//
// X11 header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 2004 by Johannes Hofmann
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

#ifndef PANORAMA_H
#define PANORAMA_H

#include "Mountain.H"

class Panorama {
 private:
  double view_phi, view_lam;
  double height_dist_ratio;
  Mountain *mountains;
  Mountain *visible_mountains;
  double pi, deg2rad;
  double center_angle;
  double scale;
  
  int get_pos(const char *name, double *phi, double *lam);

  void update_visible_mountains();

  double distance(double phi, double lam);

  double sin_alpha(double lam, double phi, double c);

  double cos_alpha(double phi, double c);

  double alpha(double phi, double lam);

 public:
  Panorama();

  ~Panorama();
  
  int load_file(const char *name);

  int set_viewpoint(const char *pos);  

  void set_height_dist_ratio(double r);

  Mountain * get_visible_mountains();

  int get_x(Mountain *m);
};
#endif
