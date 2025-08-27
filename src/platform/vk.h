#if !defined(VK_H)
#include <vector>
#include <span>
#include <deque>
#include <functional>
#include <fstream>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#define VMA_VULKAN_VERSION 1003000
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            printf("Detected Vulkan error: %s\n", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

#include "vk_utility.h"



struct VulkanBackend;
typedef struct DeletionQueue
{
	std::deque<std::function<void(VulkanBackend *)>> deleteFunctions;
	void pushFunction(std::function<void(VulkanBackend *)>&& function)
	{
		deleteFunctions.push_back(function);
	}
	
	void flush(VulkanBackend *backend)
	{
		for(auto it = deleteFunctions.rbegin(); it != deleteFunctions.rend(); it++)
		{
			(*it)(backend);
		}
		deleteFunctions.clear();
	}
} DeletionQueue;

struct FrameData
{
	VkCommandPool 	commandPool;
	VkCommandBuffer graphicsCommandBuffer;
	DeletionQueue 	deletionQueue;
};

struct AllocatedImage
{
	VkImage 		imageHandle;
	VkImageView 	imageView;
	VmaAllocation 	allocation;
	VkExtent3D 		imageExtent;
	VkFormat 		imageFormat;
};

struct VulkanBackend
{
	bool32 				isInitialized;
	uint32 				frameNumber;
	VkExtent2D 			windowExtent;
	VkInstance 			instance;
	VkSurfaceKHR 		surface;
	VkSurfaceFormatKHR 	chosenSurfaceFormat;
	VkPhysicalDevice 	physicalDevice;
	VkDevice 			logicalDevice;
	
	VkSwapchainKHR 	swapchain;
	uint32 			swapchainImageCount;
	VkImage* 		swapchainImageHandles;
	VkImageView* 	swapchainImageViews;
	VkSemaphore 	imageAvailableSemaphore;
	VkFence 		imageAvailableFence;
	VkSemaphore* 	renderFinishedSemaphores; // one for each swapchainImage 
	VkFence 		syncHostWithDeviceFence;
	VkFence			immFence;
	
	VkQueue graphicsQueue;
	uint32 	graphicsQueueCount;
	uint32 	graphicsQueueFamily;
	VkCommandBufferBeginInfo commandBufferBeginInfo;
	
	vk_util::DescriptorAllocator 	globalDescriptorAllocator;
	VkDescriptorSet 				drawImageDescriptorSet;
	VkDescriptorSetLayout 			drawImageDescriptorSetLayout;
	
	VkPipeline 				gradientPipeline;
	VkPipelineLayout		gradientPipelineLayout;
	
	FrameData 		frameData;
	VkCommandBuffer	immCommandBuffer;
	VkCommandPool	immCommandPool;
	DeletionQueue 	vulkanDeletionQueue;
	
	VmaAllocator 	vAllocator;
	
	AllocatedImage 	drawImage;
	VkExtent2D		drawExtent;
	
	//fucking around
	VkImage testImage;
};

VulkanBackend
initializeVulkan(uint32 requestedExtensionCount, const char **requestedInstanceExtensions, uint32 enabledLayerCount, const char **enabledLayers, uint32 deviceExtensionCount, const char **requestedDeviceExtensions, RenderWindowCallback *windowCreationCallback, const VkAllocationCallbacks* pAllocator)
{
	
	VulkanBackend backend = {};
	backend.isInitialized = true;
	backend.instance = {};
	backend.physicalDevice = {};
	
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.apiVersion = VK_API_VERSION_1_3;
	
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
	delete[] physicalDevices;
	
	real32 priority = 1.0f;
	VkDeviceQueueCreateInfo lDeviceQueueCreateInfo = {};
	lDeviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	// TODO(James): dawg they always have the graphics one at index 0, but to make this good we ought to query
	lDeviceQueueCreateInfo.queueFamilyIndex = 0; // hardcoded
	lDeviceQueueCreateInfo.queueCount = 1; // hardcoded
	lDeviceQueueCreateInfo.pQueuePriorities = &priority;
	
	VkPhysicalDeviceSynchronization2Features synch2Features = {};
	synch2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	synch2Features.pNext = nullptr;
	synch2Features.synchronization2 = VK_TRUE;
	
	VkDeviceCreateInfo lDeviceCreateInfo = {};
	lDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	lDeviceCreateInfo.pNext = &synch2Features;
	lDeviceCreateInfo.queueCreateInfoCount = 1;
	lDeviceCreateInfo.pQueueCreateInfos = &lDeviceQueueCreateInfo;
	lDeviceCreateInfo.enabledExtensionCount = deviceExtensionCount;
	lDeviceCreateInfo.ppEnabledExtensionNames = requestedDeviceExtensions;
	
	VkResult lDeviceResult = vkCreateDevice(backend.physicalDevice, &lDeviceCreateInfo, pAllocator, &backend.logicalDevice); 
	printf("created logical device: %s\n", string_VkResult(lDeviceResult));
	
	// Since we're using a single resetable command buffer, this can be defined once
	backend.commandBufferBeginInfo = {};
	backend.commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	backend.commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	
	// TODO(James): is there a query capabilities sort of call to verify this?
	backend.graphicsQueueFamily = lDeviceQueueCreateInfo.queueFamilyIndex;
	backend.graphicsQueueCount = lDeviceQueueCreateInfo.queueCount;
	vkGetDeviceQueue(backend.logicalDevice, backend.graphicsQueueFamily, 0, &backend.graphicsQueue);
	
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkResult surfaceCapabilityPoll = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(backend.physicalDevice, backend.surface, &surfaceCapabilities);
	printf("polled surface capability: %s\n", string_VkResult(surfaceCapabilityPoll));
	backend.windowExtent = surfaceCapabilities.currentExtent;
	
	uint32 surfaceFormatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, backend.surface, &surfaceFormatCount, nullptr);
	VkSurfaceFormatKHR* surfaceFormats = new VkSurfaceFormatKHR[surfaceFormatCount];
	vkGetPhysicalDeviceSurfaceFormatsKHR(backend.physicalDevice, backend.surface, &surfaceFormatCount, surfaceFormats);
	backend.chosenSurfaceFormat = surfaceFormats[0];
	VkFormat preferredImageFormat = VK_FORMAT_R8G8B8A8_SRGB; // hardcoded
	VkColorSpaceKHR preferredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // hardcoded
	for(uint32 i = 0; i < surfaceFormatCount; i++)
	{
		if(surfaceFormats[i].format == preferredImageFormat && surfaceFormats[i].colorSpace == preferredColorSpace)
		{
			backend.chosenSurfaceFormat = surfaceFormats[i];
			break;
		}
	}
	delete[] surfaceFormats;
	
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = backend.surface;
	swapchainCreateInfo.minImageCount = 2;
	swapchainCreateInfo.imageFormat = backend.chosenSurfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = backend.chosenSurfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = backend.windowExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
	
	vkGetSwapchainImagesKHR(backend.logicalDevice, backend.swapchain, &backend.swapchainImageCount, nullptr);
	backend.swapchainImageHandles = new VkImage[backend.swapchainImageCount];
	vkGetSwapchainImagesKHR(backend.logicalDevice, backend.swapchain, &backend.swapchainImageCount, backend.swapchainImageHandles);
	
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.flags = 0; //not a timeline semaphoreCreateInfo
	semaphoreCreateInfo.pNext = nullptr;
	VkResult semaphoreCreated = vkCreateSemaphore(backend.logicalDevice, &semaphoreCreateInfo, pAllocator, &backend.imageAvailableSemaphore);
	printf("created swapchain semaphore: %s\n", string_VkResult(semaphoreCreated));
	
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VkResult fenceCreated = vkCreateFence(backend.logicalDevice, &fenceCreateInfo, pAllocator, &backend.imageAvailableFence);
	printf("created swapchain fence: %s\n", string_VkResult(fenceCreated));
	
	backend.swapchainImageViews = new VkImageView[backend.swapchainImageCount];
	backend.renderFinishedSemaphores = new VkSemaphore[backend.swapchainImageCount];
	for(uint32 i = 0; i< backend.swapchainImageCount; i++)
	{
		VkImageViewCreateInfo swapchainImageViewCreateInfo = {};
		swapchainImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		swapchainImageViewCreateInfo.flags = 0;
		swapchainImageViewCreateInfo.image = backend.swapchainImageHandles[i];
		swapchainImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		swapchainImageViewCreateInfo.format = backend.chosenSurfaceFormat.format;
		swapchainImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		swapchainImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		swapchainImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		swapchainImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		swapchainImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		swapchainImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		swapchainImageViewCreateInfo.subresourceRange.levelCount = 1;
		swapchainImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		swapchainImageViewCreateInfo.subresourceRange.layerCount = 1;
		
		vkCreateImageView(backend.logicalDevice, &swapchainImageViewCreateInfo, pAllocator, &backend.swapchainImageViews[i]);
		
		// reusing semaphore create info and VkResult
		semaphoreCreated = vkCreateSemaphore(backend.logicalDevice, &semaphoreCreateInfo, pAllocator, &backend.renderFinishedSemaphores[i]);
		printf("created render semaphore: %s\n", string_VkResult(semaphoreCreated));
	}
	
	// reusing fence create info and VkResult
	fenceCreated = vkCreateFence(backend.logicalDevice, &fenceCreateInfo, pAllocator, &backend.syncHostWithDeviceFence);
	printf("created host/device sync fence: %s\n", string_VkResult(fenceCreated));
	fenceCreated = vkCreateFence(backend.logicalDevice, &fenceCreateInfo, pAllocator, &backend.immFence);
	printf("created imm fence: %s\n", string_VkResult(fenceCreated));
	backend.vulkanDeletionQueue.pushFunction([=](VulkanBackend* be) {
		vkDestroyFence(be->logicalDevice, be->immFence, nullptr); //pAllocator
	});
	
	// Initialize allocator for images and buffers and what have you
	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.physicalDevice = backend.physicalDevice;
	allocatorCreateInfo.device = backend.logicalDevice;
	allocatorCreateInfo.instance = backend.instance;
	allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorCreateInfo, &backend.vAllocator);
	backend.vulkanDeletionQueue.pushFunction([&](VulkanBackend* be) {
		vmaDestroyAllocator(be->vAllocator);
	});
	
	// initialize the draw image used as target before swapchain
	VkExtent3D drawImageExtent = {
		backend.windowExtent.width,
		backend.windowExtent.height,
		1
	};
	
	backend.drawImage.imageExtent = drawImageExtent;
	backend.drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT; // hard coded
	VkImageUsageFlags drawImageUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
											VK_IMAGE_USAGE_TRANSFER_DST_BIT |
											VK_IMAGE_USAGE_STORAGE_BIT |
											VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
											
	VkImageCreateInfo drawImageCreateInfo = {};
	drawImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	drawImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	drawImageCreateInfo.format = backend.drawImage.imageFormat;
	drawImageCreateInfo.extent = drawImageExtent;
	drawImageCreateInfo.usage = drawImageUsageFlags;
	drawImageCreateInfo.arrayLayers = 1; //hardcoded
	drawImageCreateInfo.mipLevels = 1; // hardcoded
	drawImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT; // hardcoded
	
	VmaAllocationCreateInfo allocationCreateInfo = {};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	VkResult drawImageCreated = vmaCreateImage(backend.vAllocator, &drawImageCreateInfo, &allocationCreateInfo, &backend.drawImage.imageHandle, &backend.drawImage.allocation, nullptr);
	printf("draw image created: %s\n", string_VkResult(drawImageCreated));
	
	VkImageViewCreateInfo drawImageViewCreateInfo = {};
	drawImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	drawImageViewCreateInfo.pNext = nullptr;
	drawImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	drawImageViewCreateInfo.image = backend.drawImage.imageHandle;
	drawImageViewCreateInfo.format = backend.drawImage.imageFormat;
	drawImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	drawImageViewCreateInfo.subresourceRange.levelCount = 1;
	drawImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	drawImageViewCreateInfo.subresourceRange.layerCount = 1;
	drawImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	
	VkResult drawImageViewCreated = vkCreateImageView(backend.logicalDevice, &drawImageViewCreateInfo, pAllocator, &backend.drawImage.imageView);
	printf("draw image view created: %s\n", string_VkResult(drawImageViewCreated));
	backend.vulkanDeletionQueue.pushFunction([&](VulkanBackend *be) {
		assert(pAllocator == nullptr);
		vkDestroyImageView(be->logicalDevice, be->drawImage.imageView, pAllocator);
		vmaDestroyImage(be->vAllocator, be->drawImage.imageHandle, be->drawImage.allocation);
	});
	
	// initialize descriptors for the draw image
	std::vector<vk_util::DescriptorAllocator::PoolSizeRatio> sizes = 
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};
	backend.globalDescriptorAllocator.initPool(backend.logicalDevice, 10, sizes, pAllocator);
	{
		vk_util::DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		backend.drawImageDescriptorSetLayout = builder.build(backend.logicalDevice, VK_SHADER_STAGE_COMPUTE_BIT, pAllocator);
	}
	backend.drawImageDescriptorSet = backend.globalDescriptorAllocator.allocate(backend.logicalDevice, backend.drawImageDescriptorSetLayout);
	
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = backend.drawImage.imageView;
	
	VkWriteDescriptorSet drawImageWrite = {};
	drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	drawImageWrite.pNext = nullptr;
	drawImageWrite.dstBinding = 0;
	drawImageWrite.dstSet = backend.drawImageDescriptorSet;
	drawImageWrite.descriptorCount = 1;
	drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	drawImageWrite.pImageInfo = &imageInfo;
	
	vkUpdateDescriptorSets(backend.logicalDevice, 1, &drawImageWrite, 0, nullptr);
	
	backend.vulkanDeletionQueue.pushFunction([&](VulkanBackend *be) {
		assert(pAllocator == nullptr);
		be->globalDescriptorAllocator.destroyPool(be->logicalDevice, pAllocator);
		vkDestroyDescriptorSetLayout(be->logicalDevice, be->drawImageDescriptorSetLayout, pAllocator);
	});
	
	// initialize pipelines
	VkPipelineLayoutCreateInfo computeLayoutCreateInfo = {};
	computeLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayoutCreateInfo.pNext = nullptr;
	computeLayoutCreateInfo.pSetLayouts = &backend.drawImageDescriptorSetLayout;
	computeLayoutCreateInfo.setLayoutCount = 1;
	
	VkResult computePipelineLayoutCreated = vkCreatePipelineLayout(backend.logicalDevice, &computeLayoutCreateInfo, pAllocator, &backend.gradientPipelineLayout);
	printf("compute pipeline layout created: %s\n", string_VkResult(computePipelineLayoutCreated));
	
	VkShaderModule computeDrawShader;
	if (!vk_util::loadShaderModule("..\\shaders\\gradient.comp.spv", backend.logicalDevice, &computeDrawShader))
	{
		printf("failed to load shader");
	}
	VkPipelineShaderStageCreateInfo stageCreateInfo = {};
	stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCreateInfo.pNext = nullptr;
	stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageCreateInfo.module = computeDrawShader;
	stageCreateInfo.pName = "main";
	
	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = backend.gradientPipelineLayout;
	computePipelineCreateInfo.stage = stageCreateInfo;
	
	VkResult computePipelineCreated = vkCreateComputePipelines(backend.logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, pAllocator, &backend.gradientPipeline);
	printf("compute pipeline created: %s\n", string_VkResult(computePipelineCreated));
	
	vkDestroyShaderModule(backend.logicalDevice, computeDrawShader, pAllocator);
	backend.vulkanDeletionQueue.pushFunction([&](VulkanBackend *be){
		assert(pAllocator == nullptr);
		vkDestroyPipelineLayout(be->logicalDevice, be->gradientPipelineLayout, pAllocator);
		vkDestroyPipeline(be->logicalDevice, be->gradientPipeline, pAllocator);
	});
	
	
	// fucking around
	#if 0
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
	
	
	VkResult result = vkCreateImage(backend.logicalDevice, &createInfo, pAllocator, &backend.testImage);
	printf("result: %s\n", string_VkResult(result));
	backend.vulkanDeletionQueue.pushFunction([&](VulkanBackend* be){
		vkDestroyImage(be->logicalDevice, be->testImage, pAllocator);
	});
		
	#endif
	
	return backend;
}

