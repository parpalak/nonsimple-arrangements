PDF = build/main.pdf

.PHONY: all clean

all: $(PDF)

$(PDF): main.tex preamble.sty references.bib sections/*.tex appendix/*.tex figures/*
	latexmk -r latexmkrc -halt-on-error main.tex

clean:
	latexmk -r latexmkrc -C main.tex
