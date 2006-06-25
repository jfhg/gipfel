// 
// "$Id: GipfelWidget.cxx,v 1.36 2005/07/04 17:39:32 hofmann Exp $"
//
// GipfelWidget routines.
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_JPEG_Image.H>
#include <FL/fl_draw.H>

#include "Fl_Search_Chooser.H"
#include "DataImage.H"
#include "choose_hill.H"
#include "util.h"
#include "GipfelWidget.H"

#define CROSS_SIZE 2


#define MAX(A,B) ((A)>(B)?(A):(B))

static double pi_d, deg2rad;

static void center_cb(Fl_Widget *o, void *f);

GipfelWidget::GipfelWidget(int X,int Y,int W, int H): Fl_Widget(X, Y, W, H) {
  int i;

  pi_d = asin(1.0) * 2.0;
  deg2rad = pi_d / 180.0;
  img = NULL;
  pan = new Panorama();
  cur_mountain = NULL;
  mb = NULL;
  marker = new Hills();
  m1 = NULL;
  m2 = NULL;
  img_file = NULL;
  track_width = 200.0;
  show_hidden = 0;

  for (i=0; i<=3; i++) {
    marker->add(new Hill(i * 10, 0));
  }
  track_points = NULL;
  fl_register_images();
}

#define GIPFEL_FORMAT "gipfel: longitude %lf, latitude %lf, height %lf, direction %lf, nick %lf, tilt %lf, scale %lf, projection type %d"

int
GipfelWidget::load_image(char *file) {
  char * args[32];
  FILE *p;
  pid_t pid;
  char buf[1024];
  double lo, la, he, dir, ni, ti, sc;
  int status;
  Projection::Projection_t pt = Projection::TANGENTIAL;
  Fl_Image *new_img;

  new_img = new Fl_JPEG_Image(file);
  
  if (new_img == NULL) {
    return 1;
  }

  if (img) {
    delete img;
  } 

  img = new_img;
  
  if (img_file) {
    free(img_file);
  }

  img_file = strdup(file);

  h(img->h());  
  w(img->w());  

  mb = new Fl_Menu_Button(x(),y(),w()+x(),h()+y(),"&popup");
  mb->type(Fl_Menu_Button::POPUP3);
  mb->box(FL_NO_BOX);
  mb->add("Center Peak", NULL, (Fl_Callback*) center_cb, this);

// try to retrieve gipfel data from JPEG comment section
  args[0] = "rdjpgcom";
  args[1] = file;
  args[2] = NULL;
  
  p = pexecvp(args[0], args, &pid, "r");

  if (p) {
    while (fgets(buf, sizeof(buf), p) != NULL) {
      if (sscanf(buf, GIPFEL_FORMAT, &lo, &la, &he, &dir, &ni, &ti, &sc, &pt) >= 7) {
        set_view_long(lo);
        set_view_lat(la);
        set_view_height(he);
        set_center_angle(dir);
        set_nick_angle(ni);
        set_tilt_angle(ti);
        set_scale(sc);
        set_projection(pt);
        
	break;
      }
    }
    fclose(p);
    waitpid(pid, &status, 0); 
    if (WEXITSTATUS(status) == 127 || WEXITSTATUS(status) == 126) {
      fprintf(stderr, "%s not found\n", args[0]);
    }
  }

  return 0;
}

