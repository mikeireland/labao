########################################################################
# Makefile                                                             #
#                                                                      #
# For labao control client 					       #
#                                                                      #
########################################################################
#                                                                      #
#		THE CHARA ARRAY					       #
#     Center for High Angular Resolution Astronomy                     #
# Georgia State University, Atlanta GA 30303-3083, U.S.A.              #
#                                                                      #
#                                                                      #
# Telephone: 1-626-796-5405                                            #
# Fax      : 1-626-796-6717                                            #
# email    : theo@chara.gsu.edu                                        #
# WWW      : http:www.chara.gsu.edu 		                       #
#                                                                      #
# (C) This source code and its associated executable                   #
# program(s) are copyright.                                            #
#                                                                      #
########################################################################
#                                                                      #
# Author : Theo ten Brummelaar                                         #
# Date   : October 2002 					       #
########################################################################

#
# Some definitions
#

CC = gcc
CFLAGS = -g -O2 -Wall -DUSE_GTK `chara-gtk-cflags`
LFLAGS =  -L/usr/X11/lib -lastrogtk -lclockclient -lgtkclient -lclient -lchara -lcfitsio -lsockman -lnsimpleX -lX11 -lrwfits -lnrc -lsla -lz `chara-gtk-libs` -lm
LIBDIR = /usr/local/lib
INCDIR = /usr/local/include
HELPDIR= /usr/local/lib/ui_help
BINDIR=/usr/local/bin
LIBDIR=/usr/local/lib
INCDIR=/usr/local/include
ETCDIR=/ctrscrut/chara/etc
LIBNAME=clockclient

#
# The targets
#

all: labaogtk

#
# The user interface
#

labaogtk: labaogtk.o messages.o dm_page.o display.o wfs_page.o
	${CC} -o labaogtk labaogtk.o messages.o dm_page.o display.o \
	wfs_page.o $(LFLAGS)

labaogtk.o: labaogtk.c labaogtk.h
	${CC} ${CFLAGS} -c labaogtk.c

messages.o: messages.c labaogtk.h
	${CC} ${CFLAGS} -c messages.c

dm_page.o: dm_page.c labaogtk.h
	${CC} ${CFLAGS} -c dm_page.c

display.o: display.c labaogtk.h
	${CC} ${CFLAGS} -c display.c

wfs_page.o: wfs_page.c labaogtk.h
	${CC} ${CFLAGS} -c wfs_page.c

#
# Cleanup
#

clean:
	rm -rf core labaogtk labaogtk.o messages.o dm_page.o display.o \
		wfs_page.o

#
# install
#

install: labaogtk 
	cp labaogtk $(BINDIR)
	chmod a+x $(BINDIR)/labaogtk

uninstall:
	rm -f $(BINDIR)/labaogtk
