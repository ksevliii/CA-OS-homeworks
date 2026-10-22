CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -O2 -I./include 

BUILD_DIR = build
SOURCES = src/main.c src/logger.c src/statistics.c \
          src/queue.c src/resources.c src/ship.c \
          src/dispatcher.c src/port.c src/simulation.c \
          src/config.c src/ui.c

OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(SOURCES:.c=.o)))
EXECUTABLE = port_sim

vpath %.c src

.PHONY: all clean run test doc doc_clean

all: $(BUILD_DIR) $(EXECUTABLE)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@$(CC) -c $(CFLAGS) $< -o $@

$(EXECUTABLE): $(OBJS)
	@$(CC) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

run: $(EXECUTABLE)
	@./$(EXECUTABLE) config.txt 1000

test: $(EXECUTABLE)
	@chmod +x run_tests.sh
	@./run_tests.sh

clean:
	@rm -rf $(BUILD_DIR) $(EXECUTABLE) port.log

doc:
	@doxygen Doxyfile

doc_clean:
	@rm -rf docs