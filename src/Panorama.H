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

#ifndef PANORAMA_H
#define PANORAMA_H

#include "Hill.H"

class Panorama {
 private:
  double view_phi, view_lam, view_height;
  char *view_name;
  double height_dist_ratio;
  Hills *mountains;
  Hills *close_mountains;
  Hills *visible_mountains;
  int x1, y1, x2, y2;
  double a_center;
  double scale;
  double a_nick;
  double a_tilt;
 
  int get_pos(const char *name, double *phi, double *lam, double *height);

  void update_angles();

  void update_coordinates();

  void update_close_mountains();

  void update_visible_mountains();

  double distance(double phi, double lam);

  double sin_alpha(double lam, double phi, double c);

  double cos_alpha(double phi, double c);

  double alpha(double phi, double lam);

  double nick(double dist, double height);
  
  double comp_center_angle(double alph_a, double alph_b, double d1, double d2);

  double comp_scale(double alph_a, double alph_b, double d1, double d2);

  int get_matrix(double m[]);

  int optimize(Hill *m1, Hill *m2);

  double get_value(Hills *p);

 public:
  Panorama();

  ~Panorama();
  
  int load_data(const char *name);

  void add_hills(Hills *h);

  void remove_trackpoints();

  int set_viewpoint(const char *pos);  

  void set_height_dist_ratio(double r);

  Hills * get_close_mountains();

  Hills * get_visible_mountains();

  void set_center_angle(double a);

  void set_nick_angle(double a);

  void set_tilt_angle(double a);

  void set_scale(double s);

  void set_view_lat(double v);

  void set_view_long(double v);

  void set_view_height(double v);

  const char * get_viewpoint();  

  double get_center_angle();

  double get_nick_angle();

  double get_tilt_angle();

  double get_scale();

  double get_height_dist_ratio();
  
  double get_view_lat();

  double get_view_long();

  double get_view_height();

  int comp_params(Hill *m1, Hill *m2);

  int guess(Hills *p1, Hill *m1);
};
#endif
