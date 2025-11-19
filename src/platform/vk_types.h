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
	DescriptorAllocatorGrowable frameDescriptors;
};

struct AllocatedImage
{
	VkImage 		imageHandle;
	VkImageView 	imageView;
	VmaAllocation 	allocation;
	VkExtent3D 		imageExtent;
	VkFormat 		imageFormat;
};

struct AllocatedBuffer
{
	VkBuffer 			buffer;
	VmaAllocation 		allocation;
	VmaAllocationInfo 	allocationInfo;
};

struct ComputePushConstants
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect 
{
	const char 		 	 *name;
	VkPipelineLayout 	 layout;
	VkPipeline 			 pipeline;
	ComputePushConstants data;
};

struct GPUMeshBuffers
{
	AllocatedBuffer indexBuffer;
	AllocatedBuffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress;
};

// push constants used for mesh draws
struct GPUDrawPushConstants
{
	glm::mat4 		worldMatrix;
	VkDeviceAddress vertexBuffer;
};

struct GPUSceneData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
};


struct GeoSurface
{
	uint32 startIndex;
	uint32 count;
};

struct MeshAsset
{
	std::string name;
	std::vector<GeoSurface> surfaces;
	
	GPUMeshBuffers meshBuffers;
};

#define VK_TYPES_H
#endif