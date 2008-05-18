.PHONY: all clean reconfigure
all: .configure
	./waf -v 2>&1 | sed -e 's/^\.\.\//\.\//'

.configure:
	./waf configure
	@touch .configure

reconfigure:
	./waf configure

clean:
	rm .configure
	./waf clean
