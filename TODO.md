Task List
  [ ] ui_widget label
  [ ] start working on menu bar for glyph
  [ ] isolate glfw functions
  [ ] generic 'make element' codepath
  [ ] Implement delta-based state mutation
  [ ] Fix framebuffer resize bug:
      Validation Layer: vkAcquireNextImageKHR(): Semaphore must not be currently signaled.
  [ ] write tests for each module
  [ ] resize elements using strictness 
  [ ] port onto windows

For when validation layers cannot be found:

export VULKAN_SDK=/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS
export PATH=$VULKAN_SDK/bin:$PATH                          
export DYLD_LIBRARY_PATH=$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH
export VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d   

Task History
March 25, 2026:
  [x] specify width/height as aspect ratio
March 16, 2026:
  [x] Pixelize large image at smaller scale
March 10, 2026:
  [x] Size text based on content scale
  [x] how to handle discrepancy in sizes with retina displays 
March 9, 2026:
  [x] extended arena functionality to chain pages when out of memory
February 24, 2026:
  [x] finished slider widget and ui input interaction
  [x] handle window resizing
February 17, 2026:
  [x] add padding, corner radius, and border size to ui layout
February 16, 2026:
  [x] handle ui elements input interation
February 15, 2026:
  [x] button embossing and debossing
February 10, 2026:
  [x] rework text length calculataion using stbtt api
  [x] split font code into its own module
  [x] fix UIElement caching bug
  [x] correctly detect and handle "double" retina displays
