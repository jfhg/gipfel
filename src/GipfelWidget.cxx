// 
// "$Id: GipfelWidget.cxx,v 1.13 2005/04/30 08:54:35 hofmann Exp $"
//
// PSEditWidget routines.
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
  img = NULL;
  pan = new Panorama();
  cur_mountain = NULL;
  mb = NULL;
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
  return pan->load_file(file);
}

int
GipfelWidget::set_viewpoint(const char *pos) {
  return pan->set_viewpoint(pos);
}

void 
GipfelWidget::draw() {
  Mountain *m;
  int center_x = w() / 2;
  int center_y = h() / 2;
  
  if (img == NULL) {
    return;
  }

  fl_push_clip(x(), y(), w(), h());
  img->draw(x(),y(),w(),h(),0,0);

  fl_font(FL_HELVETICA, 8);
  m = pan->get_visible_mountains();
  while (m) {
    if (m == cur_mountain) {
      fl_color(FL_RED);
    } else {
      fl_color(FL_BLACK);
    }
    
    fl_xyline(center_x + m->x + x() - 2, center_y + m->y + y(), center_x + m->x + x() + 2);
    fl_yxline(center_x + m->x + x(), center_y + m->y + y() - 2, center_y + m->y + y() + 2);

    fl_draw(m->name, 
	    center_x + m->x + x(), 
	    center_y + m->y + y());
    m = m->get_next_visible();
  }

  fl_pop_clip();
}

int
GipfelWidget::set_cur_mountain(int m_x, int m_y) {
  Mountain *m = pan->get_visible_mountains();
  int center = w() / 2;

  while (m) {
    if (m_x - center >= m->x - 2 && m_x - center < m->x + 2) {
      cur_mountain = m;
      redraw();
      return 0;
    }

    m = m->get_next_visible();
  }

  cur_mountain = NULL;
  redraw();
  return 1;
}

int
GipfelWidget::set_mountain(int m_x, int m_y) {
  int ret;
  int center_x = w() / 2;
  int center_y = h() / 2;

  if (cur_mountain == NULL) {
    return 1;
  }

  ret = pan->set_mountain(cur_mountain, m_x - center_x, m_y - center_y);
  
  redraw();
  return ret;
}

void
GipfelWidget::set_center_angle(double a) {
  pan->set_center_angle(a);
  redraw();
}

void
GipfelWidget::set_nick_angle(double a) {
  pan->set_nick_angle(a);
  redraw();
}

void
GipfelWidget::set_tilt_angle(double a) {
  pan->set_tilt_angle(a);
  redraw();
}

void
GipfelWidget::set_scale(double s) {
  pan->set_scale(s);
  redraw();
}

void
GipfelWidget::set_height_dist_ratio(double r) {
  pan->set_height_dist_ratio(r);
  redraw();
}

int
GipfelWidget::comp_params() {
  pan->comp_params();
  redraw();
}


int
GipfelWidget::newton() {
  pan->newton();
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
      fprintf(stderr, "x %d, y %d\n", mark_x, mark_y);
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
