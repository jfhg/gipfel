// 
// "$Id: Panorama.H,v 1.8 2005/04/25 20:02:56 hofmann Exp $"
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
  double view_phi, view_lam, view_height;
  double height_dist_ratio;
  Mountain *mountains;
  Mountain *visible_mountains;
  Mountain *m1, *m2;
  int x1, y1, x2, y2;
  double pi, deg2rad;
  double a_center;
  double scale;
  double a_nick;
  double a_tilt;
  // these must be set before using newton()
  double tan_dir_view, tan_nick_view, n_scale;
  double tan_dir_m1, tan_nick_m1, tan_dir_m2, tan_nick_m2;
  double d_m1_2, d_m2_2, d_m1_m2_2;

  int get_pos(const char *name, double *phi, double *lam, double *height);

  void update_visible_mountains();

  double distance(double phi, double lam);

  double sin_alpha(double lam, double phi, double c);

  double cos_alpha(double phi, double c);

  double alpha(double phi, double lam);

  double nick(double dist, double height);
  
  double comp_center_angle(double alph_a, double alph_b, double d1, double d2);

  double comp_scale(double alph_a, double alph_b, double d1, double d2);

  double comp_tilt();

  int get_matrix(double m[]);



 public:
  Panorama();

  ~Panorama();
  
  int load_file(const char *name);

  int set_viewpoint(const char *pos);  

  void set_height_dist_ratio(double r);

  Mountain * get_visible_mountains();

  int set_mountain(Mountain *m, int x, int y);

  void set_center_angle(double a);

  void set_nick_angle(double a);

  void set_tilt_angle(double a);

  void set_scale(double s);

  int comp_params();

  int newton();
};
#endif
