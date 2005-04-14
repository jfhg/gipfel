// 
// "$Id: GipfelWidget.cxx,v 1.7 2005/04/14 21:15:45 hofmann Exp $"
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
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_JPEG_Image.H>
#include <FL/fl_draw.H>
#include <FL/x.H>

#include "GipfelWidget.H"


GipfelWidget::GipfelWidget(int X,int Y,int W, int H): Fl_Widget(X, Y, W, H) {
  img = NULL;
  pan = new Panorama();
  cur_mountain = NULL;
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

  fl_font(FL_COURIER, 10);
  m = pan->get_visible_mountains();
  while (m) {
    if (m == cur_mountain) {
      fl_color(FL_RED);
    } else {
      fl_color(FL_BLACK);
    }
    
    fl_rectf(center_x + m->x + x() - 2, 
	     center_y + m->y + y() - 2,
	     4,
	     4);
      //    fl_line(center_x + m->x + x(), 0 + y(), center_x + m->x + x(), h() + y());
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
GipfelWidget::move_mountain(int m_x, int m_y) {
  int ret;
  int center = w() / 2;

  if (cur_mountain == NULL) {
    return 1;
  }

  ret = pan->move_mountain(cur_mountain, m_x - center, m_y - center);
  
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
    move_mountain(Fl::event_x()-x(), Fl::event_y()-y());
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
