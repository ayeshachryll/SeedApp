# Makefile for SeedApp

CXX = g++
# CXXFLAGS = -std=c++11 -Wall -pthread
TARGET = seedapp
LDFLAGS = -lpthread
# SOURCES = main.cpp SeedApp.cpp Server.cpp Client.cpp
# HEADERS = SeedApp.h Server.h Client.h
# OBJECTS = $(SOURCES:.cpp=.o)

seedplayground: seed_playground.cpp 
		$(CXX) -o seed_playground seed_playground.cpp $(LDFLAGS)
		cp seed_playground ./seed1
		cp seed_playground ./seed2
		cp seed_playground ./seed3

seed: SeedApp.cpp Client.cpp Server.cpp
		$(CXX) -o seed SeedApp.cpp Client.cpp Server.cpp $(LDFLAGS)
		cp seed ./seed1
		cp seed ./seed2
		cp seed ./seed3

# Default target
# all: $(TARGET)

# Build the executable
# $(TARGET): $(OBJECTS)
# 	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

# Compile source files to object files
# %.o: %.cpp $(HEADERS)
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
# clean:
# 	rm -f $(OBJECTS) $(TARGET)

# Rebuild everything
# rebuild: clean all

# .PHONY: all clean rebuild
