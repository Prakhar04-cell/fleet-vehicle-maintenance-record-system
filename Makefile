CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
LIBS ?= -lsqlite3 -lssl -lcrypto
LDFLAGS ?=

# Executable extension and Windows-specific configuration
ifeq ($(OS),Windows_NT)
    EXE = .exe
    LIBS += -lws2_32
    ifneq ($(wildcard C:/msys64/ucrt64/include/sqlite3.h),)
        CFLAGS += -I"C:/msys64/ucrt64/include"
        LDFLAGS += -L"C:/msys64/ucrt64/lib"
    endif
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
       src/ui.c \
       src/dsa.c \
       src/work_order.c \
       src/inspection.c \
       src/http_server.c

MAIN_SRC = src/main.c
TEST_SRC = tests/test_main.c

OBJS = $(SRCS:.c=.o)
MAIN_OBJ = $(MAIN_SRC:.c=.o)
TEST_OBJ = $(TEST_SRC:.c=.o)

.PHONY: all run web dsa test check format format-check clean directories sanitize seed help

all: directories $(TARGET)

directories:
	@python -c "import os; [os.makedirs(d, exist_ok=True) for d in ['data', 'reports', 'backups', 'web']]"

$(TARGET): $(OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo Build complete: $@

$(TEST_TARGET): $(OBJS) $(TEST_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo Test suite build complete: $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

web: all
	./$(TARGET) --web

dsa: all
	./$(TARGET) --dsa

test: directories $(TEST_TARGET)
	./$(TEST_TARGET)

check:
	@echo Checking syntax and compiler warnings across all modules...
	$(CC) $(CFLAGS) -fsyntax-only $(SRCS) $(MAIN_SRC) $(TEST_SRC)
	@echo [CHECK PASSED] Syntax and diagnostics clean.

format:
	@python -c "import shutil, sys, subprocess, glob; (print('Error: clang-format is not installed or not in PATH.') or sys.exit(1)) if not shutil.which('clang-format') else subprocess.run(['clang-format', '-i'] + glob.glob('src/*.c') + glob.glob('include/*.h') + glob.glob('tests/*.c'))"

format-check:
	@python -c "import shutil, sys, subprocess, glob; (print('Error: clang-format is not installed or not in PATH.') or sys.exit(1)) if not shutil.which('clang-format') else subprocess.run(['clang-format', '--dry-run', '--Werror'] + glob.glob('src/*.c') + glob.glob('include/*.h') + glob.glob('tests/*.c'), check=True)"

seed: directories $(TARGET)
	sqlite3 data/fleet.db < migrations/001_initial_schema.sql
	sqlite3 data/fleet.db < data/seed_sample_data.sql
	@echo Sample data loaded into data/fleet.db.

sanitize: CFLAGS += -fsanitize=address,undefined -g
sanitize: clean test

clean:
	@python -c "import os, glob; [os.remove(f) for f in glob.glob('src/*.o') + glob.glob('tests/*.o') + glob.glob('*.o') if os.path.exists(f)]"
	@python -c "import os; [os.remove(f) for f in ['fleet$(EXE)', 'test_runner$(EXE)'] if os.path.exists(f)]"
	@python -c "import os, glob; [os.remove(f) for f in glob.glob('data/test_*.db*') if os.path.exists(f)]"
	@echo Cleaned build object files, test runner, and temporary test databases.
	@echo Production database data/fleet.db preserved.

help:
	@echo "Available Makefile Targets:"
	@echo "  all          - Build the fleet management application (default)"
	@echo "  run          - Build and launch console interactive mode"
	@echo "  web          - Build and launch Winsock2 web server on port 8080"
	@echo "  dsa          - Build and run C11 algorithm benchmark demo"
	@echo "  test         - Build and execute the C unit test suite (199 assertions)"
	@echo "  check        - Run compiler syntax check and diagnostics without linking"
	@echo "  format       - Format C source and header files using clang-format"
	@echo "  format-check - Verify C formatting conformance without modifying files"
	@echo "  seed         - Load schema and sample data into production database"
	@echo "  sanitize     - Build and test with AddressSanitizer and UndefinedBehaviorSanitizer"
	@echo "  clean        - Remove object files and test databases (preserves fleet.db)"
	@echo "  help         - Display this help summary"

