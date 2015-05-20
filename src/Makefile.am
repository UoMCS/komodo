## Process this file with automake to produce Makefile.in


SUBDIRS = Pixmaps

INCLUDES = @GTK_CFLAGS@

bin_PROGRAMS = kmd flash jimulator

kmd_SOURCES = \
	interface.c interface.h\
	callbacks.c callbacks.h\
	serial.c serial.h\
	view.c view.h\
	misc.c misc.h\
	dotparse.c dotparse.h\
	chump.c chump.h\
	breakcalls.c breakcalls.h\
	breakview.c  breakview.h\
	dotkomodo.string\
	definitions.h global.h\
	viewfuncs.c viewfuncs.h\
	pixmap.c pixmap.h\
	evaluate.c

kmd_LDADD = @GTK_LIBS@

flash_SOURCES = serial.c serial.h\
		flash.c flash.h

flash_LDADD = @GTK_LIBS@

jimulator_SOURCES =   jimulator.c 

jimulator_LDADD = 

EXTRA_DIST = sample.komodo parse.pl

dotkomodo.string: sample.komodo
	parse.pl < sample.komodo > dotkomodo.string 
