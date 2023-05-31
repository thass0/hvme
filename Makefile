CC = clang
CFLAGS = -g -fsanitize=address -Werror -Wall -Wextra -pedantic-errors -std=gnu11
LDFLAGS = -ledit -lm
CPPFLAGS =

BUILD_DIR = build
SOURCE_DIR = src
SOURCES = $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS = $(patsubst $(SOURCE_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))
BINARY = $(BUILD_DIR)/vme

TEST_SOURCE_DIR = tests
TEST_BUILD_DIR = tests/build
TEST_SOURCES = $(wildcard $(TEST_SOURCE_DIR)/*.c) 
TEST_OBJECTS = $(patsubst $(TEST_SOURCE_DIR)/%.c, $(TEST_BUILD_DIR)/%.o, $(TEST_SOURCES))
# Filter out the executable's main function
TEST_OBJECTS += $(filter-out $(BUILD_DIR)/main.o, $(OBJECTS))
TEST_BINARY = $(TEST_BUILD_DIR)/vmtest

.PHONY = all clean run test set_test_flag

all: $(BINARY)
	@echo --- Build done ---

run: all
	./$(BINARY) $(args)

test: CPPFLAGS = -D UNIT_TESTS
test: $(TEST_BINARY)
	./$(TEST_BINARY)


$(TEST_BINARY): $(TEST_OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -I$(SOURCE_DIR) -I$(TEST_SOURCE_DIR) $(TEST_OBJECTS) -o $(TEST_BINARY)

$(TEST_BUILD_DIR)/%.o: $(TEST_SOURCE_DIR)/%.c | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -I$(TEST_SOURCE_DIR) -c $< -o $@

$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $(BINARY)

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -I$(SOURCE_DIR) -c $< -o $@

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(TEST_BUILD_DIR):
	mkdir $(TEST_BUILD_DIR)

clean:
	$(RM) -r $(BUILD_DIR) $(TEST_BUILD_DIR)

