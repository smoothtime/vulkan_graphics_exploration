#if !defined(VK_H)
#include <vector>
#include <array>
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
#include "vk_types.h"
#include "vk_utility.h"
#include "vk_loader.h"

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
	
	VkPipelineLayout		gradientPipelineLayout;
	
	FrameData 		frameData;
	VkCommandBuffer	immCommandBuffer;
	VkCommandPool	immCommandPool;
	DeletionQueue 	vulkanDeletionQueue;
	
	VmaAllocator 	vAllocator;
	
	AllocatedImage 	drawImage;
	VkExtent2D		drawExtent2D;
	
	std::vector<ComputeEffect> backgroundEffects;
	uint32 currentBackgroundEffect{0};
	
	VkPipelineLayout	trianglePipelineLayout;
	VkPipeline 			trianglePipeline;
	
	VkPipelineLayout 	triangleMeshPipelineLayout;
	VkPipeline			triangleMeshPipeline;
	
	// Geometry
	GPUMeshBuffers		rectangle;
	// homie do we really want a vector of shared pointers flying around?
	// homie you do not. Let's keep track of these explicitly when out of tutorial land
	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	// fucking around
	VkImage testImage;

	void
	initialize(uint32 requestedExtensionCount, const char **requestedInstanceExtensions, uint32 enabledLayerCount, const char **enabledLayers, uint32 deviceExtensionCount, const char **requestedDeviceExtensions, RenderWindowCallback *windowCreationCallback, const VkAllocationCallbacks* pAllocator)
	{
		isInitialized = true;
		instance = {};
		physicalDevice = {};
	
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
		
		VkResult instResult = vkCreateInstance(&instCreateInfo, pAllocator, &instance);
		const char *out = string_VkResult(instResult);
		printf("created Instance: %s\n", out);
		
		VkResult createSurfaceResult = windowCreationCallback(&instance, pAllocator, &surface);
		printf("created surface: %s\n", string_VkResult(createSurfaceResult));
		
		#if 0
		VkDebugUtilsMessengerCreateInfoEXT debugMessageCreateInfo;
		debugMessageCreateInfo.sType = x;
		vkCreateDebugUtilsMessengerEXT(instance, &debugMessageCreateInfo, pAllocator, x);
		#endif 
		
		uint32 pDeviceCount;
		vkEnumeratePhysicalDevices(instance, &pDeviceCount, nullptr);
		ASSERT(pDeviceCount > 0);
		
		VkPhysicalDevice* physicalDevices = new VkPhysicalDevice[pDeviceCount];
		vkEnumeratePhysicalDevices(instance, &pDeviceCount, physicalDevices);
		
		physicalDevice = physicalDevices[0];
		for(uint32 i = 0; i < pDeviceCount; i++)
		{
			VkPhysicalDevice pDevice = physicalDevices[i];
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(pDevice, &deviceProperties);
			//TODO(James) what extensions would we ever care about?
			if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				physicalDevice = pDevice;
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

		VkPhysicalDeviceBufferDeviceAddressFeatures deviceAddressFeatures = {};
		deviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
		deviceAddressFeatures.pNext = &synch2Features;
		deviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
		
		VkDeviceCreateInfo lDeviceCreateInfo = {};
		lDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		lDeviceCreateInfo.pNext = &deviceAddressFeatures;
		lDeviceCreateInfo.queueCreateInfoCount = 1;
		lDeviceCreateInfo.pQueueCreateInfos = &lDeviceQueueCreateInfo;
		lDeviceCreateInfo.enabledExtensionCount = deviceExtensionCount;
		lDeviceCreateInfo.ppEnabledExtensionNames = requestedDeviceExtensions;
		
		VkResult lDeviceResult = vkCreateDevice(physicalDevice, &lDeviceCreateInfo, pAllocator, &logicalDevice); 
		printf("created logical device: %s\n", string_VkResult(lDeviceResult));
		
		// Since we're using a single resetable command buffer, this can be defined once
		commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		
		// TODO(James): is there a query capabilities sort of call to verify this?
		graphicsQueueFamily = lDeviceQueueCreateInfo.queueFamilyIndex;
		graphicsQueueCount = lDeviceQueueCreateInfo.queueCount;
		vkGetDeviceQueue(logicalDevice, graphicsQueueFamily, 0, &graphicsQueue);
		
		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		VkResult surfaceCapabilityPoll = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
		printf("polled surface capability: %s\n", string_VkResult(surfaceCapabilityPoll));
		windowExtent = surfaceCapabilities.currentExtent;
		
		uint32 surfaceFormatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
		VkSurfaceFormatKHR* surfaceFormats = new VkSurfaceFormatKHR[surfaceFormatCount];
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats);
		chosenSurfaceFormat = surfaceFormats[0];
		VkFormat preferredImageFormat = VK_FORMAT_R8G8B8A8_SRGB; // hardcoded
		VkColorSpaceKHR preferredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // hardcoded
		for(uint32 i = 0; i < surfaceFormatCount; i++)
		{
			if(surfaceFormats[i].format == preferredImageFormat && surfaceFormats[i].colorSpace == preferredColorSpace)
			{
				chosenSurfaceFormat = surfaceFormats[i];
				break;
			}
		}
		delete[] surfaceFormats;
		
		VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.surface = surface;
		swapchainCreateInfo.minImageCount = 2;
		swapchainCreateInfo.imageFormat = chosenSurfaceFormat.format;
		swapchainCreateInfo.imageColorSpace = chosenSurfaceFormat.colorSpace;
		swapchainCreateInfo.imageExtent = windowExtent;
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
		VkResult createSwapchainResult = vkCreateSwapchainKHR(logicalDevice, &swapchainCreateInfo, pAllocator, &swapchain);
		printf("created swapchain: %s\n", string_VkResult(createSwapchainResult));
		
		vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImageCount, nullptr);
		swapchainImageHandles = new VkImage[swapchainImageCount];
		vkGetSwapchainImagesKHR(logicalDevice, swapchain, &swapchainImageCount, swapchainImageHandles);
		
		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.flags = 0; //not a timeline semaphoreCreateInfo
		semaphoreCreateInfo.pNext = nullptr;
		VkResult semaphoreCreated = vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, pAllocator, &imageAvailableSemaphore);
		printf("created swapchain semaphore: %s\n", string_VkResult(semaphoreCreated));
		
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		VkResult fenceCreated = vkCreateFence(logicalDevice, &fenceCreateInfo, pAllocator, &imageAvailableFence);
		printf("created swapchain fence: %s\n", string_VkResult(fenceCreated));
		
		swapchainImageViews = new VkImageView[swapchainImageCount];
		renderFinishedSemaphores = new VkSemaphore[swapchainImageCount];
		for(uint32 i = 0; i< swapchainImageCount; i++)
		{
			VkImageViewCreateInfo swapchainImageViewCreateInfo = {};
			swapchainImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			swapchainImageViewCreateInfo.flags = 0;
			swapchainImageViewCreateInfo.image = swapchainImageHandles[i];
			swapchainImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			swapchainImageViewCreateInfo.format = chosenSurfaceFormat.format;
			swapchainImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			swapchainImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			swapchainImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			swapchainImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			swapchainImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			swapchainImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			swapchainImageViewCreateInfo.subresourceRange.levelCount = 1;
			swapchainImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			swapchainImageViewCreateInfo.subresourceRange.layerCount = 1;
			
			vkCreateImageView(logicalDevice, &swapchainImageViewCreateInfo, pAllocator, &swapchainImageViews[i]);
			
			// reusing semaphore create info and VkResult
			semaphoreCreated = vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, pAllocator, &renderFinishedSemaphores[i]);
			printf("created render semaphore: %s\n", string_VkResult(semaphoreCreated));
		}
		
		// reusing fence create info and VkResult
		fenceCreated = vkCreateFence(logicalDevice, &fenceCreateInfo, pAllocator, &syncHostWithDeviceFence);
		printf("created host/device sync fence: %s\n", string_VkResult(fenceCreated));
		fenceCreated = vkCreateFence(logicalDevice, &fenceCreateInfo, pAllocator, &immFence);
		printf("created imm fence: %s\n", string_VkResult(fenceCreated));
		vulkanDeletionQueue.pushFunction([=](VulkanBackend* be) {
			vkDestroyFence(be->logicalDevice, be->immFence, nullptr); //pAllocator
		});
		
		// Initialize allocator for images and buffers and what have you
		VmaAllocatorCreateInfo allocatorCreateInfo = {};
		allocatorCreateInfo.physicalDevice = physicalDevice;
		allocatorCreateInfo.device = logicalDevice;
		allocatorCreateInfo.instance = instance;
		allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		vmaCreateAllocator(&allocatorCreateInfo, &vAllocator);
		vulkanDeletionQueue.pushFunction([&](VulkanBackend* be) {
			vmaDestroyAllocator(be->vAllocator);
		});
		
		// initialize the draw image used as target before swapchain
		VkExtent3D drawImageExtent = {
			windowExtent.width,
			windowExtent.height,
			1
		};
		
		drawImage.imageExtent = drawImageExtent;
		drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT; // hard coded
		VkImageUsageFlags drawImageUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
												VK_IMAGE_USAGE_TRANSFER_DST_BIT |
												VK_IMAGE_USAGE_STORAGE_BIT |
												VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
												
		VkImageCreateInfo drawImageCreateInfo = {};
		drawImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		drawImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		drawImageCreateInfo.format = drawImage.imageFormat;
		drawImageCreateInfo.extent = drawImageExtent;
		drawImageCreateInfo.usage = drawImageUsageFlags;
		drawImageCreateInfo.arrayLayers = 1; //hardcoded
		drawImageCreateInfo.mipLevels = 1; // hardcoded
		drawImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT; // hardcoded
		
		VmaAllocationCreateInfo allocationCreateInfo = {};
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		
		VkResult drawImageCreated = vmaCreateImage(vAllocator, &drawImageCreateInfo, &allocationCreateInfo, &drawImage.imageHandle, &drawImage.allocation, nullptr);
		printf("draw image created: %s\n", string_VkResult(drawImageCreated));
		
		VkImageViewCreateInfo drawImageViewCreateInfo = {};
		drawImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		drawImageViewCreateInfo.pNext = nullptr;
		drawImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		drawImageViewCreateInfo.image = drawImage.imageHandle;
		drawImageViewCreateInfo.format = drawImage.imageFormat;
		drawImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		drawImageViewCreateInfo.subresourceRange.levelCount = 1;
		drawImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		drawImageViewCreateInfo.subresourceRange.layerCount = 1;
		drawImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		
		VkResult drawImageViewCreated = vkCreateImageView(logicalDevice, &drawImageViewCreateInfo, pAllocator, &drawImage.imageView);
		printf("draw image view created: %s\n", string_VkResult(drawImageViewCreated));
		vulkanDeletionQueue.pushFunction([&](VulkanBackend *be) {
			assert(pAllocator == nullptr);
			vkDestroyImageView(be->logicalDevice, be->drawImage.imageView, pAllocator);
			vmaDestroyImage(be->vAllocator, be->drawImage.imageHandle, be->drawImage.allocation);
		});
		
		// initialize descriptors for the draw image
		std::vector<vk_util::DescriptorAllocator::PoolSizeRatio> sizes = 
		{
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
		};
		globalDescriptorAllocator.initPool(logicalDevice, 10, sizes, pAllocator);
		{
			vk_util::DescriptorLayoutBuilder builder;
			builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
			drawImageDescriptorSetLayout = builder.build(logicalDevice, VK_SHADER_STAGE_COMPUTE_BIT, pAllocator);
		}
		drawImageDescriptorSet = globalDescriptorAllocator.allocate(logicalDevice, drawImageDescriptorSetLayout);
		
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfo.imageView = drawImage.imageView;
		
		VkWriteDescriptorSet drawImageWrite = {};
		drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		drawImageWrite.pNext = nullptr;
		drawImageWrite.dstBinding = 0;
		drawImageWrite.dstSet = drawImageDescriptorSet;
		drawImageWrite.descriptorCount = 1;
		drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		drawImageWrite.pImageInfo = &imageInfo;
		
		vkUpdateDescriptorSets(logicalDevice, 1, &drawImageWrite, 0, nullptr);
		
		vulkanDeletionQueue.pushFunction([&](VulkanBackend *be) {
			assert(pAllocator == nullptr);
			be->globalDescriptorAllocator.destroyPool(be->logicalDevice, pAllocator);
			vkDestroyDescriptorSetLayout(be->logicalDevice, be->drawImageDescriptorSetLayout, pAllocator);
		});
		
		// initialize pipelines
		// compute effect pipeline
		VkPipelineLayoutCreateInfo computeLayoutCreateInfo = vk_util::defaultPipelineLayoutCreateInfo();
		computeLayoutCreateInfo.pSetLayouts = &drawImageDescriptorSetLayout;
		computeLayoutCreateInfo.setLayoutCount = 1;
		
		VkPushConstantRange computePushConstant = {};
		computePushConstant.offset = 0;
		computePushConstant.size = sizeof(ComputePushConstants);
		computePushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		computeLayoutCreateInfo.pPushConstantRanges = &computePushConstant;
		computeLayoutCreateInfo.pushConstantRangeCount = 1;
		
		VkResult computePipelineLayoutCreated = vkCreatePipelineLayout(logicalDevice, &computeLayoutCreateInfo, pAllocator, &gradientPipelineLayout);
		printf("compute pipeline layout created: %s\n", string_VkResult(computePipelineLayoutCreated));
		
		VkShaderModule gradientShader;
		if (!vk_util::loadShaderModule("..\\shaders\\gradient_color.comp.spv", logicalDevice, &gradientShader))
		{
			printf("failed to load shader");
		}
		VkShaderModule skyShader;
		if (!vk_util::loadShaderModule("..\\shaders\\sky.comp.spv", logicalDevice, &skyShader))
		{
			printf("failed to load shader");
		}
		
		VkPipelineShaderStageCreateInfo stageCreateInfo = {};
		stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreateInfo.pNext = nullptr;
		stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stageCreateInfo.module = gradientShader;
		stageCreateInfo.pName = "main";
		
		VkComputePipelineCreateInfo computePipelineCreateInfo = {};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.pNext = nullptr;
		computePipelineCreateInfo.layout = gradientPipelineLayout;
		computePipelineCreateInfo.stage = stageCreateInfo;
		
		ComputeEffect gradient;
		gradient.layout = gradientPipelineLayout;
		gradient.name = "vertical gradient";
		gradient.data = {};
		gradient.data.data1 = glm::vec4(1, 0, 0, 1);
		gradient.data.data2 = glm::vec4(0, 0, 1, 1);
		
		VkResult computePipelineCreated = vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, pAllocator, &gradient.pipeline);
		printf("gradient pipeline created: %s\n", string_VkResult(computePipelineCreated));
		
		// reusing computePipelineCreateInfo and just changing module
		computePipelineCreateInfo.stage.module = skyShader;
		
		ComputeEffect sky;
		sky.layout = gradientPipelineLayout;
		sky.name = "sky";
		sky.data = {};
		sky.data.data1 = glm::vec4(0.1, 0.2, 0.4 ,0.97);
		
		VkResult skyPipelineCreated = vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, pAllocator, &sky.pipeline);
		printf("sky pipeline created: %s\n", string_VkResult(skyPipelineCreated));
		
		backgroundEffects.push_back(gradient);
		backgroundEffects.push_back(sky);
		
		vkDestroyShaderModule(logicalDevice, gradientShader, pAllocator);
		vkDestroyShaderModule(logicalDevice, skyShader, pAllocator);
		vulkanDeletionQueue.pushFunction([&](VulkanBackend *be){
			assert(pAllocator == nullptr);
			vkDestroyPipelineLayout(be->logicalDevice, be->gradientPipelineLayout, pAllocator);
			for (auto effect : be->backgroundEffects)
			{
				vkDestroyPipeline(be->logicalDevice, effect.pipeline, pAllocator);	
			}
		});
		
		// graphics pipeline for colored triangle shader
		VkShaderModule triangleVertexShader;
		if (!vk_util::loadShaderModule("..\\shaders\\colored_triangle.vert.spv", logicalDevice, &triangleVertexShader))
		{
			printf("failed to load triangle vertex shader");
		}
		VkShaderModule triangleFragmentShader;
		if (!vk_util::loadShaderModule("..\\shaders\\colored_triangle.frag.spv", logicalDevice, &triangleFragmentShader))
		{
			printf("failed to load triangle fragment shader");
		}
		
		VkPipelineLayoutCreateInfo trianglePipelineLayoutInfo = vk_util::defaultPipelineLayoutCreateInfo();
		VkResult trianglePipelineLayoutCreated = vkCreatePipelineLayout(logicalDevice, &trianglePipelineLayoutInfo, pAllocator, &trianglePipelineLayout);
		printf("triangle pipeline layout created: %s\n", string_VkResult(trianglePipelineLayoutCreated));
		
		vk_util::PipelineBuilder pipelineBuilder;
		pipelineBuilder.pipelineLayout = trianglePipelineLayout;
		pipelineBuilder.setShaders(triangleVertexShader, triangleFragmentShader);
		pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
		pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		pipelineBuilder.setMultisamplingNone();
		pipelineBuilder.disableBlending();
		pipelineBuilder.disableDepthTest();
		pipelineBuilder.setColorAttachmentFormat(drawImage.imageFormat);
		pipelineBuilder.setDepthFormat(VK_FORMAT_UNDEFINED);
		
		trianglePipeline = pipelineBuilder.buildPipeline(logicalDevice, pAllocator);
		if (trianglePipeline == VK_NULL_HANDLE)
		{
			printf("do real error handling. triangle pipeline failed to create\n");
		}
		
		vkDestroyShaderModule(logicalDevice, triangleVertexShader, pAllocator);
		vkDestroyShaderModule(logicalDevice, triangleFragmentShader, pAllocator);
		
		vulkanDeletionQueue.pushFunction([&](VulkanBackend* be) {
			vkDestroyPipelineLayout(be->logicalDevice, be->trianglePipelineLayout, nullptr); //pAllocator
			vkDestroyPipeline(be->logicalDevice, be->trianglePipeline, nullptr); // pAllocator
		});
		
		// graphics pipeline for triangle mesh shader
		VkPipelineLayoutCreateInfo triMeshPipelineLayoutInfo = vk_util::defaultPipelineLayoutCreateInfo();
		
		VkPushConstantRange bufferRange = {};
		bufferRange.offset = 0;
		bufferRange.size = sizeof(GPUDrawPushConstants);
		bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		triMeshPipelineLayoutInfo.pPushConstantRanges = &bufferRange;
		triMeshPipelineLayoutInfo.pushConstantRangeCount = 1;
		
		VkShaderModule triangleMeshVertexShader;
		if (!vk_util::loadShaderModule("..\\shaders\\colored_triangle_mesh.vert.spv", logicalDevice, &triangleMeshVertexShader))
		{
			printf("failed to load triangle vertex shader");
		}
		VkShaderModule triangleMeshFragmentShader;
		if (!vk_util::loadShaderModule("..\\shaders\\colored_triangle.frag.spv", logicalDevice, &triangleMeshFragmentShader))
		{
			printf("failed to load triangle fragment shader");
		}
		
		VkResult triangleMeshPipelineLayoutCreated = vkCreatePipelineLayout(logicalDevice, &triMeshPipelineLayoutInfo, pAllocator, &triangleMeshPipelineLayout);
		printf("triangle mesh pipeline layout created: %s\n", string_VkResult(triangleMeshPipelineLayoutCreated));
		
		vk_util::PipelineBuilder meshPipelineBuilder;
		meshPipelineBuilder.pipelineLayout = triangleMeshPipelineLayout;
		meshPipelineBuilder.setShaders(triangleMeshVertexShader, triangleMeshFragmentShader);
		meshPipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		meshPipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
		meshPipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
		meshPipelineBuilder.setMultisamplingNone();
		meshPipelineBuilder.disableBlending();
		meshPipelineBuilder.disableDepthTest();
		meshPipelineBuilder.setColorAttachmentFormat(drawImage.imageFormat);
		meshPipelineBuilder.setDepthFormat(VK_FORMAT_UNDEFINED);
		
		triangleMeshPipeline = meshPipelineBuilder.buildPipeline(logicalDevice, pAllocator);
		
		vkDestroyShaderModule(logicalDevice, triangleMeshVertexShader, pAllocator);
		vkDestroyShaderModule(logicalDevice, triangleMeshFragmentShader, pAllocator);
		vulkanDeletionQueue.pushFunction([&](VulkanBackend* be) {
			vkDestroyPipelineLayout(be->logicalDevice, be->triangleMeshPipelineLayout, nullptr); //pAllocator
			vkDestroyPipeline(be->logicalDevice, be->triangleMeshPipeline, nullptr); // pAllocator
		});
		
		// init command pools
		buildCommandPool(pAllocator);
		
		// init default data
		{
			std::array<Vertex,4> rectVertices;

			rectVertices[0].position = { 0.5, -0.5, 0};
			rectVertices[1].position = { 0.5,  0.5, 0};
			rectVertices[2].position = {-0.5, -0.5, 0};
			rectVertices[3].position = {-0.5,  0.5, 0};

			rectVertices[0].color = {   0,   0,   0, 1};
			rectVertices[1].color = { 0.5, 0.5, 0.5, 1};
			rectVertices[2].color = {   1,   0,   0, 1};
			rectVertices[3].color = {   0,   1,   0, 1};

			std::array<uint32,6> rectIndices;

			rectIndices[0] = 0;
			rectIndices[1] = 1;
			rectIndices[2] = 2;
			
			rectIndices[3] = 2;
			rectIndices[4] = 1;
			rectIndices[5] = 3;

			rectangle = uploadMesh(rectIndices, rectVertices);

			//delete the rectangle data on engine shutdown
			vulkanDeletionQueue.pushFunction([&](VulkanBackend* be) {
				vk_util::destroyBuffer(be->vAllocator, be->rectangle.indexBuffer);
				vk_util::destroyBuffer(be->vAllocator, be->rectangle.vertexBuffer);
			});

		}
		
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
		
		
		VkResult result = vkCreateImage(logicalDevice, &createInfo, pAllocator, &testImage);
		printf("result: %s\n", string_VkResult(result));
		vulkanDeletionQueue.pushFunction([&](VulkanBackend* be){
			vkDestroyImage(be->logicalDevice, be->testImage, pAllocator);
		});
			
		#endif
	}

	void 
	initializeImgui()
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
		VK_CHECK(vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &imguiPool));

		// 2: initialize imgui library for Vulkan assuming platform layer already called
		// CreateContext() and the platform imp init

		// this initializes imgui for Vulkan
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = instance;
		init_info.PhysicalDevice = physicalDevice;
		init_info.Device = logicalDevice;
		init_info.Queue = graphicsQueue;
		init_info.DescriptorPool = imguiPool;
		init_info.MinImageCount = 3;
		init_info.ImageCount = 3;
		init_info.UseDynamicRendering = true;

		//dynamic rendering parameters for imgui to use
		init_info.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
		init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &chosenSurfaceFormat.format;
		

		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

		ImGui_ImplVulkan_Init(&init_info);

		// add the destroy the imgui created structures
		vulkanDeletionQueue.pushFunction([=](VulkanBackend* be) {
			ImGui_ImplVulkan_Shutdown();
			vkDestroyDescriptorPool(be->logicalDevice, imguiPool, nullptr);
		});
	}
	void
	cleanup(VkAllocationCallbacks* pAllocator)
	{
		
		// GPU cleanup section
		vkDeviceWaitIdle(logicalDevice);
		
		vulkanDeletionQueue.flush(this);
		frameData.deletionQueue.flush(this);
		
		// TODO(James) vkFreeMemory when we have VkDeviceMemory's
		// TODO(James) vkDestroyBuffer when we have them
		// TODO(James) vkDestroyPipeline when we have them
		// TODO(James) vkDestroyPipelineLayout when we have them
		// TODO(James) vkDestroyPipelineLayout when we have them
		// TODO(James) vkDestroyDescriptorSetLayout when we have them
		// TODO(James) vkDestroyDescriptorPool when we have them
		vkDestroyCommandPool(logicalDevice, frameData.commandPool, pAllocator);
		
		for(uint32 i = 0; i <swapchainImageCount; i++)
		{
			vkDestroyImageView(logicalDevice, swapchainImageViews[i], pAllocator);
			vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], pAllocator);
		}
		
		vkDestroySwapchainKHR(logicalDevice, swapchain, pAllocator);
		
		vkDestroyFence(logicalDevice, imageAvailableFence, pAllocator);
		vkDestroyFence(logicalDevice, syncHostWithDeviceFence, pAllocator);
		vkDestroySemaphore(logicalDevice, imageAvailableSemaphore, pAllocator);
		
		
		vkDestroyDevice(logicalDevice, pAllocator);
		vkDestroySurfaceKHR(instance,surface, pAllocator);
		// TODO(James): vkDestroyDebugUtilsMessengerEXT when we make one
		vkDestroyInstance(instance, pAllocator);
		
		// end GPU cleanup section
		
		// CPU cleanup section
		isInitialized = false;
		delete[] swapchainImageHandles;
		delete[] swapchainImageViews;
		delete[] renderFinishedSemaphores;
		
		// end CPU cleanup section
	}


	void
	buildCommandPool(const VkAllocationCallbacks* pAllocator)
	{
		// making a command pool that gives us resetable command buffers for using the whole dang time
		VkResult poolCreated = vk_util::createCommandPool(logicalDevice, graphicsQueueFamily, pAllocator, &frameData.commandPool);
		printf("created command pool: %s\n", string_VkResult(poolCreated));

		VkResult commandBufferAllocated = vk_util::allocateCommandBuffer(logicalDevice, frameData.commandPool, 1, &frameData.graphicsCommandBuffer);
		printf("allocated graphics command buffer: %s\n", string_VkResult(commandBufferAllocated));
		
		// creating another pool for immediate mode commands
		VkResult immPoolCreated = vk_util::createCommandPool(logicalDevice, graphicsQueueFamily, pAllocator, &immCommandPool);
		printf("created imm command pool: %s\n", string_VkResult(immPoolCreated));
		
		VkResult immCommandBufferAllocated = vk_util::allocateCommandBuffer(logicalDevice, immCommandPool, 1, &immCommandBuffer);
		printf("allocated imm command buffer: %s\n", string_VkResult(immCommandBufferAllocated));
		
		vulkanDeletionQueue.pushFunction([&](VulkanBackend* be) {
			vkDestroyCommandPool(be->logicalDevice, be->immCommandPool, nullptr); //pAllocator
		});
		
	}

	void 
	immediateSubmit(std::function<void(VkCommandBuffer cmdBuffer)>&& function)
	{
		VK_CHECK(vkResetFences(logicalDevice, 1, &immFence));
		VK_CHECK(vkResetCommandBuffer(immCommandBuffer, 0));
		
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		
		VK_CHECK(vkBeginCommandBuffer(immCommandBuffer, &beginInfo));
		function(immCommandBuffer);
		VK_CHECK(vkEndCommandBuffer(immCommandBuffer));
		
		VkCommandBufferSubmitInfo bufferSubmitInfo = {};
		bufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		bufferSubmitInfo.pNext = nullptr;
		bufferSubmitInfo.commandBuffer = immCommandBuffer;
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
		
		VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, immFence));
		VK_CHECK(vkWaitForFences(logicalDevice, 1, &immFence, true, 9999999999));
	}

	GPUMeshBuffers uploadMesh(std::span<uint32> indices, std::span<Vertex> vertices)
	{
		assert(isInitialized);
		const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
		const size_t indexBufferSize = indices.size() * sizeof(uint32);
		
		GPUMeshBuffers meshBuffers;
		VkBufferUsageFlags vertexBufferFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
												VK_BUFFER_USAGE_TRANSFER_DST_BIT |
												VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		meshBuffers.vertexBuffer = vk_util::createBuffer(vAllocator, vertexBufferSize, vertexBufferFlags, VMA_MEMORY_USAGE_GPU_ONLY);
		// find address of the vertex buffer
		VkBufferDeviceAddressInfo deviceAddressInfo = {};
		deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		deviceAddressInfo.buffer = meshBuffers.vertexBuffer.buffer;
		meshBuffers.vertexBufferAddress = vkGetBufferDeviceAddress(logicalDevice, &deviceAddressInfo);
		
		VkBufferUsageFlags indexBufferFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
												VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		meshBuffers.indexBuffer = vk_util::createBuffer(vAllocator, indexBufferSize, indexBufferFlags, VMA_MEMORY_USAGE_GPU_ONLY);
		
		// with the device buffers in place, we use a staging buffer we can write from CPU
		// to seed with the mesh data and then copy overflow
		AllocatedBuffer staging = vk_util::createBuffer(vAllocator, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
		
		void *data = staging.allocation->GetMappedData();
		memcpy(data, vertices.data(), vertexBufferSize);
		memcpy((uint8*)data + vertexBufferSize, indices.data(), indexBufferSize);
		
		immediateSubmit([&](VkCommandBuffer cmd)
		{
			VkBufferCopy vertexCopy { 0 };
			vertexCopy.dstOffset = 0;
			vertexCopy.srcOffset = 0;
			vertexCopy.size = vertexBufferSize;
			vkCmdCopyBuffer(cmd, staging.buffer, meshBuffers.vertexBuffer.buffer, 1, &vertexCopy);
			
			VkBufferCopy indexCopy { 0 };
			indexCopy.dstOffset = 0; // interesting
			indexCopy.srcOffset = vertexBufferSize;
			indexCopy.size = indexBufferSize;
			vkCmdCopyBuffer(cmd, staging.buffer, meshBuffers.indexBuffer.buffer, 1, &indexCopy);
		});
		
		vk_util::destroyBuffer(vAllocator, staging);
		
		return meshBuffers;
	}

	void
	beginCommandBuffer()
	{
		assert(isInitialized);
		VkResult resetCommandBuffer = vkResetCommandBuffer(frameData.graphicsCommandBuffer, 0);
		//printf("reset command buffer: %s\n", string_VkResult(resetCommandBuffer));
		VkResult beganCommandBuffer = vkBeginCommandBuffer(frameData.graphicsCommandBuffer, &commandBufferBeginInfo);
		//printf("began command buffer: %s\n", string_VkResult(beganCommandBuffer));
	}

	void
	transitionImage(VkImage image, VkImageAspectFlags aspectFlags, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		assert(isInitialized);
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
		
		vkCmdPipelineBarrier2(frameData.graphicsCommandBuffer, &dependencyInfo);
	}

	void
	copyImage(VkCommandBuffer commandBuffer, VkImage source, VkImage dest, VkExtent2D sourceSize, VkExtent2D destSize)
	{
		assert(isInitialized);
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
		
		vkCmdBlitImage2(commandBuffer, &blitInfo);
	}

	void
	drawImGui(VkCommandBuffer commandBuffer, VkImageView imageView, VkImageLayout layout, VkExtent2D renderExtent)
	{
		assert(isInitialized);
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
	draw()
	{
		assert(isInitialized);
		uint32 imageIndex;
		vkWaitForFences(logicalDevice, 1, &syncHostWithDeviceFence, VK_TRUE, 1000000000);
		vkResetFences(logicalDevice, 1, &syncHostWithDeviceFence);
		
		frameData.deletionQueue.flush(this);
		
		// acquire image
		vkWaitForFences(logicalDevice, 1, &imageAvailableFence, VK_TRUE, 1000000000);
		vkResetFences(logicalDevice, 1, &imageAvailableFence);
		VkResult imageAcquired = vkAcquireNextImageKHR(logicalDevice, swapchain, UINT64_MAX, imageAvailableSemaphore, imageAvailableFence, &imageIndex);
		//printf("image acquired: %s\n", string_VkResult(imageAcquired));
		
		drawExtent2D.width = drawImage.imageExtent.width;
		drawExtent2D.height = drawImage.imageExtent.height;
		beginCommandBuffer();
		
		// transition draw image to be drawable with a pipeline barrier
		VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout newLayout = VK_IMAGE_LAYOUT_GENERAL;
		transitionImage(drawImage.imageHandle, aspectFlags, oldLayout, newLayout);
		
		// translate commands for frame to vk commands
		#if 0
		// lol jk we're just clearing the screen
		real32 flash = abs(sin(frameNumber / 30.0f));
		VkClearColorValue clearValue = { {0.0f, 0.0f, flash, 1.0f} };
		VkImageSubresourceRange clearRange = {};
		clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		clearRange.baseMipLevel = 0;
		clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
		clearRange.baseArrayLayer = 0;
		clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
		vkCmdClearColorImage(frameData.graphicsCommandBuffer, drawImage.imageHandle, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
		#endif
		
		// render background with selected compute effect
		{
			ComputeEffect currentEffect = backgroundEffects[currentBackgroundEffect];
			vkCmdBindPipeline(frameData.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, currentEffect.pipeline);
			vkCmdBindDescriptorSets(frameData.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, currentEffect.layout, 0, 1, &drawImageDescriptorSet, 0, nullptr);

			vkCmdPushConstants(frameData.graphicsCommandBuffer, currentEffect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &currentEffect.data);
			
			// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
			uint32 groupCountX = std::ceil(drawExtent2D.width / 16.0);
			uint32 groupCountY = std::ceil(drawExtent2D.height / 16.0);
			printf("draw extent is %d by %d dispatching in groups of %d by %d\n", drawImage.imageExtent.width, drawImage.imageExtent.height, groupCountX, groupCountY);
			vkCmdDispatch(frameData.graphicsCommandBuffer, groupCountX, groupCountY, 1);
		}
		// transition draw image for rendering with graphics pipeline
		transitionImage(drawImage.imageHandle, aspectFlags, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		
		// render geometry
		{
			VkRenderingAttachmentInfo colorAttachment = {};
			colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			colorAttachment.pNext = nullptr;
			colorAttachment.imageView = drawImage.imageView;
			colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			
			VkRenderingInfo renderInfo = {};
			renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			renderInfo.pNext = nullptr;
			renderInfo.renderArea = VkRect2D { VkOffset2D { 0, 0 }, drawExtent2D };
			renderInfo.layerCount = 1;
			renderInfo.colorAttachmentCount = 1;
			renderInfo.pColorAttachments = &colorAttachment;
			renderInfo.pDepthAttachment = nullptr;
			renderInfo.pStencilAttachment = nullptr;
			
			vkCmdBeginRendering(frameData.graphicsCommandBuffer, &renderInfo);
			
			// hardcoded triangle
			vkCmdBindPipeline(frameData.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
			
			// dynamic viewport. Should live elsewhere later
			VkViewport viewport = {};
			viewport.x = 0;
			viewport.y = 0;
			viewport.width = drawExtent2D.width;
			viewport.height =drawExtent2D.height;
			viewport.minDepth = 0.f;
			viewport.maxDepth = 1.f;
			
			vkCmdSetViewport(frameData.graphicsCommandBuffer, 0, 1, &viewport);
			
			// dynamic scissor
			VkRect2D scissor = {};
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			scissor.extent.width = drawImage.imageExtent.width;
			scissor.extent.height = drawImage.imageExtent.height;
			
			vkCmdSetScissor(frameData.graphicsCommandBuffer, 0, 1, &scissor);
			
			// hard coded to triangle shader that has an array of exactly 3 vertices
			// vertex count, instance count, first vertex, firstInstance
			vkCmdDraw(frameData.graphicsCommandBuffer, 3, 1, 0, 0);
			
			// triangle mesh
			vkCmdBindPipeline(frameData.graphicsCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, triangleMeshPipeline);
			
			GPUDrawPushConstants meshPushConstants;
			meshPushConstants.worldMatrix = glm::mat4{ 1.f };
			meshPushConstants.vertexBuffer = rectangle.vertexBufferAddress;
			
			vkCmdPushConstants(frameData.graphicsCommandBuffer, triangleMeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &meshPushConstants);
			
			vkCmdBindIndexBuffer(frameData.graphicsCommandBuffer, rectangle.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			
			// still hard coding the index count, instance count, first index, vertex offset, and first instance
			vkCmdDrawIndexed(frameData.graphicsCommandBuffer, 6, 1, 0, 0, 0);
			
			vkCmdEndRendering(frameData.graphicsCommandBuffer);
		}
		
		// transfer image from drawImage to the swapchain image
		transitionImage(drawImage.imageHandle, aspectFlags, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		transitionImage(swapchainImageHandles[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyImage(frameData.graphicsCommandBuffer, drawImage.imageHandle, swapchainImageHandles[imageIndex], drawExtent2D, windowExtent);
		
		// make swapchain image layout attachment optimal so we can draw immediate mode commands
		transitionImage(swapchainImageHandles[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		
		//draw imgui
		drawImGui(frameData.graphicsCommandBuffer, swapchainImageViews[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, windowExtent);
		
		// make swapchain image presentable with a pipeline barrier
		transitionImage(swapchainImageHandles[imageIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		
		// end command buffer
		vkEndCommandBuffer(frameData.graphicsCommandBuffer);
		
		// submit command queue for the frame
		VkSemaphoreSubmitInfo imageAvailableSemaphoreSubmitInfo = {};
		imageAvailableSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		imageAvailableSemaphoreSubmitInfo.pNext = nullptr;
		imageAvailableSemaphoreSubmitInfo.semaphore = imageAvailableSemaphore;
		imageAvailableSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
		imageAvailableSemaphoreSubmitInfo.deviceIndex = 0;
		imageAvailableSemaphoreSubmitInfo.value = 1;
		
		VkSemaphoreSubmitInfo renderFinishedSemaphoreSubmitInfo = {};
		renderFinishedSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		renderFinishedSemaphoreSubmitInfo.pNext = nullptr;
		renderFinishedSemaphoreSubmitInfo.semaphore = renderFinishedSemaphores[imageIndex];
		renderFinishedSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
		renderFinishedSemaphoreSubmitInfo.deviceIndex = 0;
		renderFinishedSemaphoreSubmitInfo.value = 1;
		
		VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
		commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		commandBufferSubmitInfo.pNext = nullptr;
		commandBufferSubmitInfo.commandBuffer = frameData.graphicsCommandBuffer;
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
		
		vkQueueSubmit2(graphicsQueue, 1, &submitInfo, syncHostWithDeviceFence);
		
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.pSwapchains = &swapchain;
		presentInfo.swapchainCount = 1;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];
		presentInfo.pImageIndices = &imageIndex;
		
		VkResult presented = vkQueuePresentKHR(graphicsQueue, &presentInfo);
		//printf("image presented: %s\n", string_VkResult(presented));
		
		frameNumber++;
	}

	void
	populateUIData(EffectUI *outUI)
	{
		assert(isInitialized);
		ComputeEffect &currentEffect = backgroundEffects[currentBackgroundEffect];
		outUI->name = currentEffect.name;
		outUI->pIndex = reinterpret_cast<int32*>(&currentBackgroundEffect);
		outUI->maxSize = static_cast<int32>(backgroundEffects.size() - 1);
		outUI->data1 = (real32*) &currentEffect.data.data1;
		outUI->data2 = (real32*) &currentEffect.data.data2;
		outUI->data3 = (real32*) &currentEffect.data.data3;
		outUI->data4 = (real32*) &currentEffect.data.data4;
		
	}

};


#define VK_H
#endif
    