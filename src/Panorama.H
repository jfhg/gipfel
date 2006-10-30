
#ifndef PANORAMA_H
#define PANORAMA_H

#include "Hill.H"
#include "Projection.H"
#include "ViewParams.H"



class Panorama {
 private:
  double view_phi, view_lam, view_height;
  char *view_name;
  double height_dist_ratio;
  double view_angle;
  double hide_value;
  Hills *mountains;
  Hills *close_mountains;
  Hills *visible_mountains;
  ViewParams parms;
  Projection::Projection *proj;
  Projection::Projection_t projection_type;
 
  Hill * get_pos(const char *name);

  void update_angles();

  void update_coordinates();

  void update_close_mountains();

  void update_visible_mountains();
 
  void mark_hidden(Hills *hills);

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

  double pi_d, deg2rad;

 public:
  Panorama();

  ~Panorama();
  
  int load_data(const char *name);

  void add_hills(Hills *h);

  void remove_trackpoints();

  int set_viewpoint(const char *pos);  

  void set_viewpoint(const Hill *m);  

  void set_height_dist_ratio(double r);

  void set_hide_value(double h);

  Hills * get_mountains();

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

  double get_earth_radius(Hill *m);

  double get_real_distance(Hill *m);

  int comp_params(Hill *m1, Hill *m2);

  int guess(Hills *p1, Hill *m1);

  Projection::Projection_t get_projection();

  void set_projection(Projection::Projection_t p);

  int get_coordinates(double a_view, double a_nick, double *x, double *y);
};
#endif
