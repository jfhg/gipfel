#!/bin/sh
# Generate golden reference data from the original C++ implementation.
# Requires: the gipfel C++ sources built (objects in $SRC), g++, GSL, maxima.
set -e
cd "$(dirname "$0")"

SRC=${GIPFEL_SRC:-$HOME/gipfel/src}
DATA=${GIPFEL_DATA:-$HOME/gipfel/gipfel.dat}
BUILD=harness/build
mkdir -p $BUILD golden

# rename class-scoped generated functions to free functions
sed 's/ProjectionRectilinear::/rect_/g' $SRC/ProjectionRectilinear_funcs.cxx > $BUILD/rect_funcs.cxx
sed 's/ProjectionCylindrical::/cyl_/g'  $SRC/ProjectionCylindrical_funcs.cxx > $BUILD/cyl_funcs.cxx

g++ -O2 -o $BUILD/funcsdump harness/funcsdump.cxx -I$BUILD -lm
$BUILD/funcsdump > golden/funcs.txt

g++ -O2 -o $BUILD/fitdump harness/fitdump.cxx \
	-I$SRC $SRC/Panorama.o $SRC/Hill.o $SRC/ProjectionLSQ.o \
	$SRC/ProjectionRectilinear.o $SRC/ProjectionCylindrical.o \
	-lm -lgsl -lgslcblas

$BUILD/fitdump "$DATA" 0 > golden/fit_rect.txt
$BUILD/fitdump "$DATA" 1 > golden/fit_cyl.txt

g++ -O2 -o $BUILD/exportdump harness/exportdump.cxx \
	-I$SRC $SRC/Panorama.o $SRC/Hill.o $SRC/ProjectionLSQ.o \
	$SRC/ProjectionRectilinear.o $SRC/ProjectionCylindrical.o $SRC/ImageMetaData.o \
	-lexiv2 -ltiff -lgsl -lgslcblas -lm

$BUILD/exportdump golden/test.jpg "$DATA" 2>/dev/null > golden/export.txt

g++ -O2 -o $BUILD/stitchdump harness/stitchdump.cxx \
	-I$SRC $SRC/Panorama.o $SRC/Hill.o $SRC/ScanImage.o $SRC/ProjectionLSQ.o \
	$SRC/ProjectionRectilinear.o $SRC/ProjectionCylindrical.o \
	-lfltk -lgsl -lgslcblas -lm

# stitch: cylindrical resample of one image (projtype center pitch roll scale
# k0 k1 x0 out_w out_h from to bicubic)
$BUILD/stitchdump golden/stitch.ppm 0 20 2 0 200 0 0 0 64 48 10 30 0 > golden/stitch_rect_near.txt
$BUILD/stitchdump golden/stitch.ppm 0 20 2 0 200 0 0 0 64 48 10 30 1 > golden/stitch_rect_bic.txt
$BUILD/stitchdump golden/stitch.ppm 1 20 2 0 200 0 0 0 64 48 10 30 1 > golden/stitch_cyl_bic.txt
$BUILD/stitchdump golden/stitch.ppm 0 20 2 3 200 -0.3 0.1 0 64 48 10 30 1 > golden/stitch_dist_bic.txt

echo "golden written:"
wc -l golden/*.txt
