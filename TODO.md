Task List
  [ ] rework text length calculataion using stbtt api
  [ ] split font code into its own module
  [ ] fix UIElement caching bug
  [ ] correctly detect and handle "double" retina displays
  [ ] handle window resizing
  [ ] start working on menu bar for glyph
  [ ] read UI, part 7
  [ ] port onto windows

For when validation layers cannot be found:

export VULKAN_SDK=/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS
export PATH=$VULKAN_SDK/bin:$PATH                          
export DYLD_LIBRARY_PATH=$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH
export VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d   
