# gipfelweb: static, CSS-only web galleries of gipfel-annotated
# panoramas. Port of the historical examples/ pipeline to the Rust
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

here        := $(dir $(lastword $(MAKEFILE_LIST)))
GIPFEL_CLI  ?= $(abspath $(here)../target/release/gipfel-cli)
DATA        ?= $(abspath $(here)../gipfel.dat)
GIPFEL2HTML ?= $(here)gipfel2html

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

gipfelweb/slides/%.jpg: %.jpg
	convert $< -filter Lanczos -resize 20000x500\> -quality 94 $@

gipfelweb/thumbs/%.jpg: %.jpg
	convert $< -filter Point -resize 400x50\> -quality 80 $@

gipfelweb/gipfel/%.pos: %.jpg
	$(GIPFEL_CLI) export-position $< > $@

gipfelweb/gipfel/%.imgs: gipfelweb/slides/%.jpg %.opts gipfelweb/gipfel/index.gipfel
	$(GIPFEL_CLI) `cat $*.opts` export-hills $< -d $(DATA) -e gipfelweb/gipfel/index.gipfel > $@

gipfelweb/gipfel/%.imgs: gipfelweb/slides/%.jpg gipfelweb/gipfel/index.gipfel
	$(GIPFEL_CLI) export-hills $< -d $(DATA) -e gipfelweb/gipfel/index.gipfel > $@

gipfelweb/gipfel/%.hills: gipfelweb/slides/%.jpg %.opts
	$(GIPFEL_CLI) `cat $*.opts` export-hills $< -d $(DATA) > $@

gipfelweb/gipfel/%.hills: gipfelweb/slides/%.jpg
	$(GIPFEL_CLI) export-hills $< -d $(DATA) > $@

gipfelweb/gipfel/index.gipfel: $(positions)
	cat $(positions) > gipfelweb/gipfel/index.gipfel

gipfelweb/%.html: gipfelweb/slides/%.jpg gipfelweb/gipfel/%.imgs gipfelweb/gipfel/%.hills %.title $(thumbs) gipfelweb/style.css
	$(GIPFEL2HTML) page $* > $@

gipfelweb/%.html: gipfelweb/slides/%.jpg gipfelweb/gipfel/%.imgs gipfelweb/gipfel/%.hills $(thumbs) gipfelweb/style.css
	$(GIPFEL2HTML) page $* > $@

gipfelweb/index.html: $(thumbs) $(html)
	$(GIPFEL2HTML) gallery $(basename $(images)) > $@
