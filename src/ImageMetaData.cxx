//
// Copyright 2006-2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
//
// This software may be used and distributed according to the terms
// of the GNU General Public License, incorporated herein by reference.

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <libgen.h>
#include <assert.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <exiv2/image.hpp>
#include <exiv2/exif.hpp>

#include "../config.h"
#include "ImageMetaData.H"

#if !defined(O_BINARY)
#define O_BINARY 0
#endif

ImageMetaData::ImageMetaData() {
	_manufacturer = NULL;
    _model = NULL;
	clear();
}

ImageMetaData::~ImageMetaData() {
	if (_manufacturer)
		free(_manufacturer);
	if (_model)
		free(_model);
}

void
ImageMetaData::clear() {
	if (_manufacturer)
		free(_manufacturer);
	_manufacturer = NULL;
	if (_model)
		free(_model);
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

static void
exifSetValue(double *destVal, Exiv2::ExifData *exifData, const char *name) {
	Exiv2::ExifKey key(name);
	Exiv2::ExifData::iterator pos = exifData->findKey(key);
	pos = exifData->findKey(key);
	if (pos != exifData->end() && pos->toFloat() >= 0)
		*destVal = pos->toFloat();
}

static void
exifSetCoordinate(double *destVal, Exiv2::ExifData *exifData, const char *name) {
	Exiv2::ExifKey key(name);
	Exiv2::ExifData::iterator pos = exifData->findKey(key);

	if (pos != exifData->end()) {
		if (pos->toFloat() >= 0)
			*destVal = pos->toFloat();
		if (pos->toFloat(1) >= 0)
			*destVal += pos->toFloat(1) / 60;
		if (pos->toFloat(2) >= 0)
			*destVal += pos->toFloat(2) / 3600;
	}
}

int
ImageMetaData::load_image_exif(char *name) {
	Exiv2::Image::AutoPtr image;

	try {
		image = Exiv2::ImageFactory::open(name);
		image->readMetadata();
	} catch (Exiv2::Error error) {
		fprintf(stderr, "Error reading metadata\n");
		return 1;
	}

    Exiv2::ExifData &exifData = image->exifData();
    if (exifData.empty()) {
		fprintf(stderr, "%s: No Exif data found in the file", name);
		return 1;
	}

    Exiv2::ExifData::iterator pos = exifData.end();

    if (!_manufacturer ) {
		Exiv2::ExifKey key("Exif.Image.Make"); // tag auch wirklich vorhanden?
		pos = exifData.findKey(key);
		if (pos != exifData.end() && pos->size())
			_manufacturer = strdup(pos->toString().c_str());
	}

    if (!_model) {
		Exiv2::ExifKey key("Exif.Image.Model");
		pos = exifData.findKey(key);
		if (pos != exifData.end() && pos->size() )
			_model = strdup(pos->toString().c_str());
	}

    if (isnan(_focal_length))
		exifSetValue(&_focal_length, &exifData, "Exif.Photo.FocalLength");

    if (isnan(_focal_length_35mm))
		exifSetValue(&_focal_length_35mm, &exifData, "Exif.Photo.FocalLengthIn35mmFilm");

    if (isnan(_longitude))
		exifSetCoordinate(&_longitude, &exifData, "Exif.GPSInfo.GPSLongitude");

    if (isnan(_latitude))
		exifSetCoordinate(&_latitude, &exifData, "Exif.GPSInfo.GPSLatitude");

    if (isnan(_height))
		exifSetValue(&_height, &exifData, "Exif.GPSInfo.GPSAltitude");

    return 0;
}


#define GIPFEL_FORMAT "gipfel: longitude " FMT_DOUBLE ", latitude " FMT_DOUBLE ", height " FMT_DOUBLE ", direction " FMT_DOUBLE ", nick " FMT_DOUBLE ", tilt " FMT_DOUBLE ", focal_length_35mm " FMT_DOUBLE ", projection type %d, k0 " FMT_DOUBLE ", k1 " FMT_DOUBLE ", x0 " FMT_DOUBLE ""

#define FMT_DOUBLE "%lf"
static const char *gipfel_format_scan = GIPFEL_FORMAT;

#undef FMT_DOUBLE
#define FMT_DOUBLE "%f"
static const char *gipfel_format_prnt = GIPFEL_FORMAT;

#undef FMT_DOUBLE
#undef GIPFEL_FORMAT

int
ImageMetaData::load_image_jpgcom(char *name) {
    double lo, la, he, dir, ni, ti, fr, k0, k1, x0 = 0.0;
    int pt = 0;
    int n, ret = 1;
	Exiv2::Image::AutoPtr image;

	try {
		image = Exiv2::ImageFactory::open(name);
		image->readMetadata();
	} catch (Exiv2::Error error) {
		fprintf(stderr, "Error reading metadata\n");
		return 1;
	}

    const char *com = image->comment().c_str();

    if ((n = sscanf(com, gipfel_format_scan,
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
	}

    return ret;
}

int
ImageMetaData::save_image_jpgcom(char *in_img, char *out_img) {
    char buf[1024], *tmpname;
    int n, in_fd, tmp_fd, err = 0;

    char* dirbuf = strdup(out_img);
#if HAVE_MKSTEMP
	char tmpbuf[MAXPATHLEN];
	snprintf(tmpbuf, sizeof(tmpbuf), "%s/.gipfelXXXXXX", dirname(dirbuf));
	tmp_fd = mkstemp(tmpbuf);
	tmpname = tmpbuf;
#else
	tmpname = tempnam(dirname(dirbuf), ".gipfel");
	tmp_fd = open(tmpname, O_WRONLY|O_TRUNC|O_CREAT|O_BINARY, S_IRUSR|S_IWUSR);
#endif
	free(dirbuf);

	if (tmp_fd == -1) {
		perror("mkstemp");
		return 1;
	}

	in_fd = open(in_img, O_RDONLY|O_BINARY);
	if (in_fd == -1) {
		perror("open");
		unlink(tmpname);
		close(tmp_fd);
		return 1;
	}

	while ((n = read(in_fd, buf, sizeof(buf))) > 0) {
		if (n < 0 || write(tmp_fd, buf, n) != n) {
			perror("write");
			err++;
			break;
		}
	}

	close(in_fd);

    Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(tmpname);
    if (!image.get())
		err++;

    image->readMetadata();
    image->clearComment();

    snprintf(buf, sizeof(buf), gipfel_format_prnt,
        _longitude,
        _latitude,
        _height,
        _direction,
        _nick,
        _tilt,
        _focal_length_35mm,
        _projection_type,
        _k0, _k1, _x0);

    image->setComment(buf);

	try {
		image->writeMetadata();
	} catch (Exiv2::Error error) {
		fprintf(stderr, "Error writing metadata\n");
		err++;
	}

#ifdef HAVE_FSYNC
	fsync(tmp_fd); // make sure data is on disk before replacing orig file
#endif

	close(tmp_fd);

	if (err == 0) { // only overwrite existing image if everything was ok
#ifdef WIN32
		// Workaround as Windows does not seem to replace files on rename()
		struct stat stFileInfo;
		if (stat(out_img, &stFileInfo) == 0)
			unlink(out_img);
#endif

		if (rename(tmpname, out_img) != 0) {
			perror("rename");
			err++;
			unlink(tmpname);
		}
	}

    return (err != 0);
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