int
GipfelWidget::save_image(char *file) {
  char * args[32];
  FILE *p, *out;
  pid_t pid;
  char buf[1024];
  int status;
  size_t n;
  struct stat in_stat, out_stat;

  if (img_file == NULL) {
    fprintf(stderr, "Nothing to save\n");
    return 1;
  }

  if (stat(img_file, &in_stat) != 0) {
    perror("stat");
    return 1;
  }

  if (stat(file, &out_stat) == 0) {
    if (in_stat.st_ino == out_stat.st_ino) {
      fprintf(stderr, "Input image %s and output image %s are the same file\n",
        img_file, file);
      return 1;
    }
  } 

  out = fopen(file, "w");
  if (out == NULL) {
    perror("fopen");
    return 1;
  }

  snprintf(buf, sizeof(buf), GIPFEL_FORMAT, 
    get_view_long(), 
    get_view_lat(), 
    get_view_height(), 
    get_center_angle(), 
    get_nick_angle(), 
    get_tilt_angle(), 
    get_scale(),
    (int) get_projection());

// try to save gipfel data in JPEG comment section
  args[0] = "wrjpgcom";
  args[1] = "-replace";
  args[2] = "-comment";
  args[3] = buf;
  args[4] = img_file;
  args[5] = NULL;

  p = pexecvp(args[0], args, &pid, "r");

  if (p) {
    while ((n = fread(buf, 1, sizeof(buf), p)) != 0) {
      if (fwrite(buf, 1, n, out) != n) {
        perror("fwrite");
        fclose(out);
        fclose(p);
        waitpid(pid, &status, 0);
       }
    }
    fclose(p);
    waitpid(pid, &status, 0);
    if (WEXITSTATUS(status) == 127 || WEXITSTATUS(status) == 126) {
      fprintf(stderr, "%s not found\n", args[0]);
    }
  }

  fclose(out);
  return 0;
}


int
GipfelWidget::load_data(const char *file) {
  int r;

  r = pan->load_data(file);
  set_labels(pan->get_visible_mountains());

  return r;
}

int
GipfelWidget::load_track(const char *file) {
  if (track_points) {
    pan->remove_trackpoints();
    track_points->clobber();
    delete track_points;
  }

  track_points = new Hills();

  if (track_points->load(file) != 0) {
    delete track_points;
    track_points = NULL;
    return 1;
  }

  for (int i=0; i<track_points->get_num(); i++) {
    track_points->get(i)->flags |= Hill::TRACK_POINT;
  }

  pan->add_hills(track_points);  
  redraw();
  
  return 0;
}

int
GipfelWidget::set_viewpoint(const char *pos) {
  int r;

  r = pan->set_viewpoint(pos);
  set_labels(pan->get_visible_mountains());

  return r;
}

void
GipfelWidget::set_viewpoint(const Hill *m) {
  pan->set_viewpoint(m);
  set_labels(pan->get_visible_mountains());
}

static void
draw_flag(int x, int y, char *s) {
  Fl_Color c = fl_color();

  fl_polygon(x, y - 10, x, y - 20, x + 10, y - 15);
  fl_yxline(x, y, y - 10);
  

  if (s) {
    fl_color(FL_WHITE);
    fl_draw(s, x , y - 12);
    fl_color(c);
  }	
}

