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

#include "ExifImageMetaData.H"

#define FOCAL_LENGTH_IN_35MM_FILM 0xa405

int
ExifImageMetaData::load_image(char *name) {
	char * args[32];
	FILE *p;
	pid_t pid;
	int status;
	char buf[1024];
	char val[1024];
	int id;

	args[0] = "exif";
	args[1] = "-i";
	args[2] = "-m";
	args[3] = name;
	args[4] = NULL;
  
	p = pexecvp(args[0], args, &pid, "r");

	if (p) {
		while (fgets(buf, sizeof(buf), p) != NULL) {
			if (sscanf(buf, "%x\t%s", &id, val) != 2) {
				continue;
			}

			switch(id) {
				case FOCAL_LENGTH_IN_35MM_FILM:
					focallength_sensor_ratio = atof(val) / 35.0;
					break;
			}	
		}
	}

	fclose(p);
	waitpid(pid, &status, 0); 
	if (WEXITSTATUS(status) == 127 || WEXITSTATUS(status) == 126) {
		fprintf(stderr, "%s not found\n", args[0]);
	}

	return 0;
}


int
ExifImageMetaData::save_image(char *name) {
}

