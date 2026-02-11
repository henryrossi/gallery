Task List
  [ ] start working on menu bar for glyph
  [ ] handle window resizing
  [ ] read UI, part 7
  [ ] port onto windows

For when validation layers cannot be found:

export VULKAN_SDK=/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS
export PATH=$VULKAN_SDK/bin:$PATH                          
export DYLD_LIBRARY_PATH=$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH
export VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d   

Task History
February 10, 2026:
  [x] rework text length calculataion using stbtt api
  [x] split font code into its own module
  [x] fix UIElement caching bug
  [x] correctly detect and handle "double" retina displays
