UTSBASE	= ../..

MODULE		= ctf
OBJECTS		= $(CTF_OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/arm/Makefile.arm

ALL_TARGET	= $(BINARY)
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

CPPFLAGS	+= -I$(SRC)/common/ctf -DCTF_OLD_VERSIONS
LDFLAGS		+= $(BREDUCE) -M$(UTSBASE)/common/ctf/mapfile -dy

#
# For now, disable these lint checks; maintainers should endeavor
# to investigate and remove these for maximum lint coverage.
# Please do not carry these forward to new Makefiles.
#
CERRWARN	+= -_gcc=-Wno-uninitialized

.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

include $(UTSBASE)/arm/Makefile.targ
