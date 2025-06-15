
Tasks to be completed:

Add a uniform to fragment shader that will maintain the aspect ratio of the
image. Background space will be filled from the clamp to border setting of
the image sampler. This will need to be updated everytime the window is 
resized. Basically everytime we need to recreate the swapchain I believe.

Handle user input to change pixel colors. To do this I need to map and fill a 
staging buffer, transition image layout to TRANSFER_DST_OPTIMAL, copy buffer to 
image, and transtion image layout back to SHADER_READ_ONLY_OPTIMAL. I will use
pipeline barriers to synchronize reads and write on the image. These barriers 
with the image layouts need to be set before and after every copy.

Add a color picker and a color history.



For when validation layers cannot be found:

export VULKAN_SDK=/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS

export PATH=$VULKAN_SDK/bin:$PATH                          

export DYLD_LIBRARY_PATH=$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH

export VK_ICD_FILENAMES=$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json

export VK_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d   


Quotes:

- not still like the rigidity of ice, still like calm water

- water shining in sunset animation

- water running in creek animation


