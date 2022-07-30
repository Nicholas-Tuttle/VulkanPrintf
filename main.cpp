#include <vulkan/vulkan_core.h>
#include <iostream>
#include <fstream>
#include <vector>

#define EXIT_ON_BAD_RESULT(result) if (VK_SUCCESS != (result)) { fprintf(stderr, "Failure at %u %s\n", __LINE__, __FILE__); exit(EXIT_FAILURE); }

// If this macro is set to "false" all vulkan debug and report messages will be printed
#define SHOW_ONLY_DEBUG_PRINTF_EXT_MESSAGES true

// For more reference, see:
// https://github.com/KhronosGroup/Vulkan-ValidationLayers/blob/master/docs/debug_printf.md
// https://stackoverflow.com/questions/64617959/vulkan-debugprintfext-doesnt-print-anything
// https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_debug_printf.txt
// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers

// Note that "Debug Printf" functionality and "GPU-Assisted Validation" extensions cannot be run at the same time

// This must match the thread sizes in the GLSL and HLSL shader
static const size_t shader_local_size_x = 512;

// VK_LAYER_KHRONOS_validation device extension must be enabled
static const std::vector<const char *> requiredInstanceLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// VK_EXT_debug_utils and VK_EXT_debug_report instance extensions must be enabled
// VK_EXT_debug_utils is needed for the VulkanDebugCallback function
// VK_EXT_debug_report is needed for the VulkanReportCallback function
static const std::vector<const char *> requiredInstanceExtensions = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME
};

