// 
// "$Id: GipfelWidget.cxx,v 1.3 2005/04/13 19:09:19 hofmann Exp $"
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
  int center = w() / 2;
  
  if (img == NULL) {
    return;
  }

  fl_push_clip(x(), y(), w(), h());
  img->draw(x(),y(),w(),h(),0,0);


  fl_color(FL_RED);
  fl_font(FL_COURIER, 10);
  m = pan->get_visible_mountains();
  while (m) {
    int m_x = pan->get_x(m);

    fl_line(center + m_x + x(), 0 + y(), center + m_x + x(), h() + y());
    fl_draw(m->name, m_x + x(), 20 + y() + m_x / 4);
    m = m->get_next();
  }

  fl_pop_clip();
}