void 
GipfelWidget::draw() {
  Hills *mnts;
  Hill *m;
  int center_x = w() / 2;
  int center_y = h() / 2;
  int i;
  
  if (img == NULL) {
    return;
  }

  fl_push_clip(x(), y(), w(), h());
  img->draw(x(),y(),w(),h(),0,0);
  
  /* hills */
  fl_font(FL_HELVETICA, 10);
  mnts = pan->get_visible_mountains();
  for (i=0; i<mnts->get_num(); i++) {
    m = mnts->get(i);

    if (m->flags & (Hill::DUPLICATE|Hill::TRACK_POINT)) {
      continue;
    }

    if (!show_hidden && (m->flags & Hill::HIDDEN)) {
      continue;
    }

    if (m == m1) {
      fl_color(FL_RED);
      draw_flag(center_x + m->x + x(), center_y + m->y + y(), "1");
    } else if (m == m2) {
      fl_color(FL_RED);
      draw_flag(center_x + m->x + x(), center_y + m->y + y(), "2");
    } else if (m->flags & Hill::HIDDEN) {
      fl_color(FL_BLUE);
    } else {
      fl_color(FL_BLACK);
    }
    
    fl_xyline(center_x + m->x + x() - CROSS_SIZE, center_y + m->y + y(), center_x + m->x + x() + CROSS_SIZE);
    fl_yxline(center_x + m->x + x(), center_y + m->y + m->label_y + y() - CROSS_SIZE, center_y + m->y + y() + CROSS_SIZE);

    fl_draw(m->name, 
	    center_x + m->x + x(), 
	    center_y + m->y + m->label_y + y());
  }

  /* markers */
  for (i=0; i<marker->get_num(); i++) {
    m = marker->get(i);

    fl_color(FL_GREEN);
    fl_xyline(center_x + m->x + x() - CROSS_SIZE * 2, center_y + m->y + y(), center_x + m->x + x() + CROSS_SIZE * 2);
    fl_yxline(center_x + m->x + x(), center_y + m->y + y() - CROSS_SIZE * 2, center_y + m->y + y() + CROSS_SIZE * 2);
    draw_flag(center_x + m->x + x(), center_y + m->y + y(), NULL);
  }

  /* track */
  if (track_points && track_points->get_num() > 0) {
    int last_x, last_y, last_initialized = 0;
    
    for (i=1; i<track_points->get_num(); i++) {
      if (!(track_points->get(i)->flags & Hill::VISIBLE)) {
        continue;
      }

      if (track_points->get(i)->flags & Hill::HIDDEN) {
        fl_color(FL_BLUE);
      } else {
        fl_color(FL_RED);
      }

      fl_line_style(FL_SOLID|FL_CAP_ROUND|FL_JOIN_ROUND,
                    get_rel_track_width(track_points->get(i)));
      if (last_initialized) {
        fl_begin_line();
        fl_vertex(center_x + x() + last_x, center_y + y() + last_y);
        fl_vertex(center_x + x() + track_points->get(i)->x, 
                  center_y + y() + track_points->get(i)->y);
        fl_end_line();
      }

      last_x = track_points->get(i)->x;
      last_y = track_points->get(i)->y;
      last_initialized++;
    }
    fl_line_style(0);
  }

  fl_pop_clip();
}


static int 
overlap(int m1, int n1, int m2, int n2) {
  return m1 <= n2 && n1 >= m2; 
}

void 
GipfelWidget::set_labels(Hills *v) {
  int i, j, width, height;
  Hill *m, *n;

  fl_font(FL_HELVETICA, 10);
  height = fl_height();

  for (i=0; i<v->get_num(); i++) {
    m = v->get(i);
    
    if (m->flags & (Hill::DUPLICATE|Hill::TRACK_POINT)) {
      continue;
    }

    if (!show_hidden && (m->flags & Hill::HIDDEN)) {
      continue;
    }

    width = (int) ceilf(fl_width(m->name));
    m->label_x = width;
    m->label_y = 0;
    for (j=0; j < i; j++) {
      n = v->get(j);
      
      if (n->flags & (Hill::DUPLICATE | Hill::TRACK_POINT)) {
	continue;
      }

      if (!show_hidden && (n->flags & Hill::HIDDEN)) {
        continue;
      }

      // Check for overlapping labels and
      // overlaps between labels and peak markers
      if ((overlap(m->x, m->x + m->label_x, n->x, n->x + n->label_x) &&
	   overlap(m->y + m->label_y - height, m->y + m->label_y, n->y + n->label_y - height, n->y + n->label_y)) ||
	  (overlap(m->x, m->x + m->label_x, n->x - 2, n->x + 2) &&
	   overlap(m->y + m->label_y - height, m->y + m->label_y, n->y - 2, n->y + 2))) {
	m->label_y = n->y + n->label_y - m->y - height - 1;
      }
    }
  }
}

