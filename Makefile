CXXFLAGS+=`pkg-config --cflags opencv`
LIBS+=`pkg-config --libs opencv`

.PHONY: all clean

all: termview

termview: termview.cc
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)

clean:
	rm -f termview
