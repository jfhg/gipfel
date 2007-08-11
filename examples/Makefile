images    := $(wildcard *.jpg)
slides    := $(addprefix gipfelweb/slides/, $(images))
thumbs    := $(addprefix gipfelweb/thumbs/, $(images))
positions := $(addprefix gipfelweb/gipfel/, $(addsuffix .pos, $(basename $(images))))
dumps     := $(addprefix gipfelweb/gipfel/, $(addsuffix .gipf, $(basename $(images))))
html      := $(addprefix gipfelweb/, $(addsuffix .html, $(basename $(images))))

all:
	echo "make gipfelweb - create a HTML based we of gipfel oriented"
	echo "images in the current directory."

gipfelweb: dirs $(slides) $(thumbs) $(dumps) $(html)

clean:
	rm -rf gipfelweb

dirs:
	mkdir -p gipfelweb/slides
	mkdir -p gipfelweb/thumbs
	mkdir -p gipfelweb/gipfel

gipfelweb/slides/%.jpg: %.jpg
	convert -resize '10000x600>' -quality 90 $< $@

gipfelweb/thumbs/%.jpg: %.jpg
	convert -resize '1000x150>' -quality 80 $< $@

gipfelweb/gipfel/%.pos: %.jpg
	gipfel -e position $< > $@

gipfelweb/gipfel/%.gipf: gipfelweb/slides/%.jpg gipfelweb/gipfel/index.gipfel
	gipfel -v 0.03 -d gipfelweb/gipfel/index.gipfel -e hills $< > $@

gipfelweb/gipfel/index.gipfel: $(positions)
	cat $(positions) > gipfelweb/gipfel/index.gipfel

gipfelweb/%.html: gipfelweb/slides/%.jpg gipfelweb/gipfel/%.gipf $(thumbs)
	gipfel2html $* > $@
