LCC = $(GBDK_HOME)/bin/lcc
PROJECTNAME = marioland
BINS	    = $(PROJECTNAME).gb
CSOURCES := $(shell find src -name '*.c')
CFLAGS = -autobank -Wm-yt0x1A -Wm-yo4 -Wm-ya4

DEBUG=no
ifeq ($(DEBUG),yes)
	CPPFLAGS = -DDEBUG
endif


all:	$(BINS)

$(BINS):	$(CSOURCES)
	$(LCC) $(CFLAGS) -o $@ hUGEDriver.o $(CSOURCES) $(CPPFLAGS)

clean:
	rm -f $(BINS)

clean_assets:
	rm -f src/graphics/* src/levels/* res/*