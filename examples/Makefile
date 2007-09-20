images    := $(wildcard *.jpg)
slides    := $(addprefix gipfelweb/slides/, $(images))
thumbs    := $(addprefix gipfelweb/thumbs/, $(images))
positions := $(addprefix gipfelweb/gipfel/, $(addsuffix .pos, $(basename $(images))))
imgpos    := $(addprefix gipfelweb/gipfel/, $(addsuffix .imgs, $(basename $(images))))
hillpos   := $(addprefix gipfelweb/gipfel/, $(addsuffix .hills, $(basename $(images))))
html      := $(addprefix gipfelweb/, $(addsuffix .html, $(basename $(images))))
htmlhills := $(addprefix gipfelweb/hills/, $(addsuffix .html, $(basename $(images))))

all:
	echo "make gipfelweb - create a HTML based we of gipfel oriented"
	echo "images in the current directory."

gipfelweb: dirs $(slides) $(thumbs) $(imgpos) $(hillpos) $(html) $(htmlhills)

clean:
	rm -rf gipfelweb/gipfel

clobber:
	rm -rf gipfelweb

dirs:
	mkdir -p gipfelweb/slides
	mkdir -p gipfelweb/thumbs
	mkdir -p gipfelweb/hills
	mkdir -p gipfelweb/gipfel

gipfelweb/slides/%.jpg: %.jpg
	anytopnm $< | pamscale -xyfit 10000 500 | pnmtojpeg -quality 94 > $@
	jhead -te $< $@

gipfelweb/thumbs/%.jpg: %.jpg
	anytopnm $< | pamscale -xyfit 400 50 | pnmtojpeg -quality 80 > $@

gipfelweb/gipfel/%.pos: %.jpg
	gipfel -p $< > $@

gipfelweb/gipfel/%.imgs: gipfelweb/slides/%.jpg %.opts gipfelweb/gipfel/index.gipfel
	gipfel `cat $*.opts` -e gipfelweb/gipfel/index.gipfel $< > $@

gipfelweb/gipfel/%.imgs: gipfelweb/slides/%.jpg gipfelweb/gipfel/index.gipfel
	gipfel -V 0.05 -e gipfelweb/gipfel/index.gipfel $< > $@

gipfelweb/gipfel/%.hills: gipfelweb/slides/%.jpg
	gipfel -V 0.05 -E $< > $@

gipfelweb/gipfel/index.gipfel: $(positions)
	cat $(positions) > gipfelweb/gipfel/index.gipfel


gipfelweb/%.html: gipfelweb/slides/%.jpg gipfelweb/gipfel/%.imgs %.title $(thumbs)
	gipfel2html images "`cat $*.title`" $* > $@

gipfelweb/%.html: gipfelweb/slides/%.jpg gipfelweb/gipfel/%.imgs $(thumbs)
	gipfel2html images $* $* > $@

gipfelweb/hills/%.html: gipfelweb/slides/%.jpg gipfelweb/gipfel/%.hills %.title
	gipfel2html hills "`cat $*.title`" $* > $@

gipfelweb/hills/%.html: gipfelweb/slides/%.jpg gipfelweb/gipfel/%.hills
	gipfel2html hills $* $*  > $@
