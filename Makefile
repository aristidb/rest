.PHONY: all clean
all:
	./waf -v 2>&1 | sed -e 's/^\.\.\//\.\//'

clean:
	./waf clean
