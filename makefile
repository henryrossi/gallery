
export VULKAN_SDK := /Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS
export PATH := $(PATH):$(VULKAN_SDK)/bin
export DYLD_LIBRARY_PATH := $(VULKAN_SDK)/lib:$(DYLD_LIBRARY_PATH)
export VK_ICD_FILENAMES := $(VULKAN_SDK)/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH := $(VULKAN_SDK)/share/vulkan/explicit_layer.d

CFLAGS = -Isrc -IVulkanSDK/1.4.309.0/macOS/include -Lthirdparty_lib \
	  -LVulkanSDK/1.4.309.0/macOS/lib -rpath VulkanSDK/1.4.309.0/macOS/lib \
	  -lglfw3 -lvulkan -framework Cocoa -framework IOKit

glyph: shaders
	gcc -o glyph src/glyph.c $(CFLAGS) -g -Wall -Werror

grove: src/grove/grove_main.c
	gcc -o grove src/grove/grove_main.c $(CFLAGS) -Wall -g -Wno-unused-function

GLSLC = ./VulkanSDK/1.4.309.0/macOS/bin/glslc
shaders:
	$(GLSLC) src/shaders/shader.vert -o src/shaders/vert.spv
	$(GLSLC) src/shaders/shader.frag -o src/shaders/frag.spv

	$(GLSLC) src/shaders/quad.vert -o src/shaders/quadVert.spv
	$(GLSLC) src/shaders/quad.frag -o src/shaders/quadFrag.spv

clean:
	rm glyph