int
GipfelWidget::set_cur_mountain(int m_x, int m_y) {
  Hills *mnts = pan->get_visible_mountains();
  Hill *m;
  int center_x = w() / 2;
  int center_y = h() / 2;
  int i;

  for (i=0; i<mnts->get_num(); i++) {
    m = mnts->get(i); 
    if (m->flags & (Hill::DUPLICATE | Hill::TRACK_POINT)) {
      continue;
    }

    if (m_x - center_x >= m->x - 2 && m_x - center_x < m->x + 2 &&
	m_y - center_y >= m->y - 2 && m_y - center_y < m->y + 2) {
      cur_mountain = m;
      if (m1 != NULL && m2 != NULL) {
	fprintf(stderr, "Resetting m1 and m2\n");
	m1 = NULL;
	m2 = NULL;
      }

      if (m1 == NULL) {
	m1 = cur_mountain;
	fprintf(stderr, "m1 = %s\n", m1->name);
      } else if (m2 == NULL) {
	m2 = cur_mountain;
	fprintf(stderr, "m2 = %s\n", m2->name);
      }

      redraw();
      return 0;
    }
  }

  for (i=0; i<marker->get_num(); i++) {
    m = marker->get(i);

    if (m_x - center_x >= m->x - 2 && m_x - center_x < m->x + 2 &&
	m_y - center_y >= m->y - 2 && m_y - center_y < m->y + 2) {
      cur_mountain = m;
      redraw();
      return 0;
    }
  }

  cur_mountain = NULL;
  redraw();
  return 1;
}

int
GipfelWidget::set_mountain(int m_x, int m_y) {
  int old_x, old_y, old_label_y;
  int center_x = w() / 2;
  int center_y = h() / 2;

  if (cur_mountain == NULL) {
    return 1;
  }

  old_x = cur_mountain->x;
  old_y = cur_mountain->y;
  old_label_y = cur_mountain->label_y;

  cur_mountain->x = m_x - center_x;
  cur_mountain->y = m_y - center_y;
  cur_mountain->label_y = 0;
 
  damage(4, center_x + x() + old_x - 2*CROSS_SIZE - 1,
            center_y + y() + old_y + old_label_y - 2*CROSS_SIZE - 20,
            MAX(20, cur_mountain->label_x) + 2*CROSS_SIZE + 2,
            MAX(20, old_label_y) + 22 ); 
  damage(4, center_x + x() + cur_mountain->x - 2*CROSS_SIZE - 1,
            center_y + y() + cur_mountain->y + cur_mountain->label_y - 2*CROSS_SIZE - 20,
            MAX(20, cur_mountain->label_x) + 2*CROSS_SIZE + 2,
            MAX(20, cur_mountain->label_y) + 22 ); 
 
  return 0;
}

void
GipfelWidget::set_center_angle(double a) {
  pan->set_center_angle(a);
  set_labels(pan->get_visible_mountains());
  redraw();
}

void
GipfelWidget::set_nick_angle(double a) {
  pan->set_nick_angle(a);
  set_labels(pan->get_visible_mountains());
  redraw();
}

void
GipfelWidget::set_tilt_angle(double a) {
  pan->set_tilt_angle(a);
  set_labels(pan->get_visible_mountains());
  redraw();
}

void
GipfelWidget::set_scale(double s) {
  pan->set_scale(s);
  set_labels(pan->get_visible_mountains());
  redraw();
}

void
GipfelWidget::set_projection(Projection::Projection_t p) {
  pan->set_projection(p);
  set_labels(pan->get_visible_mountains());
  redraw();
}

Projection::Projection_t
GipfelWidget::get_projection() {
  return pan->get_projection();
}

double
GipfelWidget::get_center_angle() {
  return pan->get_center_angle();
}

double
GipfelWidget::get_nick_angle() {
  return pan->get_nick_angle();
}

double
GipfelWidget::get_tilt_angle() {
  return pan->get_tilt_angle();
}

double
GipfelWidget::get_scale() {
  return pan->get_scale();
}

double
GipfelWidget::get_height_dist_ratio() {
  return pan->get_height_dist_ratio();
}

const char *
GipfelWidget::get_viewpoint() {
  return pan->get_viewpoint();
}

