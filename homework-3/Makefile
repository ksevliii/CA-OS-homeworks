CC ?= cc
CFLAGS ?= -O2 -g
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Iinclude -pthread
LDLIBS += -pthread
ifeq ($(shell uname -s),Linux)
LDLIBS += -lrt
endif
SOURCES := $(filter-out src/main.c,$(wildcard src/*.c))
OBJECTS := $(SOURCES:src/%.c=build/%.o)
PROGRAMS := port_sim port_fork_named port_fork_unnamed port_dispatcher port_fifo_dispatcher port_worker port_ship port_tug port_berth port_crane port_warehouse
.PHONY: all clean test doc sanitize
all: $(PROGRAMS)
port_sim: $(OBJECTS) build/main.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@
port_fork_named: $(OBJECTS) build/entry_named.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@
port_fork_unnamed: $(OBJECTS) build/entry_unnamed.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@
port_dispatcher: $(OBJECTS) build/entry_shm.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@
port_fifo_dispatcher: $(OBJECTS) build/entry_fifo.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@
port_worker: $(OBJECTS) build/entry_worker.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@
ROLE_PROGRAMS := port_ship port_tug port_berth port_crane port_warehouse
$(ROLE_PROGRAMS): port_%: $(OBJECTS) build/role_%.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDLIBS) -o $@
build/role_ship.o: ROLE=0
build/role_tug.o: ROLE=1
build/role_berth.o: ROLE=2
build/role_crane.o: ROLE=3
build/role_warehouse.o: ROLE=4
build/role_%.o: src/main.c $(wildcard include/*.h) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DACTOR_ROLE=$(ROLE) -c $< -o $@
build/entry_named.o: src/main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDEFAULT_MODE='"fork-named"' -c $< -o $@
build/entry_unnamed.o: src/main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDEFAULT_MODE='"fork-unnamed"' -c $< -o $@
build/entry_shm.o: src/main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDEFAULT_MODE='"shm"' -c $< -o $@
build/entry_fifo.o: src/main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDEFAULT_MODE='"fifo"' -c $< -o $@
build/entry_worker.o: src/main.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -DDEFAULT_MODE='"worker"' -c $< -o $@
build/%.o: src/%.c $(wildcard include/*.h) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build:
	mkdir -p $@
clean:
	rm -rf build $(PROGRAMS) port_tests
sanitize:
	$(MAKE) clean
	$(MAKE) all port_tests CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Iinclude -pthread -fsanitize=address,undefined -fno-omit-frame-pointer' LDLIBS='$(LDLIBS) -fsanitize=address,undefined'
port_tests: tests/test_port.c tests/test_support.c tests/test_support.h
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_port.c tests/test_support.c -o $@
test: all port_tests
	./port_tests
doc:
	doxygen Doxyfile
