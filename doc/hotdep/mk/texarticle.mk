ifndef TEX_CMD
TEX_CMD		= latex
endif

ifndef TARGET
TARGET		= paper
endif

ifndef TEX_SRCS
TEX_SRCS	:= $(wildcard *.tex) $(wildcard *.bib)
endif

ifndef SUBDIRS
SUBDIRS		:= $(shell [ -d plots ] && echo plots) \
		   $(shell [ -d figs  ] && echo figs)
endif

ifndef PAPER_SIZE
PAPER_SIZE	= a4
endif

PDF_LATEX_CMD := pdflatex

#release default tex->dvi rule
%.dvi: %.tex

all: $(TARGET).pdf

pdf: $(TARGET).pdf

ps: $(TARGET).ps

dvi: $(TARGET).dvi

clean:
	rm -f *.aux *.log *.blg *.bbl $(TARGET).dvi $(TARGET).ps $(TARGET).pdf
	rm -f x gv
	for sd in $(SUBDIRS); do $(MAKE) -C $$sd clean; done

force: clean all

#hack to handle pdflatex
ifeq ($(TEX_CMD),$(PDF_LATEX_CMD))
%.pdf: %.bbl
else
%.dvi: %.bbl
endif
	$(TEX_CMD) $(<:.bbl=)
	while grep -q "Rerun to get" $(<:%.bbl=%.log); \
		do $(TEX_CMD) $(<:.bbl=); done

%.ps: %.dvi
	dvips -Ppdf -G0 -t$(PAPER_SIZE) -o $@ $<

%.pdf: %.ps
	ps2pdf -sPAPERSIZE=$(PAPER_SIZE) -dMaxSubsetPct=100 \
		-dSubsetFonts=true \
		-dPDFSETTINGS=/prepress \
		-dEmbedAllFonts=true $<

%.bbl %.blg: %.tex
	for sd in $(SUBDIRS); do $(MAKE) -C $$sd all || break; done
	$(TEX_CMD) $<
	-bibtex $(<:.tex=)

.PHONY: always

always:

define MakeX
@[ -f $@ ] && { touch $@; kill -USR1 $$(cat $@) &>/dev/null || rm $@; }; true
@[ -f $@ ] || { xdvi $(XDVI_OPT) $< & echo $$! >$@; }
endef

define MakeGV
@[ -f $@ ] && { touch $@; kill -HUP $$(cat $@) &>/dev/null || rm $@; }; true
@[ -f $@ ] || { gv $(GV_OPT) $< & echo $$! >$@; }
endef

define MakeAcroRead
{ xpdf $(ACROREAD_OPT) $<; }; true
endef

ifeq ($(TEX_CMD),$(PDF_LATEX_CMD))
x: $(TARGET).pdf always
	$(MakeAcroRead)
else
x: $(TARGET).dvi always
	$(MakeX)
endif

gv: $(TARGET).ps always
	$(MakeGV)

wc:
	@if which detex &>/dev/null; then \
	  detex -l $(TARGET).tex | wc -w; \
	else \
	  wc *.tex; \
	fi

spell:
	detex $(TARGET).tex | spell | sort -u

# Dependencies...
$(TARGET).bbl: $(TEX_SRCS)
