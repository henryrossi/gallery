FLAGS = -Isrc -IVulkanSDK/1.4.309.0/macOS/include -Lthirdparty_lib \
	  -LVulkanSDK/1.4.309.0/macOS/lib -rpath VulkanSDK/1.4.309.0/macOS/lib \
	  -lglfw3 -lvulkan -framework Cocoa -framework IOKit

.PHONY: grove run

grove: 
	gcc -o grove src/grove/grove_main.c $(FLAGS) -Wall -g -Wno-unused-function


GLSLC = ./VulkanSDK/1.4.309.0/macOS/bin/glslc
shaders:
	$(GLSLC) src/shaders/shader.vert -o src/shaders/vert.spv
	$(GLSLC) src/shaders/shader.frag -o src/shaders/frag.spv

	$(GLSLC) src/shaders/quad.vert -o src/shaders/quadVert.spv
	$(GLSLC) src/shaders/quad.frag -o src/shaders/quadFrag.spv

run: grove
	./run.sh grove
	
clean:
	rm glyph
