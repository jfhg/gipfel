images := $(wildcard *.jpg)
slides := $(addprefix gipfelweb/slides/, $(images))
thumbs := $(addprefix gipfelweb/thumbs/, $(images))
html   := $(addprefix gipfelweb/, $(addsuffix .html, $(basename $(images))))


all:
	echo "make gipfelweb - create a HTML based we of gipfel oriented"
	echo "images in the current directory."

gipfelweb: dirs $(slides) $(thumbs) $(html)

clean:
	rm -rf gipfelweb

dirs:
	mkdir -p gipfelweb/slides
	mkdir -p gipfelweb/thumbs

gipfelweb/slides/%.jpg: %.jpg
	anytopnm $< | pamscale -xyfit 200000 600 | convert -unsharp 0.5x0.5 - - | pnmtojpeg -quality=85 > $@

gipfelweb/thumbs/%.jpg: %.jpg
	anytopnm $< | pamscale -xyfit 200000 200 | pnmtojpeg -quality=80 > $@

gipfelweb/%.html: %.jpg index.gipfel
	echo tbd

index.gipfel: $(images)
	echo tbd

