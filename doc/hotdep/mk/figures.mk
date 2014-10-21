PDF_LATEX_CMD := pdflatex

ifndef TEX_CMD
TEX_CMD:= latex
endif

ifeq ($(TEX_CMD),$(PDF_LATEX_CMD))
TARGET_EXTENSION := pdf
else
TARGET_EXTENSION := eps
endif

ifndef FIG_SRCS
FIG_SRCS	:= $(wildcard *.fig)
FIG_TARGETS	:= $(FIG_SRCS:%.fig=%.$(TARGET_EXTENSION))
endif

ifndef PLOT_SRCS
PLOT_SRCS	:= $(wildcard *.gp)
PLOT_TARGETS	:= $(PLOT_SRCS:%.gp=%.$(TARGET_EXTENSION))

ifeq ($(TEX_CMD),$(PDF_LATEX_CMD))
PLOT_TARGETS+=$(PLOT_SRCS:%.gp=%.eps)
endif
endif

ifndef EPS_GZ_SRCS
EPS_GZ_SRCS	:= $(wildcard *.eps.gz)
EPS_GZ_TARGETS	:= $(EPS_GZ_SRCS:%.eps.gz=%.$(TARGET_EXTENSION))
ifeq ($(TEX_CMD),$(PDF_LATEX_CMD))
EPS_GZ_TARGETS+=$(EPS_GZ_SRCS:%.eps.gz=%.eps)
endif
endif

#for pdflatex the eps's become sources for pdfs
ifeq ($(TEX_CMD),$(PDF_LATEX_CMD))
ifndef EPS_SRCS
EPS_SRCS := $(wildcard *.eps)
EPS_TARGETS := $(EPS_SRCS:%.eps=%.$(TARGET_EXTENSION))
endif
endif

ifndef TARGETS
TARGETS		:= $(FIG_TARGETS) $(PLOT_TARGETS) $(EPS_GZ_TARGETS) $(EPS_TARGETS)
endif

%.pdf: %.fig
	fig2dev -L pdf $< $@ 2>/dev/null

%.pdf: %.eps
	epstopdf $<

%.eps: %.gp
	gnuplot $<

%.eps: %.fig
	fig2dev -L eps $< $@ 2>/dev/null || fig2dev -L ps $< $@

%.eps: %.eps.gz
	gunzip -c $< >$@

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

force: clean all