// This Vulkan debug callback receives messages from the 
// debugPrintfEXT (GLSL) or printf (HLSL) functions in the
// compute shaders, along with other vulkan messages. 
// For more reference, see:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDebugUtilsMessengerEXT.html
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCreateDebugUtilsMessengerEXT.html
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkDestroyDebugUtilsMessengerEXT.html
// for more info
static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData,
    void *pUserData
)
{
#if SHOW_ONLY_DEBUG_PRINTF_EXT_MESSAGES
    // NOTE:
    // This message filtering can (and probably should) be done as part of the 
    // intialization in "vkCreateDebugUtilsMessengerEXT", using the 
    // VkDebugUtilsMessengerCreateInfoEXT.messageType variable. 
    // The initialization in "CreateDebugMessenger()" does not do any filtering 
    // and it is instead done here to demonstrate one potential usage of the 
    // messageType parameter, but is not optimal.
    if (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT != messageType)
    {
        return VK_FALSE;
    }
#endif

    std::cout << "[VULKAN DEBUG] : ";

    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        {
            std::cout << "[VERBOSE]";
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        {
            std::cout << "[INFO]   ";
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        {
            std::cout << "[WARNING]";
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        {
            std::cout << "[ERROR]  ";
            break;
        }
        default:
        {
            std::cout << "[UNKNOWN]";
            break;
        }
    }

    std::cout << " : [FLAGS]: " << messageType << "\t" << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

// This Vulkan report callback receives messages from the 
// debugPrintfEXT (GLSL) or printf (HLSL) functions in the
// compute shaders, along with other vulkan messages. 
// For more reference, see:
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDebugReportCallbackEXT.html
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCreateDebugReportCallbackEXT.html
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkDestroyDebugReportCallbackEXT.html
// for more info
static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanReportCallback(
    VkDebugReportFlagsEXT                       flags,
    VkDebugReportObjectTypeEXT                  objectType,
    uint64_t                                    object,
    size_t                                      location,
    int32_t                                     messageCode,
    const char *pLayerPrefix,
    const char *pMessage,
    void *pUserData
)
{
#if SHOW_ONLY_DEBUG_PRINTF_EXT_MESSAGES
    // NOTE:
    // This message filtering can (and probably should) be done as part of the 
    // intialization in "vkCreateDebugReportCallbackEXT", using the 
    // VkDebugReportCallbackCreateInfoEXT.flags variable. 
    // The initialization in "CreateReportCallback()" does not do any filtering 
    // and it is instead done here to demonstrate one potential usage of the 
    // flags parameter, but is not optimal.
    if (VK_DEBUG_REPORT_INFORMATION_BIT_EXT != flags)
    {
        return VK_FALSE;
    }

    std::string prefix(pMessage);
    if (prefix.find("Validation") == std::string::npos)
    {
        return VK_FALSE;
    }
#endif

    std::cout << "[VULKAN REPORT]: [FLAGS]: " << flags << " [LAYER]: " << pLayerPrefix << " [MESSAGE]: " << pMessage << std::endl;
    return VK_FALSE;
}

// Reads a shader source file (SPIR-V) into a vector<uint32_t>
static std::vector<uint32_t> readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> buffer(fileSize);
    file.seekg(0);
    file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

// Verifies the instance layers in requiredInstanceLayers are available
static bool VerifyInstanceLayers()
{
    if (requiredInstanceLayers.size() == 0)
    {
        return true;
    }
    
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *layerName : requiredInstanceLayers) 
    {
        bool layerFound = false;

        for (const auto &layerProperties : availableLayers) 
        {
            if (strcmp(layerName, layerProperties.layerName) == 0) 
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) 
        {
            return false;
        }
    }

    return true;
}

// Verifies the instance extensions in requiredInstanceExtensions are available
static bool VerifyInstanceExtensions()
{
    if (requiredInstanceExtensions.size() == 0)
    {
        return true;
    }

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    for (const char *extensionName : requiredInstanceExtensions)
    {
        bool layerFound = false;

        for (const auto &extensionProperties : availableExtensions)
        {
            if (strcmp(extensionName, extensionProperties.extensionName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

// Creates a Vulkan instance (without a window)
static VkResult CreateHeadlessVulkanInstance(VkInstance &instance)
{
    VkApplicationInfo applicationInfo{};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext = 0;
    applicationInfo.pApplicationName = "VKComputeSample";
    applicationInfo.applicationVersion = 0;
    applicationInfo.pEngineName = "";
    applicationInfo.engineVersion = 0;
    applicationInfo.apiVersion = VK_HEADER_VERSION_COMPLETE;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(requiredInstanceLayers.size());
    createInfo.ppEnabledLayerNames = requiredInstanceLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredInstanceExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();

    VkValidationFeatureEnableEXT enabled = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
    VkValidationFeaturesEXT features = {};
    features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    features.disabledValidationFeatureCount = 0;
    features.enabledValidationFeatureCount = 1;
    features.pDisabledValidationFeatures = nullptr;
    features.pEnabledValidationFeatures = &enabled;

    createInfo.pNext = &features;
    
    return vkCreateInstance(&createInfo, nullptr, &instance);
}

// Creates a Vulkan Debug Messenger that receives all messages
static VkResult CreateDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT *debugMessenger)
{
    if (nullptr == instance || nullptr == debugMessenger)
    {
        return VK_ERROR_UNKNOWN;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = VulkanDebugCallback;
    createInfo.pUserData = nullptr;

    // The function must by loaded dynamically by name
    auto function = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (function != nullptr)
    {
        // call dll function vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, debugMessenger);
        return function(instance, &createInfo, nullptr, debugMessenger);
    }
    else 
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// Destroys a previously created Vulkan Debug Messenger
static void DestroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger)
{
    if (nullptr == instance || nullptr == debugMessenger)
    {
        return;
    }

    auto function = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    // call dll function vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    function(instance, debugMessenger, nullptr);
}

// Creates a Vulkan Report Callback that receives all messages
static VkResult CreateReportCallback(VkInstance instance, VkDebugReportCallbackEXT *reportCallback)
{
    VkDebugReportCallbackCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    createInfo.flags = VK_DEBUG_REPORT_DEBUG_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
    createInfo.pfnCallback = VulkanReportCallback;
    createInfo.pUserData = nullptr;
    createInfo.pNext = nullptr;

    // The function must by loaded dynamically by name
    auto function = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (function != nullptr)
    {
        // call dll function vkCreateDebugReportCallbackEXT(instance, &createInfo, nullptr, reportCallback);
        return function(instance, &createInfo, nullptr, reportCallback);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

// Destroys a previously created Vulkan Report Callback
static void DestroyReportCallback(VkInstance instance, VkDebugReportCallbackEXT reportCallback)
{
    if (nullptr == instance || nullptr == reportCallback)
    {
        return;
    }

    auto function = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    // call dll function vkDestroyDebugReportCallbackEXT(instance, reportCallback, nullptr);
    function(instance, reportCallback, nullptr);
}

// Enumerates available Vulkan devices
static VkResult EnumerateDevices(VkInstance instance, VkPhysicalDevice *&devices, uint32_t &device_count)
{
    VkResult result = VK_SUCCESS;

    result = vkEnumeratePhysicalDevices(instance, &device_count, 0);
    if (VK_SUCCESS != result)
    {
        return result;
    }

    devices = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * device_count);

    return vkEnumeratePhysicalDevices(instance, &device_count, devices);
}

// Gets the best compute queue family index for the compute shaders
static VkResult GetBestComputeQueue(VkPhysicalDevice physicalDevice, uint32_t &queueFamilyIndex)
{
    uint32_t queueFamilyPropertiesCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, 0);

    VkQueueFamilyProperties *const queueFamilyProperties = (VkQueueFamilyProperties *)_malloca(sizeof(VkQueueFamilyProperties) * queueFamilyPropertiesCount);
    if (nullptr == queueFamilyProperties)
    {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertiesCount, queueFamilyProperties);

    // first try and find a queue that has just the compute bit set
    for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++)
    {
        // mask out the sparse binding bit that we aren't caring about (yet!) and the transfer bit
        const VkQueueFlags maskedFlags = (~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT) & queueFamilyProperties[i].queueFlags);

        if (!(VK_QUEUE_GRAPHICS_BIT & maskedFlags) && (VK_QUEUE_COMPUTE_BIT & maskedFlags))
        {
            queueFamilyIndex = i;
            return VK_SUCCESS;
        }
    }

    // lastly get any queue that'll work for us
    for (uint32_t i = 0; i < queueFamilyPropertiesCount; i++)
    {
        // mask out the sparse binding bit that we aren't caring about (yet!) and the transfer bit
        const VkQueueFlags maskedFlags = (~(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_SPARSE_BINDING_BIT) & queueFamilyProperties[i].queueFlags);

        if (VK_QUEUE_COMPUTE_BIT & maskedFlags)
        {
            queueFamilyIndex = i;
            return VK_SUCCESS;
        }
    }

    return VK_ERROR_INITIALIZATION_FAILED;
}

// Creates a Vulkan device
static VkResult CreateDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, VkDevice &device)
{
    const float queuePrioritory = 1.0f;
    VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    deviceQueueCreateInfo.queueCount = 1;
    deviceQueueCreateInfo.pQueuePriorities = &queuePrioritory;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;

    return vkCreateDevice(physicalDevice, &deviceCreateInfo, 0, &device);
}

// Runs a compute shader from the provided shaderCode
// NOTE: This is not a generic function, and only works with the provided shaders.
static VkResult RunComputeShader(VkDevice device, uint32_t queueFamilyIndex, const std::vector<uint32_t> &shaderCode)
{
    VkResult result = VK_ERROR_UNKNOWN;

    VkShaderModuleCreateInfo glslShaderModuleCreateInfo = {};
    glslShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    glslShaderModuleCreateInfo.codeSize = shaderCode.size();
    glslShaderModuleCreateInfo.pCode = shaderCode.data();

    VkShaderModule glslShaderModule = nullptr;
    result = vkCreateShaderModule(device, &glslShaderModuleCreateInfo, 0, &glslShaderModule);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkPipelineLayoutCreateInfo glslPipelineLayoutCreateInfo = {};
    glslPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineLayout glslPipelineLayout = nullptr;
    result = vkCreatePipelineLayout(device, &glslPipelineLayoutCreateInfo, 0, &glslPipelineLayout);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkComputePipelineCreateInfo glslComputePipelineCreateInfo = {};
    glslComputePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    glslComputePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    glslComputePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    glslComputePipelineCreateInfo.stage.module = glslShaderModule;
    glslComputePipelineCreateInfo.stage.pName = "main";
    glslComputePipelineCreateInfo.layout = glslPipelineLayout;

    VkPipeline pipeline = nullptr;
    result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &glslComputePipelineCreateInfo, VK_NULL_HANDLE, &pipeline);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool commandPool;
    result = vkCreateCommandPool(device, &commandPoolCreateInfo, VK_NULL_HANDLE, &commandPool);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = {};
    result = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkCommandBufferBeginInfo commandBufferBeginInfo = {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    result = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdDispatch(commandBuffer, shader_local_size_x, 1, 1);
    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkQueue queue = {};
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    result = vkQueueSubmit(queue, 1, &submitInfo, 0);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    result = vkQueueWaitIdle(queue);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(device, commandPool, NULL);
    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, glslPipelineLayout, NULL);
    vkDestroyShaderModule(device, glslShaderModule, NULL);

    return result;
}

int main()
{
    // Vulkan setup

    bool instanceLayersPresent = VerifyInstanceLayers();
    bool instanceExtensionsPresent = VerifyInstanceExtensions();

    if (!instanceLayersPresent || !instanceExtensionsPresent)
    {
        return 1;
    }

    VkInstance instance = {};
    EXIT_ON_BAD_RESULT(CreateHeadlessVulkanInstance(instance));

    VkDebugUtilsMessengerEXT debugMessenger = {};
    EXIT_ON_BAD_RESULT(CreateDebugMessenger(instance, &debugMessenger));

    VkDebugReportCallbackEXT reportCallback = {};
    EXIT_ON_BAD_RESULT(CreateReportCallback(instance, &reportCallback));

    VkPhysicalDevice *physicalDevices = nullptr;
    uint32_t physicalDeviceCount = 0;
    EXIT_ON_BAD_RESULT(EnumerateDevices(instance, physicalDevices, physicalDeviceCount));

    uint32_t queueFamilyIndex = 0;
    EXIT_ON_BAD_RESULT(GetBestComputeQueue(physicalDevices[0], queueFamilyIndex));

    VkDevice device = {};
    EXIT_ON_BAD_RESULT(CreateDevice(physicalDevices[0], queueFamilyIndex, device));

    // GLSL Shader setup and run
    auto glslShaderCode = readFile("GLSLComputeShader.comp.spv");
    EXIT_ON_BAD_RESULT(RunComputeShader(device, queueFamilyIndex, glslShaderCode));

    // HLSL Shader setup and run
    auto hlslShaderCode = readFile("HLSLComputeShader.comp.spv");
    EXIT_ON_BAD_RESULT(RunComputeShader(device, queueFamilyIndex, hlslShaderCode));

    // Vulkan cleanup

    vkDestroyDevice(device, NULL);

    DestroyDebugMessenger(instance, debugMessenger);
    DestroyReportCallback(instance, reportCallback);

    vkDestroyInstance(instance, nullptr);

    return 0;
}

#undef EXIT_ON_BAD_RESULT
