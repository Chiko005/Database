CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# ── Sources shared between the REPL and server ──────────────────────────────
COMMON_SRCS = diskmanager.cpp \
              persistent.cpp \
              table.cpp \
              sql/Lexer.cpp \
              sql/Parser.cpp \
              sql/Executor.cpp

# ── Main REPL target ────────────────────────────────────────────────────────
REPL_SRCS  = main.cpp $(COMMON_SRCS)
REPL_TARGET = main.exe

# ── TCP Server target ───────────────────────────────────────────────────────
SERVER_SRCS   = server.cpp $(COMMON_SRCS)
SERVER_TARGET = server.exe

ifdef _WIN32
  LDFLAGS = -lws2_32
else
  LDFLAGS =
endif

# ---------------------------------------------------------------
all: $(REPL_TARGET) $(SERVER_TARGET)

$(REPL_TARGET): $(REPL_SRCS)
	$(CXX) $(CXXFLAGS) -o $(REPL_TARGET) $(REPL_SRCS)

$(SERVER_TARGET): $(SERVER_SRCS)
	$(CXX) $(CXXFLAGS) -o $(SERVER_TARGET) $(SERVER_SRCS) -lws2_32

# Run automated tests
test: $(REPL_TARGET)
	./$(REPL_TARGET) --test

# Launch old number-menu interface
menu: $(REPL_TARGET)
	./$(REPL_TARGET) --menu

# Launch SQL REPL (default)
run: $(REPL_TARGET)
	./$(REPL_TARGET)

# Launch TCP server
server: $(SERVER_TARGET)
	./$(SERVER_TARGET)

clean:
	del /Q $(REPL_TARGET) $(SERVER_TARGET) 2>nul || rm -f $(REPL_TARGET) $(SERVER_TARGET)

.PHONY: all test menu run server clean
