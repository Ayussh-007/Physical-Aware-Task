CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

# Detect OS for raylib linking flags
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S), Darwin)
    RAYLIB_FLAGS = -I/opt/homebrew/include -I. -L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
else ifeq ($(UNAME_S), Linux)
    RAYLIB_FLAGS = -I. -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
else
    # Windows MinGW / w64devkit
    RAYLIB_FLAGS = -I. -lraylib -lopengl32 -lgdi32 -lwinmm
endif

all: gui demo test_scheduler bench

gui: main.cpp raygui.h
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(RAYLIB_FLAGS)

demo: demo.cpp scheduler.hpp demo_project.hpp
	$(CXX) $(CXXFLAGS) -o $@ demo.cpp

test_scheduler: test_scheduler.cpp scheduler.hpp demo_project.hpp
	$(CXX) $(CXXFLAGS) -o $@ test_scheduler.cpp

bench: bench.cpp scheduler.hpp
	$(CXX) $(CXXFLAGS) -o $@ bench.cpp

test: test_scheduler
	./test_scheduler

clean:
	rm -f gui demo test_scheduler bench
