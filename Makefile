TARGET_EXEC ?= myprogram
TARGET_TEST ?= test-lab

BUILD_DIR ?= build
TEST_DIR ?= tests
SRC_DIR ?= src
EXE_DIR ?= app
COVERAGE_DIR ?= coverage

SRCS := $(shell find $(SRC_DIR) -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

TEST_SRCS := $(shell find $(TEST_DIR) -name *.c)
TEST_OBJS := $(TEST_SRCS:%=$(BUILD_DIR)/%.o)
TEST_DEPS := $(TEST_OBJS:.o=.d)

EXE_SRCS := $(shell find $(EXE_DIR) -name *.c)
EXE_OBJS := $(EXE_SRCS:%=$(BUILD_DIR)/%.o)
EXE_DEPS := $(EXE_OBJS:.o=.d)

CFLAGS ?= -Wall -Wextra -fno-omit-frame-pointer -fsanitize=address -g -MMD -MP --coverage
LDFLAGS ?= -pthread -lreadline --coverage

all: $(TARGET_EXEC) $(TARGET_TEST)

$(TARGET_EXEC): $(OBJS) $(EXE_OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(EXE_OBJS) -o $@ $(LDFLAGS)

$(TARGET_TEST): $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_OBJS)  -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Run the test suite
test: $(TARGET_TEST)
	./$(TARGET_TEST)	

check: $(TARGET_TEST)
	ASAN_OPTIONS=detect_leaks=1 ./$<

# Generate coverage report
coverage: $(TARGET_TEST)
	./$(TARGET_TEST)
	mkdir -p $(COVERAGE_DIR)
	# Capture all coverage
	lcov --capture --directory . --output-file $(COVERAGE_DIR)/coverage.info
	# Remove tests/harness/ from the coverage report
	lcov --remove $(COVERAGE_DIR)/coverage.info '*/tests/harness/*' --output-file $(COVERAGE_DIR)/coverage_filtered.info
	# Generate HTML report
	genhtml $(COVERAGE_DIR)/coverage_filtered.info --output-directory $(COVERAGE_DIR)
	@echo "Coverage report generated in $(COVERAGE_DIR)/index.html"

.PHONY: clean
clean:
	$(RM) -rf $(BUILD_DIR) $(TARGET_EXEC) $(TARGET_TEST) $(COVERAGE_DIR)

# Install the libs needed to use git send-email on codespaces
.PHONY: install-deps
install-deps:
	sudo apt-get update -y
	sudo apt-get install -y libio-socket-ssl-perl libmime-tools-perl

-include $(DEPS) $(TEST_DEPS) $(EXE_DEPS)
