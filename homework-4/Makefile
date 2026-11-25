CC ?= cc
CFLAGS ?= -O2 -g
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Wimplicit-fallthrough -Iinclude -MMD -MP
SOURCES := $(filter-out src/main.c,$(wildcard src/*.c))
OBJECTS := $(SOURCES:src/%.c=build/%.o)
ROLE_PROGRAMS := port_ship port_tug port_berth port_crane port_warehouse
PROGRAMS := port_server port_dispatcher port_sim $(ROLE_PROGRAMS) port_observer
.PHONY: all clean test doc sanitize
all: $(PROGRAMS)
port_server port_dispatcher port_sim: $(OBJECTS) build/main.o
	@$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@
$(ROLE_PROGRAMS): port_%: $(OBJECTS) build/role_%.o
	@$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@
port_observer: $(OBJECTS) build/observer_main.o
	@$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@
build/role_ship.o: ROLE=0
build/role_tug.o: ROLE=1
build/role_berth.o: ROLE=2
build/role_crane.o: ROLE=3
build/role_warehouse.o: ROLE=4
build/role_%.o: src/main.c | build
	@$(CC) $(CPPFLAGS) $(CFLAGS) -DACTOR_ROLE=$(ROLE) -c $< -o $@
build/observer_main.o: src/main.c | build
	@$(CC) $(CPPFLAGS) $(CFLAGS) -DOBSERVER_CLIENT -c $< -o $@
build/%.o: src/%.c | build
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
build:
	@mkdir -p $@
port_tests: tests/test_port.c tests/test_support.c tests/test_frames.c tests/test_support.h src/frames.c $(wildcard include/*.h)
	@$(CC) $(CPPFLAGS) $(filter-out -MMD -MP,$(CFLAGS)) -Itests tests/test_port.c tests/test_support.c tests/test_frames.c src/frames.c -o $@
test: all port_tests
	@./port_tests
sanitize:
	@$(MAKE) clean
	@$(MAKE) all port_tests CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Iinclude -fsanitize=address,undefined -fno-omit-frame-pointer' LDLIBS='-fsanitize=address,undefined'
doc:
	@doxygen Doxyfile
clean:
	@rm -rf build $(PROGRAMS) port_tests
-include $(wildcard build/*.d)
