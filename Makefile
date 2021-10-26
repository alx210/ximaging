# -----------------------------------------------------------------------------
# Copyright (C) 2012-2017 alx@fastestcode.org
# This software is distributed under the terms of the MIT license.
# See the included LICENSE file for further information.
# -----------------------------------------------------------------------------

all:
	@if [ -e src/Makefile ]; then \
		$(MAKE) -C src $(MAKEFLAGS); \
	else \
		echo "Run: make <target>" && \
		echo "Available targets are:" && \
		ls mf/ | sed 's/Makefile\.//g'; \
	fi

.PHONY: clean install distclean

install:
	$(MAKE) -C src $(MAKEFLAGS) install

uninstall:
	$(MAKE) -C src $(MAKEFLAGS) uninstall

clean:
	$(MAKE) -C src $(MAKEFLAGS) clean

distclean: clean
	-rm src/Makefile
	
.DEFAULT:
	@if [ -e src/Makefile ]; then rm src/Makefile; fi
	@if ! [ -f mf/Makefile.$@ ]; then \
		echo "Invalid target name: $@" && exit 1; fi
	ln -s ../mf/Makefile.$@ src/Makefile
	$(MAKE) -C src $(MAKEFLAGS)
	@echo "You can now run 'make install' as root."
