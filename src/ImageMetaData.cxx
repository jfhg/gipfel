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
	_manufacturer = NULL;
    _model = NULL;
	clear();
}

ImageMetaData::~ImageMetaData() {
	if (_manufacturer) free(_manufacturer);
	if (_model) free(_model);
}

void
ImageMetaData::clear() {
	if (_manufacturer) free(_manufacturer);
	_manufacturer = NULL;
	if (_model) free(_model);
    _model = NULL;
	_longitude = NAN;
	_latitude = NAN;
	_height = NAN;
	_direction = NAN;
	_nick = NAN;
	_tilt = NAN;
	_k0 = NAN;
	_k1 = NAN;
	_x0 = NAN;
	_focal_length = NAN;
	_focal_length_35mm = NAN;
	_scale = NAN;
	_projection_type = 0;
}

int
ImageMetaData::load_image(char *name) {
	clear();

	load_image_jpgcom(name);
	load_image_exif(name); // fill missing values from exif data

	return 0;
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

#define EXIF_MANUFACTURER              0x010f
#define EXIF_MODEL                     0x0110
#define EXIF_FOCAL_LENGTH              0x920a
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
                case EXIF_MANUFACTURER:
                    if (!_manufacturer) _manufacturer = strdup(val);
                    break;
                case EXIF_MODEL:
                    if (!_model) _model = strdup(val);
                    break;
                case EXIF_FOCAL_LENGTH:
                    if (isnan(_focal_length)) _focal_length = atof(val);
                    break;
                case EXIF_FOCAL_LENGTH_IN_35MM_FILM:
                    if (isnan(_focal_length_35mm)) _focal_length_35mm = atof(val);
                    break;
                case EXIF_GPS_LONGITUDE:
                    if (isnan(_longitude)) _longitude = degminsecstr2double(val);
                    break;
                case EXIF_GPS_LATIITUDE:
                    if (isnan(_latitude)) _latitude = degminsecstr2double(val);
                    break;
                case EXIF_GPS_ALTITUDE:
                    if (isnan(_height)) _height = atof(val);
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


#define GIPFEL_FORMAT_2 "gipfel: longitude %lf, latitude %lf, height %lf, direction %lf, nick %lf, tilt %lf, focal_length_35mm %lf, projection type %d, k0 %lf, k1 %lf, x0 %lf"

int
ImageMetaData::load_image_jpgcom(char *name) {
    char * args[32];
    FILE *p;
    pid_t pid;
    int status;
    char buf[1024];
    double lo, la, he, dir, ni, ti, fr, k0, k1, x0 = 0.0;
    int pt = 0;
    int n, ret = 1;

    args[0] = "rdjpgcom";
    args[1] = name;
    args[2] = NULL;

    p = pexecvp(args[0], args, &pid, "r");

    if (p) {
        while (fgets(buf, sizeof(buf), p) != NULL) {
            if ((n = sscanf(buf, GIPFEL_FORMAT_2,
                    &lo, &la, &he, &dir, &ni, &ti, &fr, &pt, &k0, &k1, &x0)) >= 8) {

                _longitude = lo;
                _latitude  = la;
                _height    = he;
                _direction = dir;
                _nick      = ni;
                _tilt      = ti;
                _focal_length_35mm = fr;
                _projection_type = pt;

				if (n >= 10) {
					_k0 = k0;
					_k1 = k1;
					_x0 = x0;
				}

                ret = 0;

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
        _longitude,
        _latitude,
        _height,
        _direction,
        _nick,
        _tilt,
        _focal_length_35mm,
        _projection_type,
		_k0, _k1, _x0);

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

void
ImageMetaData::distortion_params(double *k0, double *k1, double *x0) {
	*k0 = _k0;	
	*k1 = _k1;	
	*x0 = _x0;	
}

void
ImageMetaData::distortion_params(double k0, double k1, double x0) {
	_k0 = k0;	
	_k1 = k1;	
	_x0 = x0;	
}
