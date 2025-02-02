CXXFLAGS+=`pkg-config --cflags opencv4`
LIBS+=`pkg-config --libs opencv4`

.PHONY: all clean

all: termview

termview: termview.cc
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)

clean:
	rm -f termview
