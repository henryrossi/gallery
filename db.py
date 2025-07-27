def setenv(lldb):
    target = lldb.debugger.GetSelectedTarget()
    launchInfo = target.GetLaunchInfo()
    env = launchInfo.GetEnvironment()
    env.PutEntry("VULKAN_SDK=/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS")
    env.PutEntry("PATH=/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS/bin")
    env.PutEntry(
        "DYLD_LIBRARY_PATH=/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS/lib"
    )
    env.PutEntry(
        "VK_ICD_FILENAMES=/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS/share/vulkan/icd.d/MoltenVK_icd.json"
    )
    env.PutEntry(
        "VK_LAYER_PATH=/Users/hrossi/dev/gallery/VulkanSDK/1.4.309.0/macOS/share/vulkan/explicit_layer.d"
    )
    launchInfo.SetEnvironment(env, True)
    target.SetLaunchInfo(launchInfo)
