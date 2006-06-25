// 
// Copyright 2005 by Johannes Hofmann
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

#ifndef STITCH_H
#define STITCH_H

#include "GipfelWidget.H"
#include "DataImage.H"

#define MAX_PICS 256

class Stitch {
	private:
		GipfelWidget *gipf[MAX_PICS];
		

	public:
		Stitch();

		~Stitch();
		
		int load_image(char *file);
		
		int resample(DataImage *img,
			double view_start, double view_end);
};

#endif
