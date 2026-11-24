CXX := g++
CXXFLAGS := -std=c++20 -O3 -Wall -Wextra -Wpedantic -Isrc
LDFLAGS := -lX11

TARGET := swarm_sim
TEST_BIN := test_swarm

SRCS := src/main.cpp src/swarm.cpp src/renderer.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all test benchmark report clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(TEST_BIN): tests/test_swarm.cpp src/swarm.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test: $(TEST_BIN)
	./$(TEST_BIN)

benchmark: $(TARGET)
	./$(TARGET) --benchmark report/data

report: report/data/scaling.csv
	cd report && pdflatex -interaction=nonstopmode report.tex && bibtex report && pdflatex -interaction=nonstopmode report.tex && pdflatex -interaction=nonstopmode report.tex

report/data/scaling.csv: $(TARGET)
	./$(TARGET) --benchmark report/data

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_BIN)
	rm -f report/*.aux report/*.bbl report/*.blg report/*.log report/*.out report/*.toc
