#!/bin/sh

for arg in "$@"; do declare $arg='1'; done

VULKAN_SDK="/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS"
export VULKAN_SDK
PATH="$PATH:$VULKAN_SDK/bin"
export PATH
DYLD_LIBRARY_PATH="$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH
VK_ICD_FILENAMES="$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json"
export VK_ICD_FILENAMES
VK_LAYER_PATH="$VULKAN_SDK/share/vulkan/explicit_layer.d"
export VK_LAYER_PATH

if [ -n "${glyph+x}" ]; then ./build/glyph $2 $3 $4 $5; fi
if [ -n "${grove+x}" ]; then ./build/grove; fi
