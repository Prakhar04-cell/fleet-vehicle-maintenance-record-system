CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
LIBS ?= -lsqlite3 -lssl -lcrypto

# Executable extension
ifeq ($(OS),Windows_NT)
    EXE = .exe
else
    EXE =
endif

TARGET = fleet$(EXE)
TEST_TARGET = test_runner$(EXE)

SRCS = src/common.c \
       src/database.c \
       src/auth.c \
       src/input.c \
       src/vehicle.c \
       src/driver.c \
       src/maintenance.c \
       src/schedule.c \
       src/expense.c \
       src/notification.c \
       src/audit.c \
       src/dashboard.c \
       src/pdf_gen.c \
       src/reports_csv.c \
       src/reports_pdf.c \
       src/backup.c \
       src/ui.c

MAIN_SRC = src/main.c
TEST_SRC = tests/test_main.c

OBJS = $(SRCS:.c=.o)
MAIN_OBJ = $(MAIN_SRC:.c=.o)
TEST_OBJ = $(TEST_SRC:.c=.o)

.PHONY: all run test clean directories sanitize

all: directories $(TARGET)

directories:
	@mkdir -p data reports backups

$(TARGET): $(OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo Build complete: $@

$(TEST_TARGET): $(OBJS) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo Test suite build complete: $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

test: directories $(TEST_TARGET)
	./$(TEST_TARGET)

seed: directories $(TARGET)
	sqlite3 data/fleet.db < migrations/001_initial_schema.sql
	sqlite3 data/fleet.db < data/seed_sample_data.sql
	@echo Sample data loaded into data/fleet.db.

sanitize: CFLAGS += -fsanitize=address,undefined -g
sanitize: clean test

clean:
	@rm -f src/*.o tests/*.o $(TARGET) $(TEST_TARGET) data/test_fleet.db
	@echo Cleaned build artifacts.
