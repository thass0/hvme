CC = clang
CFLAGS = -g -fsanitize=address -Werror -Wall -Wextra -pedantic-errors -std=gnu11
LDFLAGS =  -lm
CPPFLAGS =

BUILD_DIR = build
SOURCE_DIR = src
SOURCES = $(filter-out $(wildcard $(SOURCE_DIR)/repl.*), $(wildcard $(SOURCE_DIR)/*.c))
OBJECTS = $(patsubst $(SOURCE_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))
BINARY = $(BUILD_DIR)/hvme
DEPS = $(OBJECTS:%.o=%.d)

TEST_SOURCE_DIR = tests
TEST_BUILD_DIR = tests/build
TEST_SOURCES = $(wildcard $(TEST_SOURCE_DIR)/*.c) 
TEST_OBJECTS = $(patsubst $(TEST_SOURCE_DIR)/%.c, $(TEST_BUILD_DIR)/%.o, $(TEST_SOURCES))
# Filter out the executable's main function
TEST_OBJECTS += $(filter-out $(BUILD_DIR)/main.o, $(OBJECTS))
TEST_DEPS = $(TEST_OBJECTS:%.o=%.d)
TEST_BINARY = $(TEST_BUILD_DIR)/vmtest

.PHONY = all clean run test examples

all: $(BINARY)
	@echo --- Build done ---

run: all
	./$(BINARY) $(args)

test: CPPFLAGS = -D UNIT_TESTS
test: $(TEST_BINARY)
	./$(TEST_BINARY) $(args)

examples: $(BINARY)
	python3 $(TEST_SOURCE_DIR)/integration.py $(BINARY)



$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $(BINARY)

-include $(DEPS)

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -I$(SOURCE_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(TEST_BINARY): $(TEST_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -I$(SOURCE_DIR) -I$(TEST_SOURCE_DIR) $(TEST_OBJECTS) -o $(TEST_BINARY)

-include $(TEST_DEPS)

$(TEST_BUILD_DIR)/%.o: $(TEST_SOURCE_DIR)/%.c | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -I$(TEST_SOURCE_DIR) -c $< -o $@

$(TEST_BUILD_DIR):
	mkdir $(TEST_BUILD_DIR)

clean:
	$(RM) -r $(BUILD_DIR) $(DEPS) $(TEST_BUILD_DIR)

