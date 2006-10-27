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
#include <sys/wait.h>

#include "util.h"

#include "JpgcomImageMetaData.H"

#define GIPFEL_FORMAT_1 "gipfel: longitude %lf, latitude %lf, height %lf, direction %lf, nick %lf, tilt %lf, scale %lf, projection type %d"
#define GIPFEL_FORMAT_2 "gipfel: longitude %lf, latitude %lf, height %lf, direction %lf, nick %lf, tilt %lf, focallength_sensor_ratio %lf, projection type %d"

int
JpgcomImageMetaData::load_image(char *name) {
	char * args[32];
	FILE *p;
	pid_t pid;
	int status;
	char buf[1024];
	double lo, la, he, dir, ni, ti, fr;
	int pt;
	int ret = 1;

	args[0] = "rdjpgcom";
	args[1] = name;
	args[2] = NULL;
  
	p = pexecvp(args[0], args, &pid, "r");

	if (p) {
		while (fgets(buf, sizeof(buf), p) != NULL) {
			if (sscanf(buf, GIPFEL_FORMAT_2,
					&lo, &la, &he, &dir, &ni, &ti, &fr, &pt) >= 7) {

				longitude = lo;
				latitude  = la;
				height    = he;
				direction = dir;
				nick      = ni;
				tilt      = ti;
				focallength_sensor_ratio = fr;
				projection_type = pt;	

				ret = 0;

				break;
			} else if (sscanf(buf, GIPFEL_FORMAT_1,
					&lo, &la, &he, &dir, &ni, &ti, &fr, &pt) >= 7) {

				longitude = lo;
				latitude  = la;
				height    = he;
				direction = dir;
				nick      = ni;
				tilt      = ti;
				focallength_sensor_ratio = fr;
				projection_type = pt;	

				ret = 2; // special return value for compatibility with
				         // old format

				break;
			}
		}

		fclose(p);
		waitpid(pid, &status, 0); 
		if (WEXITSTATUS(status) == 127 || WEXITSTATUS(status) == 126) {
			fprintf(stderr, "%s not found\n", args[0]);
		}
	}

	return ret;
}

int
JpgcomImageMetaData::save_image(char *name) {
} 
