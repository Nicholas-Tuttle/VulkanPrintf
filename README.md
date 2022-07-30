# VulkanPrintf
 A reference project for using the Vulkan printf feature.
 
 This is a minimal project to print a debug and report message from a GLSL or HLSL shader. It uses a headless Vulkan instance to print the first 16 global invocation ID X values (0-15). It prints once for a GLSL shader and once in an HLSL shader.
 
 The Vulkan SDK must be installed and the VULKAN_SDK environment variable must be set in order to compile from the Visual Studio solutions. No other setup should be necessary on Windows.
 
 Note that this currently only is tested on Windows, and compile features are not provided for any other platforms.