void initializeImgui(VulkanBackend *backend)
{
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversized, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize poolSizes[] =
	{ 
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = (uint32)std::size(poolSizes);
	poolInfo.pPoolSizes = poolSizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(backend->logicalDevice, &poolInfo, nullptr, &imguiPool));

	// 2: initialize imgui library for Vulkan assuming platform layer already called
	// CreateContext() and the platform imp init

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = backend->instance;
	init_info.PhysicalDevice = backend->physicalDevice;
	init_info.Device = backend->logicalDevice;
	init_info.Queue = backend->graphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &backend->chosenSurfaceFormat.format;
	

	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	// add the destroy the imgui created structures
	backend->vulkanDeletionQueue.pushFunction([=](VulkanBackend* be) {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(be->logicalDevice, imguiPool, nullptr);
	});
}

void
cleanupVulkan(VulkanBackend* backend, VkAllocationCallbacks* pAllocator)
{
	
	// GPU cleanup section
	vkDeviceWaitIdle(backend->logicalDevice);
	
	backend->vulkanDeletionQueue.flush(backend);
	backend->frameData.deletionQueue.flush(backend);
	
	// TODO(James) vkFreeMemory when we have VkDeviceMemory's
	// TODO(James) vkDestroyBuffer when we have them
	// TODO(James) vkDestroyPipeline when we have them
	// TODO(James) vkDestroyPipelineLayout when we have them
	// TODO(James) vkDestroyPipelineLayout when we have them
	// TODO(James) vkDestroyDescriptorSetLayout when we have them
	// TODO(James) vkDestroyDescriptorPool when we have them
	vkDestroyCommandPool(backend->logicalDevice, backend->frameData.commandPool, pAllocator);
	
	for(uint32 i = 0; i < backend->swapchainImageCount; i++)
	{
		vkDestroyImageView(backend->logicalDevice, backend->swapchainImageViews[i], pAllocator);
		vkDestroySemaphore(backend->logicalDevice, backend->renderFinishedSemaphores[i], pAllocator);
	}
	
	vkDestroySwapchainKHR(backend->logicalDevice, backend->swapchain, pAllocator);
	
	vkDestroyFence(backend->logicalDevice, backend->imageAvailableFence, pAllocator);
	vkDestroyFence(backend->logicalDevice, backend->syncHostWithDeviceFence, pAllocator);
	vkDestroySemaphore(backend->logicalDevice, backend->imageAvailableSemaphore, pAllocator);
	
	
	vkDestroyDevice(backend->logicalDevice, pAllocator);
	vkDestroySurfaceKHR(backend->instance, backend->surface, pAllocator);
	// TODO(James): vkDestroyDebugUtilsMessengerEXT when we make one
	vkDestroyInstance(backend->instance, pAllocator);
	
	// end GPU cleanup section
	
	// CPU cleanup section
	backend->isInitialized = false;
	delete[] backend->swapchainImageHandles;
	delete[] backend->swapchainImageViews;
	delete[] backend->renderFinishedSemaphores;
	
	// end CPU cleanup section
}

