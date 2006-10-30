
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
