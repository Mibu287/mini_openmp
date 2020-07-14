CXX = clang++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -Wthread-safety

%: %.cpp *.hpp
	${CXX} ${CXXFLAGS} -o $@ $<
