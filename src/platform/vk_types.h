#if !defined(VK_TYPES_H)

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

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char 		 	 *name;
	VkPipelineLayout 	 layout;
	VkPipeline 			 pipeline;
	ComputePushConstants data;
};

#define VK_TYPES_H
#endif