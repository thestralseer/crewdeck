CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
INCLUDES = -Iinclude -Ivendor/mongoose -Ivendor/sqlite3 -Ivendor/cjson -Ivendor/libsodium/include
DEFINES = -DSODIUM_STATIC=1

# Platform specific libraries
ifeq ($(OS),Windows_NT)
    TARGET = crewdeck.exe
    TEST_TARGET = test_runner.exe
    LDFLAGS = vendor/libsodium/lib/libsodium.a -lws2_32 -lbcrypt
    MKDIR = if not exist "$(@D)" mkdir "$(@D)"
    RM = del /f /q
    RMDIR = rmdir /s /q
    SEP = \\
else
    TARGET = crewdeck
    TEST_TARGET = test_runner
    LDFLAGS = -Lvendor/libsodium/lib -lsodium -lpthread -ldl -lm
    MKDIR = mkdir -p $(@D)
    RM = rm -f
    RMDIR = rm -rf
    SEP = /
endif

# Source and Object files
SRC = src/config.c \
      src/util.c \
      src/db.c \
      src/auth.c \
      src/employees.c \
      src/departments.c \
      src/attendance.c \
      src/leave.c \
      src/payroll.c \
      src/dashboard.c \
      src/audit.c \
      src/router.c

VENDOR_SRC = vendor/mongoose/mongoose.c \
             vendor/sqlite3/sqlite3.c \
             vendor/cjson/cJSON.c

OBJ = $(SRC:.c=.o)
VENDOR_OBJ = $(VENDOR_SRC:.c=.o)
MAIN_OBJ = src/main.o
TEST_OBJ = tests/test_main.o

.PHONY: all clean test asan run

all: $(TARGET)

$(TARGET): $(OBJ) $(VENDOR_OBJ) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Vendor objects compiled with relaxed warnings
vendor/mongoose/mongoose.o: vendor/mongoose/mongoose.c
	$(CC) -std=c11 -O2 -Ivendor/mongoose $(DEFINES) -c $< -o $@

vendor/sqlite3/sqlite3.o: vendor/sqlite3/sqlite3.c
	$(CC) -std=c11 -O2 -Ivendor/sqlite3 -DSQLITE_ENABLE_JSON1=1 -DSQLITE_THREADSAFE=1 -c $< -o $@

vendor/cjson/cJSON.o: vendor/cjson/cJSON.c
	$(CC) -std=c11 -O2 -Ivendor/cjson -c $< -o $@

# Application objects
src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

tests/%.o: tests/%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(DEFINES) -c $< -o $@

# Unit test runner
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(OBJ) $(VENDOR_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# AddressSanitizer target
asan: CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address -g -O1
asan: $(TARGET)
	@echo "AddressSanitizer build completed."

run: $(TARGET)
	./$(TARGET)

clean:
	-$(RM) src$(SEP)*.o tests$(SEP)*.o vendor$(SEP)mongoose$(SEP)*.o vendor$(SEP)sqlite3$(SEP)*.o vendor$(SEP)cjson$(SEP)*.o $(TARGET) $(TEST_TARGET) data$(SEP)test_crewdeck.db