void
vulkanBuildCommandPool(VulkanBackend *backend, VkAllocationCallbacks* pAllocator)
{
	// making a command pool that gives us resetable command buffers for using the whole dang time
	VkResult poolCreated = vk_util::createCommandPool(backend->logicalDevice, backend->graphicsQueueFamily, pAllocator, &backend->frameData.commandPool);
	printf("created command pool: %s\n", string_VkResult(poolCreated));

	VkResult commandBufferAllocated = vk_util::allocateCommandBuffer(backend->logicalDevice, backend->frameData.commandPool, 1, &backend->frameData.graphicsCommandBuffer);
	printf("allocated graphics command buffer: %s\n", string_VkResult(commandBufferAllocated));
	
	// creating another pool for immediate mode commands
	VkResult immPoolCreated = vk_util::createCommandPool(backend->logicalDevice, backend->graphicsQueueFamily, pAllocator, &backend->immCommandPool);
	printf("created imm command pool: %s\n", string_VkResult(immPoolCreated));
	
	VkResult immCommandBufferAllocated = vk_util::allocateCommandBuffer(backend->logicalDevice, backend->immCommandPool, 1, &backend->immCommandBuffer);
	printf("allocated imm command buffer: %s\n", string_VkResult(immCommandBufferAllocated));
	
	backend->vulkanDeletionQueue.pushFunction([&](VulkanBackend* be) {
		vkDestroyCommandPool(be->logicalDevice, be->immCommandPool, nullptr); //pAllocator
	});
	
}