double
GipfelWidget::get_view_lat() {
  return pan->get_view_lat();
}

double
GipfelWidget::get_view_long() {
  return pan->get_view_long();
}

double
GipfelWidget::get_view_height() {
  return pan->get_view_height();
}

void 
GipfelWidget::center() {
  Hill *m = choose_hill(pan->get_close_mountains(), "Center Peak");
  if (m) {
    set_center_angle(m->alph / deg2rad);
    if (!m1 || (m1 && m2)) {
      m1 = m;
    } else {
      m2 = m;
    }
  }
}

static void
center_cb(Fl_Widget *o, void *f) {
  ((GipfelWidget*)f)->center();
}

void
GipfelWidget::set_height_dist_ratio(double r) {
  pan->set_height_dist_ratio(r);
  set_labels(pan->get_visible_mountains());
  
  redraw();
}

void
GipfelWidget::set_hide_value(double h) {
  pan->set_hide_value(h);
  set_labels(pan->get_visible_mountains());
  
  redraw();
}

void
GipfelWidget::set_show_hidden(int h) {
  show_hidden = h;
  set_labels(pan->get_visible_mountains());
  
  redraw();

}
void
GipfelWidget::set_view_lat(double v) {
  pan->set_view_lat(v);
  set_labels(pan->get_visible_mountains());
  redraw();
}

void
GipfelWidget::set_view_long(double v) {
  pan->set_view_long(v);
  set_labels(pan->get_visible_mountains());
  redraw();
}

void
GipfelWidget::set_view_height(double v) {
  pan->set_view_height(v);
  set_labels(pan->get_visible_mountains());
  redraw();
}

Hills*
GipfelWidget::get_mountains() {
  return pan->get_mountains();
}

int
GipfelWidget::comp_params() {
  if (m1 == NULL || m2 == NULL) {
    fprintf(stderr, "Position m1 and m2 first.\n");
    return 1;
  }
  fl_cursor(FL_CURSOR_WAIT);
  pan->comp_params(m1, m2);
  set_labels(pan->get_visible_mountains());
  redraw();
  fl_cursor(FL_CURSOR_DEFAULT);
}

int
GipfelWidget::guess() {
  if (m1 == NULL) {
    fprintf(stderr, "Position m1 first.\n");
    return 1;
  }
  fl_cursor(FL_CURSOR_WAIT);
  pan->guess(marker, m1);
  set_labels(pan->get_visible_mountains());
  redraw();
  fl_cursor(FL_CURSOR_DEFAULT);
}

int
GipfelWidget::update() {
  redraw();
  Fl::wait(1.0);
}

int
GipfelWidget::get_rel_track_width(Hill *m) {
  double dist = pan->get_real_distance(m);

  return MAX((pan->get_scale() * track_width) / (dist * 10.0), 1.0);
}

void
GipfelWidget::set_track_width(double w) {
  track_width = w;
  redraw();
}


int
GipfelWidget::handle(int event) {
  int mark_x, mark_y;

  switch(event) {
  case FL_PUSH:    
    if (Fl::event_button() == 1) {
         
      mark_x = Fl::event_x()-x();
      mark_y = Fl::event_y()-y();
      set_cur_mountain(mark_x, mark_y);

      Fl::focus(this);
      return 1;
    }
    break;
  case FL_DRAG:
    set_mountain(Fl::event_x()-x(), Fl::event_y()-y());
    return 1;
    break;
  case FL_FOCUS:
    return 1;
    break;
  case FL_UNFOCUS:
    return 0;
    break;
  }
  return 0;
}

int
GipfelWidget::get_pixel(double a_view, double a_nick,
                        char *r, char *g, char *b) {
	int px, py;


	if (img == NULL) {
		return 1;
	}

	pan->get_coordinates(a_view, a_nick, &px, &py);
	return DataImage::get_pixel(img, px, py, r, g, b);
}

