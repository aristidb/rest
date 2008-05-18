.PHONY: configure all clean
all: .configure
	./waf -v 2>&1 | sed -e 's/^\.\.\//\.\//'

.configure:
	./waf configure
	@touch .configured

clean:
	./waf clean