void
vulkanBeginCommandBuffer(VulkanBackend *backend)
{
	VkResult resetCommandBuffer = vkResetCommandBuffer(backend->frameData.graphicsCommandBuffer, 0);
	//printf("reset command buffer: %s\n", string_VkResult(resetCommandBuffer));
	VkResult beganCommandBuffer = vkBeginCommandBuffer(backend->frameData.graphicsCommandBuffer, &backend->commandBufferBeginInfo);
	//printf("began command buffer: %s\n", string_VkResult(beganCommandBuffer));
}

void vulkanTransitionImage(VulkanBackend* backend, VkImage image, VkImageAspectFlags aspectFlags, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageSubresourceRange imageSubresourceRange {};
    imageSubresourceRange.aspectMask = aspectFlags;
    imageSubresourceRange.baseMipLevel = 0;
    imageSubresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageSubresourceRange.baseArrayLayer = 0;
    imageSubresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	
	VkImageMemoryBarrier2 imageBarrier = {};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.pNext = nullptr;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
	imageBarrier.oldLayout = oldLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.subresourceRange = imageSubresourceRange;
	imageBarrier.image = image;
	
	VkDependencyInfo dependencyInfo = {};
	dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.pNext = nullptr;
	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers = &imageBarrier;
	
	vkCmdPipelineBarrier2(backend->frameData.graphicsCommandBuffer, &dependencyInfo);
}

