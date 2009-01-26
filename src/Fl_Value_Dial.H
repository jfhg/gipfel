//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef Fl_Value_Dial_H
#define Fl_Value_Dial_H

#include <FL/Fl_Dial.H>

class Fl_Value_Dial : public Fl_Dial {
	private:
		uchar textfont_, textsize_;
		unsigned textcolor_;
	public:
		void draw();
		int handle(int event);

		Fl_Value_Dial(int x,int y,int w,int h, const char *l = 0);
		Fl_Font textfont() const {return (Fl_Font)textfont_;}
		void textfont(uchar s) {textfont_ = s;}
		uchar textsize() const {return textsize_;}
		void textsize(uchar s) {textsize_ = s;}
		Fl_Color textcolor() const {return (Fl_Color)textcolor_;}
		void textcolor(unsigned s) {textcolor_ = s;}
};

#endif
