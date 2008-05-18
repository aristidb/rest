.PHONY: configure all clean
all: configure
	./waf -v 2>&1 | sed -e 's/^\.\.\//\.\//'

configure: .configured
	./waf configure

.configured:
	@touch .configured

clean:
	./waf clean