void vulkanCopyImage(VulkanBackend* backend, VkImage source, VkImage dest, VkExtent2D sourceSize, VkExtent2D destSize)
{
	VkImageBlit2 blitRegion = {};
	blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blitRegion.pNext = nullptr;
	
	blitRegion.srcOffsets[1].x = sourceSize.width;
	blitRegion.srcOffsets[1].y = sourceSize.height;
	blitRegion.srcOffsets[1].z = 1;
	blitRegion.dstOffsets[1].x = destSize.width;
	blitRegion.dstOffsets[1].y = destSize.height;
	blitRegion.dstOffsets[1].z = 1;
	
	blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.srcSubresource.baseArrayLayer = 0;
	blitRegion.srcSubresource.layerCount = 1;
	blitRegion.srcSubresource.mipLevel = 0;
	blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blitRegion.dstSubresource.baseArrayLayer = 0;
	blitRegion.dstSubresource.layerCount = 1;
	blitRegion.dstSubresource.mipLevel = 0;
	
	VkBlitImageInfo2 blitInfo = {};
	blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blitInfo.pNext = nullptr;
	blitInfo.srcImage = source;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.dstImage = dest;
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.filter = VK_FILTER_LINEAR;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;
	
	vkCmdBlitImage2(backend->frameData.graphicsCommandBuffer, &blitInfo);
}

