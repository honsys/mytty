#SHELL := /bin/bash
SHELL := /bin/csh -f

OS := $(shell uname )

# CCFLAGS := -c -g -ansi -fPIC -fpermissive -pipe -Wno-deprecated -D$(OS) -Dx86_64 -D_X86_64_ -D_REENTRANT -D_XOPEN_SOURCE=500 -D_POSIX_C_SOURCE -DSYSV -DSYSVSEM -D__EXTENSIONS__  -I./ -I/usr/include/c++/3.4.6/backward
CCFLAGS := -c -g -ansi -fPIC -fpermissive -pipe -Wno-deprecated -D$(OS) -D__USE_XOPEN -D_REENTRANT -I./ -I/usr/include/c++/3.4.6/backward

LDFLAGS :=  -L./$(OS)

OBJ := $(OS)/mytty.o $(OS)/PSem.o $(OS)/Runtime.o $(OS)/PosixRuntime.o

LDLIBS := -lrt -lpthread -lm

$(OS)/mytty: clean $(OBJ)
	$(CXX) -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS)
	-ln -s $(OS)/mytty

clean:
	@echo OS == $(OS)
	@touch mytty $(OS)
	-rm -rf mytty $(OS)

$(OS)/%.o: %.cc
	-@mkdir -p $(OS)
	$(CXX) $(CCFLAGS) -c -o $@ $<
