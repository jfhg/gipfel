//
// Copyright 2006 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>


#include "util.h"

#include "ImageMetaData.H"

ImageMetaData::ImageMetaData() {
	longitude = NAN;
	latitude = NAN;
	height = 0.0;
	direction = 0.0;
	nick = 0.0;
	tilt = 0.0;
	k0 = 0.0;
	k1 = 0.0;
	u0 = 0.0;
	v0 = 0.0;
	focal_length_35mm = 35.0;
	scale = NAN;
	projection_type = 0;
}


int
ImageMetaData::load_image(char *name, int img_width) {
	int ret;

	ret = load_image_jpgcom(name);
	if (ret == 2) { // old format
		focal_length_35mm = scale * 35.0 / (double) img_width;
	} else if (ret == 1) { // get reasonable defaults from exif data
		ret = load_image_exif(name);
	}

	return ret;
}

int
ImageMetaData::save_image(char *in_img, char *out_img) {
	return save_image_jpgcom(in_img, out_img);
}

static double
degminsecstr2double(char *val) {
    double ret, dv;

    ret = 0.0;
    for (dv=1.0; dv <= 3600.0; dv = dv * 60.0) {
        ret = ret + atof(val) / dv;
        val = strchr(val, ',');
        if (!val || val[1] == '\0') {
            break;
        } else {
            val++;
        }
    }

    return ret;
}

#define EXIF_FOCAL_LENGTH_IN_35MM_FILM 0xa405
#define EXIF_GPS_LATIITUDE             0x0002
#define EXIF_GPS_LONGITUDE             0x0004
#define EXIF_GPS_ALTITUDE              0x0006

int
ImageMetaData::load_image_exif(char *name) {
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
            if (sscanf(buf, "%x\t%[^\n]\n", &id, val) != 2) {
                continue;
            }

            switch(id) {
                case EXIF_FOCAL_LENGTH_IN_35MM_FILM:
                    focal_length_35mm = atof(val);
                    break;
                case EXIF_GPS_LONGITUDE:
                    longitude = degminsecstr2double(val);
                    break;
                case EXIF_GPS_LATIITUDE:
                    latitude = degminsecstr2double(val);
                    break;
                case EXIF_GPS_ALTITUDE:
                    height = atof(val);
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


#define GIPFEL_FORMAT_1 "gipfel: longitude %lf, latitude %lf, height %lf, direction %lf, nick %lf, tilt %lf, scale %lf, projection type %d"
#define GIPFEL_FORMAT_2 "gipfel: longitude %lf, latitude %lf, height %lf, direction %lf, nick %lf, tilt %lf, focal_length_35mm %lf, projection type %d, k0 %lf, k1 %lf, u0 %lf, v0 %lf"

int
ImageMetaData::load_image_jpgcom(char *name) {
    char * args[32];
    FILE *p;
    pid_t pid;
    int status;
    char buf[1024];
    double lo, la, he, dir, ni, ti, fr, _k0, _k1, _u0, _v0;
    int pt;
    int n, ret = 1;

    args[0] = "rdjpgcom";
    args[1] = name;
    args[2] = NULL;

    p = pexecvp(args[0], args, &pid, "r");

    if (p) {
        while (fgets(buf, sizeof(buf), p) != NULL) {
            if ((n = sscanf(buf, GIPFEL_FORMAT_2,
                    &lo, &la, &he, &dir, &ni, &ti, &fr, &pt, &_k0, &_k1, &_u0, &_v0)) >= 7) {

                longitude = lo;
                latitude  = la;
                height    = he;
                direction = dir;
                nick      = ni;
                tilt      = ti;
                focal_length_35mm = fr;
                projection_type = pt;

				if (n == 11) {
					k0 = _k0;
					k1 = _k1;
					u0 = _u0;
					v0 = _v0;
				}

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
                scale     = fr;
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
ImageMetaData::save_image_jpgcom(char *in_img, char *out_img) {
    char * args[32];
    FILE *p, *out;
    pid_t pid;
    char buf[1024];
    int status;
    size_t n;
    struct stat in_stat, out_stat;

    if (stat(in_img, &in_stat) != 0) {
        perror("stat");
        return 1;
    }

    if (stat(out_img, &out_stat) == 0) {
        if (in_stat.st_ino == out_stat.st_ino) {
            fprintf(stderr, "Input image %s and output image %s are the same file\n",
                in_img, out_img);
            return 1;
        }
    }

    out = fopen(out_img, "w");
    if (out == NULL) {
        perror("fopen");
        return 1;
    }

    snprintf(buf, sizeof(buf), GIPFEL_FORMAT_2,
        longitude,
        latitude,
        height,
        direction,
        nick,
        tilt,
        focal_length_35mm,
        projection_type,
		k0, k1, u0, v0);

    // try to save gipfel data in JPEG comment section
    args[0] = "wrjpgcom";
    args[1] = "-replace";
    args[2] = "-comment";
    args[3] = buf;
    args[4] = in_img;
    args[5] = NULL;

    p = pexecvp(args[0], args, &pid, "r");

    if (p) {
        while ((n = fread(buf, 1, sizeof(buf), p)) != 0) {
            if (fwrite(buf, 1, n, out) != n) {
                perror("fwrite");
                fclose(out);
                fclose(p);
                waitpid(pid, &status, 0);
            }
        }
        fclose(p);
        waitpid(pid, &status, 0);
        if (WEXITSTATUS(status) == 127 || WEXITSTATUS(status) == 126) {
            fprintf(stderr, "%s not found\n", args[0]);
        }
    }

    fclose(out);
    return 0;
}


double
ImageMetaData::get_longitude() {
	return longitude;
}

double
ImageMetaData::get_latitude() {
	return latitude;
}

double
ImageMetaData::get_height() {
	return height;
}

double
ImageMetaData::get_direction() {
	return direction;
}

double
ImageMetaData::get_nick() {
	return nick;
}

double
ImageMetaData::get_tilt() {
	return tilt;
}

double
ImageMetaData::get_focal_length_35mm() {
	return focal_length_35mm;
}

int
ImageMetaData::get_projection_type() {
	return projection_type;
}

void
ImageMetaData::set_longitude(double v) {
	longitude = v;
}

void
ImageMetaData::set_latitude(double v) {
	latitude = v;
}

void
ImageMetaData::set_height(double v) {
	height = v;
}

void
ImageMetaData::set_direction(double v) {
	direction = v;
}

void
ImageMetaData::set_nick(double v) {
	nick = v;
}

void
ImageMetaData::set_tilt(double v) {
	tilt = v;
}

void
ImageMetaData::set_focal_length_35mm(double v) {
	focal_length_35mm = v;
}

void
ImageMetaData::set_projection_type(int v) {
	projection_type = v;
}

void
ImageMetaData::get_distortion_params(double *_k0, double *_k1,
	double *_u0, double *_v0) {
	*_k0 = k0;	
	*_k1 = k1;	
	*_u0 = u0;	
	*_v0 = v0;	
}

void
ImageMetaData::set_distortion_params(double _k0, double _k1,
	double _u0, double _v0) {
	k0 = _k0;	
	k1 = _k1;	
	u0 = _u0;	
	v0 = _v0;	
}
