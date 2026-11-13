# Build the user manual: make (PDF), make listings (the outputs of the tool,
# from the use cases of examples/), make clean
PDF = gauthier2026-mapping-user-manual.pdf
BIN ?= ../build/bin

all: $(PDF)

$(PDF): manual.tex $(wildcard sections/*.tex) $(wildcard listings/*.txt)
	pdflatex -interaction=nonstopmode -halt-on-error manual.tex >/dev/null
	pdflatex -interaction=nonstopmode -halt-on-error manual.tex >/dev/null
	cp manual.pdf $(PDF)

listings:
	sh make_listings.sh $(BIN)

clean:
	rm -f manual.aux manual.log manual.out manual.pdf manual.toc

.PHONY: all listings clean
