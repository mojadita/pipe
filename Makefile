# Makefile -- makefile for pipe
# Author: Luis Colorado <luiscoloradourcola@gmail.com>
# Date: Tue Feb 27 18:44:26 EET 2018

RM ?= -rm -f

targets = pipe
TOCLEAN += $(targets)

pipe_objs = pipe.o
TOCLEAN += $(pipe_objs)

all: $(targets)
clean:
	$(RM) $(TOCLEAN)

pipe: $(pipe_objs) $(pipe_deps)
	$(CC) $(LDFLAGS) -o $@ $($@_objs) $($@_ldflags) $($@_libs)

pipe.o: pipe.c pipe.i
