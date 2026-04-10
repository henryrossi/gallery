
set clang_common= -Isrc -IC:\VulkanSDK\1.4.341.1\Include -Lthirdparty_lib -LC:\VulkanSDK\1.4.341.1\Lib -lvulkan-1 -lglfw3dll

set clang_debug=   call clang -g -O0 %clang_common% 
set clang_release= call clang -g -O2 %clang_common% 

set compile= %clang_debug%

%compile% src\grove\grove_main.c
