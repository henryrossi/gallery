#!/bin/sh

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

./$1 $2 $3 $4 $5
