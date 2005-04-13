// 
// "$Id: GipfelWidget.H,v 1.3 2005/04/13 19:09:19 hofmann Exp $"
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

#ifndef GipfelWidget_H
#define GipfelWidget_H

#include <FL/Fl_Widget.H>
#include "Panorama.H"

class GipfelWidget : public Fl_Widget {
 private:
  Fl_Image *img;
  Panorama *pan;

 public:
  GipfelWidget(int X,int Y,int W, int H);

  int load_image(const char *file);

  int load_data(const char *file);

  int set_viewpoint(const char *pos);

  void draw();
};
#endif
