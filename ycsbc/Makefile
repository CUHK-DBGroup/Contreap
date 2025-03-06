CC=g++
CFLAGS=-std=c++20 -I./ -DNDEBUG -O3
WFLAGS=-Wall# -Wno-unused-value -Wno-strict-aliasing -Wno-maybe-uninitialized
LDFLAGS=-lpthread# -lmimalloc -march=native -msse -msse2 -maes
SUBDIRS=
SUBSRCS=$(wildcard core/*.cc) $(wildcard db/*.cc)
OBJECTS=$(SUBSRCS:.cc=.o)
EXEC=ycsbc

all: $(EXEC)

$(EXEC): $(wildcard *.cc) $(OBJECTS)
	$(CC) $(CFLAGS) $(WFLAGS) $^ $(LDFLAGS) -o $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
	$(RM) $(EXEC)

.PHONY: $(SUBDIRS) $(EXEC)
