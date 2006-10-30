
#ifndef PROJECTIONSPHAERIC_H
#define PROJECTIONSPHAERIC_H

#include "Hill.H"
#include "Projection.H"

class ProjectionSphaeric : public Projection {
 private:

  double comp_scale(const Hill *m1, const Hill *m2, double d_m1_m2_2);
  double comp_dir_view(const Hill *m1, const Hill *m2, double d_m1_2, double d_m2_2, double scale, double sign3);
  double comp_nick_view(const Hill *m1, const Hill *m2, double d_m1_2, double scale, double dir_view, double sign1);
  double comp_tilt_view(const Hill *m, double scale, double dir_view, double nick_view);

 public:
  void get_coordinates(double a_view, double a_nick, const ViewParams *parms,
		double *x, double *y);

  int comp_params(const Hill *m1, const Hill *m2, ViewParams *parms);
};
#endif
