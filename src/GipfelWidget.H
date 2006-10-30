//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef GipfelWidget_H
#define GipfelWidget_H

#include <FL/Fl_Widget.H>
#include <FL/Fl_Menu_Button.H>
#include "Panorama.H"

class GipfelWidget : public Fl_Widget {
 private:
  Fl_Image *img;
  Hill *cur_mountain;
  Hills *marker;
  Hills *track_points;
  Hill *m1, *m2;
  Panorama *pan;
  Fl_Menu_Button *mb;
  char *img_file;
  double track_width;
  int show_hidden;

  int handle(int event);

  int set_cur_mountain(int m_x, int m_y);

  int set_mountain(int m_x, int m_y);

  void set_labels(Hills *v);

  int get_rel_track_width(Hill *m);

  static int get_pixel_nearest(Fl_Image *img, double x, double y,
                     char *r, char *g, char *b);

  static int get_pixel(Fl_Image *img, int x, int y,
                     char *r, char *g, char *b);

 public:
  GipfelWidget(int X,int Y,int W, int H);

  void center();

  int load_image(char *file);

  int save_image(char *file);
	
  const char * get_image_filename();

  int load_data(const char *file);

  int load_track(const char *file);

  int set_viewpoint(const char *pos);

  void set_viewpoint(const Hill *m);

  void set_center_angle(double a);

  void set_nick_angle(double a);

  void set_tilt_angle(double a);

  void set_focal_length_35mm(double s);

  void set_height_dist_ratio(double r);

  void set_hide_value(double h);

  void set_show_hidden(int h);

  void set_view_lat(double v);

  void set_view_long(double v);

  void set_view_height(double v);
  
  const char * get_viewpoint();

  double get_center_angle();

  double get_nick_angle();

  double get_tilt_angle();

  double get_focal_length_35mm();

  double get_height_dist_ratio();

  double get_view_lat();

  double get_view_long();

  double get_view_height();

  void set_track_width(double w);

  Projection::Projection_t get_projection();

  void set_projection(Projection::Projection_t p);

  Hills *get_mountains();

  int comp_params();

  int guess();

  int update();

  int get_pixel(double a_view, double a_nick,
                    char *r, char *g, char *b);

  void draw();
};
#endif
