//
// "$Id: Fl_Value_Dial.cxx,v 1.2 2005/05/18 11:34:30 hofmann Exp $"
//
// Value dial widget for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2004 by Bill Spitzak and others.
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
// Please report all bugs and problems to "fltk-bugs@fltk.org".
//

#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <math.h>
#include "Fl_Value_Dial.H"

Fl_Value_Dial::Fl_Value_Dial(int X, int Y, int W, int H, const char*l)
: Fl_Dial(X,Y,W,H,l) {
  step(1,100);
  textfont_ = FL_HELVETICA;
  textsize_ = 10;
  textcolor_ = FL_BLACK;
}

void Fl_Value_Dial::draw() {
  int sxx = x(), syy = y(), sww = w(), shh = h();
  int bxx = x(), byy = y(), bww = w(), bhh = h();

  if (damage()&FL_DAMAGE_ALL) draw_box(box(),sxx,syy,sww,shh,color());
  Fl_Dial::draw(sxx+Fl::box_dx(box()),
		  syy+Fl::box_dy(box()),
		  sww-Fl::box_dw(box()),
		  shh-Fl::box_dh(box()));

  char buf[128];
  format(buf);
  fl_font(textfont(), textsize());

  fl_color(active_r() ? textcolor() : fl_inactive(textcolor()));
  fl_draw(buf, bxx, byy + fl_height() - 2, bww, bhh, FL_ALIGN_TOP);
}

int Fl_Value_Dial::handle(int event) {  
  switch (event) {
  case FL_KEYBOARD :
    switch (Fl::event_key()) {
    case FL_Left:
      handle_drag(clamp(increment(value(),-1)));
      handle_release();
      return 1;
    case FL_Right:
      handle_drag(clamp(increment(value(),1)));
      handle_release();
      return 1;
    default:
      return 0;
    }
    break; 
  case FL_FOCUS :
  case FL_UNFOCUS :
    if (Fl::visible_focus()) {
      redraw();
      return 1;
    } else return 0;
  case FL_ENTER :
  case FL_LEAVE :
    return 1;
  default:
    return Fl_Dial::handle(event);
  }
}
