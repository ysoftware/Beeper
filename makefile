compflags  := -Wall -Wextra -fno-limit-debug-info -Wuninitialized
# compflags += -fsanitize=address -fsanitize=undefined -fsanitize=leak -fstack-protector-strong

raylib    := lib/raylib-5.5
raylib_i  := -I$(raylib)/src
raylib_l  := build/libraylib.a

all_i := $(raylib_i)
all_l := 
renderer_l := $(raylib_l)
renderer_files := $(raylib_l)

ifeq ($(shell uname), Linux)
	jobs := -j$(shell nproc)

	plug_name  := libplug.so
	dynamic    := 
	compiler   := clang $(compflags) -Iinclude -pedantic -ggdb3 -g -O0
	frameworks := -lGL -lm -lpthread -ldl -lrt -lX11
	
	target_version      := 
	target_version_flag := 
else # macos
	jobs := -j$(shell sysctl -n hw.logicalcpu)
	plug_name  := libplug.dylib
	dynamic    := -undefined dynamic_lookup
	compiler   := clang $(compflags) -Iinclude -pedantic -Wno-deprecated-declarations -mmacosx-version-min=14.7

	target_version      := -DCMAKE_OSX_DEPLOYMENT_TARGET=14.7
	target_version_flag := "-mmacosx-version-min=14.7"

	f := -framework
	frameworks := $(f) CoreAudio     $(f) OpenGL       $(f) AudioToolbox   $(f) AudioUnit \
				  $(f) CoreServices  $(f) Carbon       $(f) CoreVideo      $(f) IOKit \
				  $(f) Cocoa         $(f) GLUT         $(f) CoreFoundation $(f) AppKit
endif

all: build main.app build/$(plug_name)

clean_all:
	@echo $(nproc)
	rm -f main.app
	rm -f build/*
	make clean_raylib -i

clean_raylib:
	rm -f $(raylib_l)
	cd $(raylib) && make clean
	cd $(raylib) && rm CMakeCache.txt

build:
	mkdir -p build

build/libraylib.a:
	make build
	cd $(raylib) && cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DPLATFORM=Desktop -Wno-dev $(target_v)
	cd $(raylib) && make raylib $(jobs)
	cp $(raylib)/raylib/libraylib.a $(raylib_l)

main.app: src/main.c $(renderer_files)
	$(compiler) $(warnings) -rdynamic -o main.app src/main.c $(all_i) $(renderer_l) $(frameworks)

build/$(plug_name): $(all_l) $(renderer_files) src/*
	make build
	touch build/libplug.lock
	$(compiler) $(warnings) $(all_i) -fPIC -c src/plug.c -o build/plug.o
	$(compiler) $(warnings) $(dynamic) -shared -o build/$(plug_name) build/plug.o $(all_l) $(frameworks)
	rm build/libplug.lock

dependencies:
	gcc src/plug.c -o main -MD -Iinclude $(all_i) $(all_l) $(frameworks)
