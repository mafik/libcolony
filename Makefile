all : colony.js demo_sdl

clean :
	rm -f colony.js colony.wasm demo_sdl

colony.js : src/colony.h src/colony_js.cc src/colony_js_post.js
	em++ -MJ compile_colony_js.json -s INITIAL_MEMORY=327680000 -s TOTAL_STACK=160000000 -O3 -lembind -o $@ -std=c++20 src/colony_js.cc --post-js src/colony_js_post.js
	truncate -s -2 compile_colony_js.json

demo_sdl : src/*
	clang++ -MJ compile_demo.json -o $@ -std=c++20 src/demo_sdl.cc `pkg-config --cflags --libs sdl2`
	truncate -s -2 compile_demo.json

compile_commands.json : compile_colony_js.json compile_demo.json
	jq -s '.' $^ > $@
