CC ?= gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
.PHONY: all run web test clean
all: libbanker.so banker
libbanker.so: src/bankers_core.c src/bankers_core.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ src/bankers_core.c
banker: src/bankers_cli.c
	$(CC) $(CFLAGS) -o $@ src/bankers_cli.c
run: banker
	./banker
web: libbanker.so
	python3 web/server.py
test: libbanker.so banker
	python3 -m unittest discover -s tests -v
clean:
	rm -f banker libbanker.so
