# gipfelweb: static, CSS-only web galleries of gipfel-annotated
# images. Port of the historical examples/ pipeline to the Rust
# stack: gipfel-cli replaces the C binary, ImageMagick replaces
# netpbm + jhead (convert preserves the gipfel comment and EXIF).
#
# usage: put annotated *.jpg files (optionally per-image %.opts with
# extra gipfel-cli arguments and %.title with a display title) in a
# directory and run
#
#   make -f /path/to/gipfel/examples/Makefile gipfelweb
#
# variables:
#   GIPFEL_CLI  path to the gipfel-cli binary
#   DATA        hill database used for the peak view
#   GIPFEL2HTML html generator script
#   V           visibility threshold for the peak view (lower shows
#               more, distant peaks; C default was 0.07)

here        := $(dir $(lastword $(MAKEFILE_LIST)))
GIPFEL_CLI  ?= $(abspath $(here)../target/release/gipfel-cli)
DATA        ?= $(abspath $(here)../gipfel.dat)
GIPFEL2HTML ?= $(here)gipfel2html
V           ?= 0.05

images    := $(wildcard *.jpg)
slides    := $(addprefix gipfelweb/slides/, $(images))
thumbs    := $(addprefix gipfelweb/thumbs/, $(images))
positions := $(addprefix gipfelweb/gipfel/, $(addsuffix .pos, $(basename $(images))))
imgpos    := $(addprefix gipfelweb/gipfel/, $(addsuffix .imgs, $(basename $(images))))
hillpos   := $(addprefix gipfelweb/gipfel/, $(addsuffix .hills, $(basename $(images))))
html      := $(addprefix gipfelweb/, $(addsuffix .html, $(basename $(images))))

all:
	echo "make gipfelweb - create a HTML based view of gipfel oriented"
	echo "images in the current directory."

gipfelweb: dirs style $(slides) $(thumbs) $(imgpos) $(hillpos) $(html) gipfelweb/index.html

clean:
	rm -rf gipfelweb/gipfel

clobber:
	rm -rf gipfelweb

dirs:
	mkdir -p gipfelweb/slides
	mkdir -p gipfelweb/thumbs
	mkdir -p gipfelweb/gipfel

style: gipfelweb/style.css

gipfelweb/style.css: $(here)style.css
	cp $< $@

# slides: web display size, capped at 6000 px on the long edge so
# both normal photos and very wide stitched images keep a sane size
gipfelweb/slides/%.jpg: %.jpg
	convert $< -filter Lanczos -resize 6000x6000\> -quality 90 $@

# thumbs: gallery cards and hover popups
gipfelweb/thumbs/%.jpg: %.jpg
	convert $< -filter Point -resize 480x480\> -quality 80 $@

gipfelweb/gipfel/%.pos: %.jpg
	$(GIPFEL_CLI) export-position $< > $@

# photo cross-links: the position index holds only a handful of
# entries, so disable the prominence filter entirely (-V 0); it is
# meant for the 21k peak database, not for neighbouring photos
#
# per-image %.opts are appended after the standard flags, so they can
# override -d/-V
gipfelweb/gipfel/%.imgs: gipfelweb/slides/%.jpg %.opts gipfelweb/gipfel/index.gipfel
	$(GIPFEL_CLI) export-hills $< -d $(DATA) -V 0 -e gipfelweb/gipfel/index.gipfel `cat $*.opts` > $@

gipfelweb/gipfel/%.imgs: gipfelweb/slides/%.jpg gipfelweb/gipfel/index.gipfel
	$(GIPFEL_CLI) export-hills $< -d $(DATA) -V 0 -e gipfelweb/gipfel/index.gipfel > $@

gipfelweb/gipfel/%.hills: gipfelweb/slides/%.jpg %.opts
	$(GIPFEL_CLI) export-hills $< -d $(DATA) -V $(V) `cat $*.opts` > $@

gipfelweb/gipfel/%.hills: gipfelweb/slides/%.jpg
	$(GIPFEL_CLI) export-hills $< -d $(DATA) -V $(V) > $@

gipfelweb/gipfel/index.gipfel: $(positions)
	cat $(positions) > gipfelweb/gipfel/index.gipfel

gipfelweb/%.html: gipfelweb/slides/%.jpg gipfelweb/gipfel/%.imgs gipfelweb/gipfel/%.hills %.title $(thumbs) gipfelweb/style.css
	$(GIPFEL2HTML) page $* > $@

gipfelweb/%.html: gipfelweb/slides/%.jpg gipfelweb/gipfel/%.imgs gipfelweb/gipfel/%.hills $(thumbs) gipfelweb/style.css
	$(GIPFEL2HTML) page $* > $@

gipfelweb/index.html: $(thumbs) $(html)
	$(GIPFEL2HTML) gallery $(basename $(images)) > $@
