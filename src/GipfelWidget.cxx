// 
// "$Id: GipfelWidget.cxx,v 1.31 2005/05/17 09:20:38 hofmann Exp $"
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
#include <fcntl.h>
#include <errno.h>

#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_JPEG_Image.H>
#include <FL/fl_draw.H>
#include <FL/x.H>

#include "GipfelWidget.H"

static Fl_Menu_Item menuitems[] = {
  { "&File",              0, 0, 0, FL_SUBMENU },
    { "&Open File...",    FL_CTRL + 'o', NULL},
  {0},
  { 0 }
};

GipfelWidget::GipfelWidget(int X,int Y,int W, int H): Fl_Widget(X, Y, W, H) {
  int i;

  img = NULL;
  pan = new Panorama();
  cur_mountain = NULL;
  mb = NULL;
  marker = new Hills();
  m1 = NULL;
  m2 = NULL;
  for (i=0; i<=3; i++) {
    marker->add(new Hill(i * 10, 0));
  }
  fl_register_images();
}

int
GipfelWidget::load_image(const char *file) {
  img = new Fl_JPEG_Image(file);
  
  if (img == NULL) {
    return 1;
  }
  
  w(img->w());
  h(img->h());

  mb = new Fl_Menu_Button(x(),y(),w()+x(),h()+y(),"&popup");
  mb->type(Fl_Menu_Button::POPUP3);
  mb->box(FL_NO_BOX);
  mb->menu(menuitems);

  return 0;
}

int
GipfelWidget::load_data(const char *file) {
  int r;
  r = pan->load_file(file);
  return r;
}

int
GipfelWidget::set_viewpoint(const char *pos) {
  int r;

  r = pan->set_viewpoint(pos);
  set_labels(pan->get_visible_mountains());
  return r;
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

  fl_font(FL_HELVETICA, 8);
  mnts = pan->get_visible_mountains();
  for (i=0; i<mnts->get_num(); i++) {
    m = mnts->get(i);

    if (m == m1) {
      fl_color(FL_RED);
      draw_flag(center_x + m->x + x(), center_y + m->y + y(), "1");
    } else if (m == m2) {
      fl_color(FL_RED);
      draw_flag(center_x + m->x + x(), center_y + m->y + y(), "2");
    } else {
      fl_color(FL_BLACK);
    }
    
    fl_xyline(center_x + m->x + x() - 2, center_y + m->y + y(), center_x + m->x + x() + 2);
    fl_yxline(center_x + m->x + x(), center_y + m->label_y + y() - 2, center_y + m->y + y() + 2);

    fl_draw(m->name, 
	    center_x + m->x + x(), 
	    center_y + m->label_y + y());
  }

  for (i=0; i<marker->get_num(); i++) {
    m = marker->get(i);

    fl_color(FL_GREEN);
    fl_xyline(center_x + m->x + x() - 3, center_y + m->y + y(), center_x + m->x + x() + 3);
    fl_yxline(center_x + m->x + x(), center_y + m->y + y() - 3, center_y + m->y + y() + 3);
    draw_flag(center_x + m->x + x(), center_y + m->y + y(), NULL);
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

  fl_font(FL_HELVETICA, 8);

  for (i=0; i<v->get_num(); i++) {
    m = v->get(i);
    
    fl_measure(m->name, width, height);
    m->label_x = m->x + width;
    m->label_y = m->y;

    for (j=0; j<v->get_num() && j < i; j++) {
      n = v->get(j);
      
      // Check for overlapping labels and
      // overlaps between labels and peak markers
      if ((overlap(m->x, m->label_x, n->x, n->label_x) &&
	   overlap(m->label_y - height, m->label_y, n->label_y - height, n->label_y)) ||
	  (overlap(m->x, m->label_x, n->x - 2, n->x + 2) &&
	   overlap(m->label_y - height, m->label_y, n->y - 2, n->y + 2))) {
	m->label_y = n->label_y - height - 1;
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
  int center_x = w() / 2;
  int center_y = h() / 2;

  if (cur_mountain == NULL) {
    return 1;
  }

  cur_mountain->x = m_x - center_x;
  cur_mountain->y = m_y - center_y;
  cur_mountain->label_y = cur_mountain->y;
  
  redraw();
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

void
GipfelWidget::set_height_dist_ratio(double r) {
  pan->set_height_dist_ratio(r);
  set_labels(pan->get_visible_mountains());
  redraw();
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