void
drawImGui(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout layout, VkExtent2D renderExtent)
{
	VkRenderingAttachmentInfo colorAttachment = {};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.pNext = nullptr;
	colorAttachment.imageView = imageView;
	colorAttachment.imageLayout = layout;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	
	VkRenderingInfo renderInfo = {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.renderArea = VkRect2D { VkOffset2D { 0, 0 }, renderExtent };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;
	
	vkCmdBeginRendering(commandBuffer, &renderInfo);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	vkCmdEndRendering(commandBuffer);
}

void
vulkanDraw(VulkanBackend* backend)
{
	
	uint32 imageIndex;
	vkWaitForFences(backend->logicalDevice, 1, &backend->syncHostWithDeviceFence, VK_TRUE, 1000000000);
	vkResetFences(backend->logicalDevice, 1, &backend->syncHostWithDeviceFence);
	
	backend->frameData.deletionQueue.flush(backend);
	
	// acquire image
	vkWaitForFences(backend->logicalDevice, 1, &backend->imageAvailableFence, VK_TRUE, 1000000000);
	vkResetFences(backend->logicalDevice, 1, &backend->imageAvailableFence);
	VkResult imageAcquired = vkAcquireNextImageKHR(backend->logicalDevice, backend->swapchain, UINT64_MAX, backend->imageAvailableSemaphore, backend->imageAvailableFence, &imageIndex);
	//printf("image acquired: %s\n", string_VkResult(imageAcquired));
	
	backend->drawExtent.width = backend->drawImage.imageExtent.width;
	backend->drawExtent.height = backend->drawImage.imageExtent.height;
	vulkanBeginCommandBuffer(backend);
	
	// transition image to be drawable with a pipeline barrier
	VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL;
	vulkanTransitionImage(backend, backend->drawImage.imageHandle, aspectFlags, oldLayout, newLayout);
	
	// translate commands for frame to vk commands
	#if 0
	// lol jk we're just clearing the screen
	real32 flash = abs(sin(backend->frameNumber / 30.0f));
	VkClearColorValue clearValue = { {0.0f, 0.0f, flash, 1.0f} };
	VkImageSubresourceRange clearRange = {};
	clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearRange.baseMipLevel = 0;
    clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
    clearRange.baseArrayLayer = 0;
    clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	vkCmdClearColorImage(backend->frameData.graphicsCommandBuffer, backend->drawImage.imageHandle, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
	#endif
	#if 1
	// lol jk we're just running our little compute shader
	vkCmdBindPipeline(backend->frameData.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, backend->gradientPipeline);
	vkCmdBindDescriptorSets(backend->frameData.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, backend->gradientPipelineLayout, 0, 1, &backend->drawImageDescriptorSet, 0, nullptr);
	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	uint32 groupCountX = std::ceil(backend->drawExtent.width / 16.0);
	uint32 groupCountY = std::ceil(backend->drawExtent.height / 16.0);
	printf("draw extent is %d by %d dispatching in groups of %d by %d\n", backend->drawImage.imageExtent.width, backend->drawImage.imageExtent.height, groupCountX, groupCountY);
	vkCmdDispatch(backend->frameData.graphicsCommandBuffer, groupCountX, groupCountY, 1);
	#endif
	// transfer image from drawImage to the swapchain image
	vulkanTransitionImage(backend, backend->drawImage.imageHandle, aspectFlags, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vulkanTransitionImage(backend, backend->swapchainImageHandles[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vulkanCopyImage(backend, backend->drawImage.imageHandle, backend->swapchainImageHandles[imageIndex], backend->drawExtent, backend->windowExtent);
	
	// make swapchain image layout attachment optimal so we can draw immediate mode commands
	vulkanTransitionImage(backend, backend->swapchainImageHandles[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	
	//draw imgui
	drawImGui(backend->frameData.graphicsCommandBuffer, backend->swapchainImageViews[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, backend->windowExtent);
	
	// make swapchain image presentable with a pipeline barrier
	vulkanTransitionImage(backend, backend->swapchainImageHandles[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	
	// end command buffer
	vkEndCommandBuffer(backend->frameData.graphicsCommandBuffer);
	
	// submit command queue for the frame
	VkSemaphoreSubmitInfo imageAvailableSemaphoreSubmitInfo = {};
	imageAvailableSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	imageAvailableSemaphoreSubmitInfo.pNext = nullptr;
	imageAvailableSemaphoreSubmitInfo.semaphore = backend->imageAvailableSemaphore;
	imageAvailableSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
	imageAvailableSemaphoreSubmitInfo.deviceIndex = 0;
	imageAvailableSemaphoreSubmitInfo.value = 1;
	
	VkSemaphoreSubmitInfo renderFinishedSemaphoreSubmitInfo = {};
	renderFinishedSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	renderFinishedSemaphoreSubmitInfo.pNext = nullptr;
	renderFinishedSemaphoreSubmitInfo.semaphore = backend->renderFinishedSemaphores[imageIndex];
	renderFinishedSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
	renderFinishedSemaphoreSubmitInfo.deviceIndex = 0;
	renderFinishedSemaphoreSubmitInfo.value = 1;
	
	VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.pNext = nullptr;
	commandBufferSubmitInfo.commandBuffer = backend->frameData.graphicsCommandBuffer;
	commandBufferSubmitInfo.deviceMask = 0;
	
	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &imageAvailableSemaphoreSubmitInfo;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &renderFinishedSemaphoreSubmitInfo;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
	
	vkQueueSubmit2(backend->graphicsQueue, 1, &submitInfo, backend->syncHostWithDeviceFence);
	
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &backend->swapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &backend->renderFinishedSemaphores[imageIndex];
	presentInfo.pImageIndices = &imageIndex;
	
	VkResult presented = vkQueuePresentKHR(backend->graphicsQueue, &presentInfo);
	//printf("image presented: %s\n", string_VkResult(presented));
	
	backend->frameNumber++;
}

void vulkanImmediateSubmit(VulkanBackend *backend, std::function<void(VkCommandBuffer cmdBuffer)>&& function)
{
	VK_CHECK(vkResetFences(backend->logicalDevice, 1, &backend->immFence));
	VK_CHECK(vkResetCommandBuffer(backend->immCommandBuffer, 0));
	
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	
	VK_CHECK(vkBeginCommandBuffer(backend->immCommandBuffer, &beginInfo));
	function(backend->immCommandBuffer);
	VK_CHECK(vkEndCommandBuffer(backend->immCommandBuffer));
	
	VkCommandBufferSubmitInfo bufferSubmitInfo = {};
	bufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	bufferSubmitInfo.pNext = nullptr;
	bufferSubmitInfo.commandBuffer = backend->immCommandBuffer;
	bufferSubmitInfo.deviceMask = 0;
	
	// not waiting or signalling any semaphores
	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreInfoCount = 0;
	submitInfo.pWaitSemaphoreInfos = nullptr;
	submitInfo.signalSemaphoreInfoCount = 0;
	submitInfo.pSignalSemaphoreInfos = nullptr;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &bufferSubmitInfo;
	
	VK_CHECK(vkQueueSubmit2(backend->graphicsQueue, 1, &submitInfo, backend->immFence));
	VK_CHECK(vkWaitForFences(backend->logicalDevice, 1, &backend->immFence, true, 9999999999));
}

#define VK_H
#endif
    