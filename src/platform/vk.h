#if !defined(VK_H)

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

struct VulkanBackend
{
	bool32 isInitialized;
	int32 frameNumber;
	VkExtent2D windowExtent;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
	
	VkSwapchainKHR swapchain;
	VkImage* swapchainImageHandles;
	VkSemaphore imageAvailableSemaphore;
	VkFence imageAvailableFence;
	VkSemaphore renderFinishedSemaphore;
	VkFence syncHostWithDeviceFence;
	
	VkQueue commandQueue;
	uint32 commandQueueCount;
};

VulkanBackend
initializeVulkan(uint32 requestedExtensionCount, const char **requestedInstanceExtensions, uint32 enabledLayerCount, const char **enabledLayers, uint32 deviceExtensionCount, const char **requestedDeviceExtensions, RenderWindowCallback *windowCreationCallback)
{
	const VkAllocationCallbacks* pAllocator = nullptr;
	
	VulkanBackend backend = {};
	backend.isInitialized = true;
	backend.instance = {};
	backend.physicalDevice = {};
	
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.apiVersion = VK_API_VERSION_1_2;
	
	VkInstanceCreateInfo instCreateInfo = {};
	instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instCreateInfo.pApplicationInfo = &applicationInfo;
	instCreateInfo.enabledExtensionCount = requestedExtensionCount;
	instCreateInfo.ppEnabledExtensionNames = requestedInstanceExtensions;
	instCreateInfo.enabledLayerCount = enabledLayerCount;
	instCreateInfo.ppEnabledLayerNames = enabledLayers;
	
	VkResult instResult = vkCreateInstance(&instCreateInfo, pAllocator, &backend.instance);
	const char *out = string_VkResult(instResult);
	printf("created Instance: %s\n", out);
	
	VkResult createSurfaceResult = windowCreationCallback(&backend.instance, pAllocator, &backend.surface);
	printf("created surface: %s\n", string_VkResult(createSurfaceResult));
	
	#if 0
	VkDebugUtilsMessengerCreateInfoEXT debugMessageCreateInfo;
	debugMessageCreateInfo.sType = x;
	vkCreateDebugUtilsMessengerEXT(backend.instance, &debugMessageCreateInfo, pAllocator, x);
	#endif 
	
	uint32 pDeviceCount;
	vkEnumeratePhysicalDevices(backend.instance, &pDeviceCount, nullptr);
	ASSERT(pDeviceCount > 0);
	
	VkPhysicalDevice* physicalDevices = new VkPhysicalDevice[pDeviceCount];
	vkEnumeratePhysicalDevices(backend.instance, &pDeviceCount, physicalDevices);
	
	backend.physicalDevice = physicalDevices[0];
	for(uint32 i = 0; i < pDeviceCount; i++)
	{
		VkPhysicalDevice pDevice = physicalDevices[i];
		VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(pDevice, &deviceProperties);
		//TODO(James) what extensions would we ever care about?
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			backend.physicalDevice = pDevice;
			break;
		}
	}
	
	real32 priority = 1.0f;
	VkDeviceQueueCreateInfo lDeviceQueueCreateInfo = {};
	lDeviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	// TODO(James): dawg they always have the graphics one at index 0, but to make this good we ought to query
	lDeviceQueueCreateInfo.queueFamilyIndex = 0;
	lDeviceQueueCreateInfo.queueCount = 1;
	lDeviceQueueCreateInfo.pQueuePriorities = &priority;
	// TODO(James): is there a query capabilities sort of call to verify this?
	backend.commandQueueCount = lDeviceQueueCreateInfo.queueCount;
	
	VkDeviceCreateInfo lDeviceCreateInfo = {};
	lDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	lDeviceCreateInfo.queueCreateInfoCount = 1;
	lDeviceCreateInfo.pQueueCreateInfos = &lDeviceQueueCreateInfo;
	lDeviceCreateInfo.enabledExtensionCount = deviceExtensionCount;
	lDeviceCreateInfo.ppEnabledExtensionNames = requestedDeviceExtensions;
	
	VkResult lDeviceResult = vkCreateDevice(backend.physicalDevice, &lDeviceCreateInfo, pAllocator, &backend.logicalDevice); 
	
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkResult surfaceCapabilityPoll = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(backend.physicalDevice, backend.surface, &surfaceCapabilities);
	printf("polled surface capability: %s\n", string_VkResult(surfaceCapabilityPoll));
	backend.windowExtent = surfaceCapabilities.currentExtent;
	
	uint32 surfaceFormatCount;
	VkSurfaceFormatKHR chosenSurfaceFormat;
	vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, backend.surface, &surfaceFormatCount, nullptr);
	VkSurfaceFormatKHR* surfaceFormats = new VkSurfaceFormatKHR[surfaceFormatCount];
	vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, backend.surface, &surfaceFormatCount, surfaceFormats);
	chosenSurfaceFormat = surfaceFormats[0];
	VkFormat preferredImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	VkColorSpaceKHR preferredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	for(uint32 i = 0; i < surfaceFormatCount; i++)
	{
		if(surfaceFormats[i].format == preferredImageFormat && surfaceFormats[i].colorSpace == preferredColorSpace)
		{
			chosenSurfaceFormat = surfaceFormats[i];
			break;
		}
	}
	
	#if 0
	// this stuff requires a swapchainMaintenance1 feature that isn't always available so fml
	VkSwapchainPresentScalingCreateInfoKHR presentScalingCreateInfo = {};
	presentScalingCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_KHR;
	presentScalingCreateInfo.scalingBehavior = VK_PRESENT_SCALING_ASPECT_RATIO_STRETCH_BIT_KHR ;
	presentScalingCreateInfo.presentGravityX = 0;
	presentScalingCreateInfo.presentGravityY = 0;
	#endif
	
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	//swapchainCreateInfo.pNext = &presentScalingCreateInfo;
	swapchainCreateInfo.surface = backend.surface;
	swapchainCreateInfo.minImageCount = 2;
	swapchainCreateInfo.imageFormat = chosenSurfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = chosenSurfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = backend.windowExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	// we're not using VK_SHARING_MODE_CONCURRENT so these queue family settings doesn't matter
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = VK_NULL_HANDLE;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	// If I ever write a fragment shader with a side effect or want to read back an image before presentation or after reacquisition, then I may want to change this back
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	VkResult createSwapchainResult = vkCreateSwapchainKHR(backend.logicalDevice, &swapchainCreateInfo, pAllocator, &backend.swapchain);
	printf("created swapchain: %s\n", string_VkResult(createSwapchainResult));
	
	uint32 swapchainImageCount;
	vkGetSwapchainImagesKHR(backend.logicalDevice, backend.swapchain, &swapchainImageCount, nullptr);
	backend.swapchainImageHandles = new VkImage[swapchainImageCount];
	vkGetSwapchainImagesKHR(backend.logicalDevice, backend.swapchain, &swapchainImageCount, backend.swapchainImageHandles);
	
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.flags = 0; //not a timeline semaphoreCreateInfo
	semaphoreCreateInfo.pNext = nullptr;
	VkResult semaphoreCreated = vkCreateSemaphore(backend.logicalDevice, &semaphoreCreateInfo, pAllocator, &backend.imageAvailableSemaphore);
	printf("created swapchain semaphore: %s\n", string_VkResult(semaphoreCreated));
	
	
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0; // don't create it signalled
	VkResult fenceCreated = vkCreateFence(backend.logicalDevice, &fenceCreateInfo, pAllocator, &backend.imageAvailableFence);
	printf("created swapchain fence: %s\n", string_VkResult(fenceCreated));
	
	// reusing semaphore create info and VkResult
	semaphoreCreated = vkCreateSemaphore(backend.logicalDevice, &semaphoreCreateInfo, pAllocator, &backend.renderFinishedSemaphore);
	printf("created render semaphore: %s\n", string_VkResult(semaphoreCreated));
	
	// reusing fence create info and VkResult
	fenceCreated = vkCreateFence(backend.logicalDevice, &fenceCreateInfo, pAllocator, &backend.syncHostWithDeviceFence);
	printf("created host/device sync fence: %s\n", string_VkResult(fenceCreated));
	
	
	VkImageCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	createInfo.extent.width = 512;
	createInfo.extent.height = 512;
	createInfo.extent.depth = 1;
	createInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.arrayLayers = 1;
	createInfo.mipLevels = 1;
	createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	
	VkImage image;
	VkResult result = vkCreateImage(backend.logicalDevice, &createInfo, pAllocator, &image);
	printf("result: %s\n", string_VkResult(result));
	
	
	return backend;
}

void
cleanupVulkan(VulkanBackend* backend)
{
	backend->isInitialized = false;
}

void
vulkanDraw(VulkanBackend* backend)
{
	uint32 imageIndex;
	vkAcquireNextImageKHR(backend->logicalDevice, backend->swapchain, UINT64_MAX, backend->imageAvailableSemaphore, backend->imageAvailableFence, &imageIndex);
	
	backend->swapchainImageHandles[imageIndex];
	
	
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 0;
	submitInfo.pCommandBuffers = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &backend->imageAvailableSemaphore;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &backend->renderFinishedSemaphore;
	
	vkQueueSubmit(backend->commandQueue, 1, &submitInfo, backend->syncHostWithDeviceFence);
}

#define VK_H
#endif
    