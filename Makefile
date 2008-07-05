# PYTHON := python2.4

.PHONY: all clean reconfigure install
all: .configure
	$(PYTHON) ./waf -v 2>&1 | sed -e 's/^\.\.\//\.\//'

.configure:
	$(PYTHON) ./waf configure
	@touch .configure

reconfigure:
	$(PYTHON) ./waf configure

install:
	$(PYTHON) ./waf install

clean:
	@rm .configure
	$(PYTHON) ./waf clean
