//
// Copyright 2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#ifndef SCREENDUMP_H
#define SCREENDUMP_H

#include <FL/Fl_Widget.H>

#include "OutputImage.H"

class ScreenDump {
	private:
		int w, h;
		unsigned char * rgb;

	public:
		ScreenDump(Fl_Widget *widget);
		~ScreenDump();

		int save(OutputImage *out);
};

#endif
