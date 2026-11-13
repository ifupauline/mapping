# Build the paper: make (PDF), make figures (figures and tables from
# ../results), make clean
PDF = gauthier2026-system-level-mapping-automotive.pdf

all: $(PDF)

$(PDF): main.tex refs.bib sections/*.tex figures/*.pdf
	pdflatex -interaction=nonstopmode -halt-on-error main.tex >/dev/null
	bibtex main >/dev/null
	pdflatex -interaction=nonstopmode -halt-on-error main.tex >/dev/null
	pdflatex -interaction=nonstopmode -halt-on-error main.tex >/dev/null
	cp main.pdf $(PDF)

figures:
	python3 make_figures.py --results ../results --out figures
	python3 make_tables.py --results ../results --out sections

clean:
	rm -f main.aux main.bbl main.blg main.log main.out main.pdf main.toc

.PHONY: all figures clean
