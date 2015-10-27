#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright (c) 2013, Joyent, Inc.  All rights reserved.
# Copyright (c) 2015, Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../../..

#
#	Define the module and object file sets.
#
LOADER		= loader
LOADER_BIN	= $(OBJS_DIR)/$(LOADER)
LOADER_ELF	= $(LOADER).elf
LOADER_ELF_BIN	= $(OBJS_DIR)/$(LOADER_ELF)
ROOTMODULE	= $(ROOT_BCM2836_KERN_DIR)/$(LOADER)
OBJECTS		= $(ARM_LOADER_OBJS:%=$(OBJS_DIR)/%) \
		  $(BCM2836_LOADER_OBJS:%=$(OBJS_DIR)/%)

#
#	Include common rules.
#
include $(UTSBASE)/armv7/bcm2836/Makefile.bcm2836

#
#	Define targets
#
ALL_TARGET	= $(LOADER_BIN)
INSTALL_TARGET	= $(LOADER_BIN) $(ROOTMODULE)

#
#	Overrides
#
CLEANFILES	+= $(OBJECTS)

CLOBBERFILES	= $(CLEANFILES) $(LOADER_BIN)

CFLAGS 		+= $(CCVERBOSE) \
		-DELF_TARGET_ARM #-_gcc=-march=armv7ve #-DMAP_DEBUG

CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-uninitialized
CERRWARN	+= -_gcc=-Wno-unused-label
CERRWARN	+= -_gcc=-Wno-unused-function
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-but-set-variable

# The mapfile used to link unix
MAPFILE		= $(UTSBASE)/armv7/bcm2836/conf/Mapfile.loader

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

install:  	$(INSTALL_DEPS)

symcheck:       $(SYM_DEPS)

$(LOADER_ELF_BIN): $(OBJECTS)
	$(LD) -dy -b -znointerp -o $@ -e _start -M $(MAPFILE) \
	$(OBJECTS)
	$(CTFMERGE) $(CTFMRGFLAGS) -L VERSION -o $(LOADER_ELF_BIN) $(OBJECTS)

$(LOADER_BIN): $(LOADER_ELF_BIN)
	/opt/armtc/usr/gnu/bin/gobjcopy $(LOADER_ELF_BIN) -O binary $(LOADER_BIN)


#
#	Include common targets.
#
include $(UTSBASE)/armv7/bcm2836/Makefile.targ
