// 
// "$Id: GipfelWidget.H,v 1.1 2005/04/13 15:36:36 hofmann Exp $"
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

#include <FL/Fl_Overlay_Window.H>

class GipfelWidget : public Fl_Overlay_Window {
 private:
  Fl_Image *img;

 public:
  GipfelWidget(int X,int Y,int W, int H);

  int load(const char *file);

  void draw_overlay();

  void draw();
};
#endif
