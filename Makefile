########################################################################
# Makefile                                                             #
#                                                                      #
# Based on a Makefile by AJB Sept 89 booth@physics.su.edu.au	       #
########################################################################
#                                                                      #
#     Center for High Angular Resolution Astronomy                     #
# Georgia State University, Atlanta GA 30303-3083, U.S.A.              #
#                                                                      #
#                                                                      #
# Telephone: 1-404-651-1882                                            #
# Fax      : 1-404-651-1389                                            #
# email    : theo@chara.gsu.edu                                        #
# WWW      : http:www.chara.gsu.edu~theotheo.html                      #
#                                                                      #
# (C) This source code and its associated executable                   #
# program(s) are copyright.                                            #
#                                                                      #
########################################################################
#                                                                      #
# Author : Theo ten Brummelaar                                         #
# Date   : October 2001  					       #
########################################################################

# fill in your bits for SUBDIRS 

SUBDIRS= server gtkclient

# TARGETS should match "makefile" "targets" in all the SUBDIRS.
# A missing target in a subdir will cause a "fatal" error which will be
# ignored

TARGETS= all clean install uninstall

.KEEP_STATE:

$(TARGETS):
	$(MAKE) $(SUBDIRS) TARGET=$@

$(SUBDIRS): FORCE
	cd $@; $(MAKE) $(TARGET)

FORCE:
# thus "make all" here runs "make all" in all the SUBDIRS
# subdirectories.  Obviously this can be recursive, so
# running copies of itself in lower and lower subdirectories
