// 
// Stitch routines.
//
// Copyright 2006 by Johannes Hofmann
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Stitch.H"

Stitch::Stitch() {
	for (int i=0; i<MAX_PICS; i++) {
		gipf[i] = NULL;
	}
}

Stitch::~Stitch() {
	for (int i=0; i<MAX_PICS; i++) {
		if (gipf[i]) {
			delete(gipf[i]);
		} else {
			break;
		}
	}
}


int
Stitch::load_image(char *file) {
	for (int i=0; i<MAX_PICS; i++) {
		if (gipf[i] == NULL) {
			gipf[i] = new GipfelWidget(0, 0, 800, 600);
			gipf[i]->load_image(file);
			break;
		}
	}


}

int
Stitch::resample(DataImage *img,
            double view_start, double view_end) {
	double step_view = (view_end - view_start) / img->w();
	char r, g, b;
	int y_off = img->h() / 2;

	for (int x=0; x<img->w(); x++) {
		for (int y=0; y<img->h(); y++) {
			double a_view, a_nick;

			a_view = x * step_view;
			a_nick = (y_off - y) * step_view;

			for (int i=0; i<MAX_PICS; i++) {
				if (gipf[i] == NULL) {
					break;
				} else if (gipf[i]->get_pixel(a_view, a_nick, &r, &g, &b)==0) {
					img->set_pixel(x, y, r, g, b);
					break;
				}
			}
		}
	}
}
