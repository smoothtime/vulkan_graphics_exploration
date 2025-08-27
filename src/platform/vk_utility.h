namespace vk_util{
	struct DescriptorLayoutBuilder
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		
		void addBinding(uint32 binding, VkDescriptorType type)
		{
			VkDescriptorSetLayoutBinding newBind = {};
			newBind.binding = binding;
			newBind.descriptorCount = 1;
			newBind.descriptorType = type;
			
			bindings.push_back(newBind);
		}
		
		void clear()
		{
			bindings.clear();
		}
		
		VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, const VkAllocationCallbacks* pAllocator, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0)
		{
			for(VkDescriptorSetLayoutBinding& b : bindings)
			{
				b.stageFlags |= shaderStages;
			}
			
			VkDescriptorSetLayoutCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			createInfo.pNext = pNext;
			createInfo.bindingCount = (uint32)bindings.size();
			createInfo.pBindings = bindings.data();
			createInfo.flags = flags;
			
			VkDescriptorSetLayout layout;
			VkResult descriptorSetLayoutCreated = vkCreateDescriptorSetLayout(device, &createInfo, pAllocator, &layout);
			printf("descriptorSetLayout created: %s\n", string_VkResult(descriptorSetLayoutCreated));
			
			return layout;
		}
	};

	struct DescriptorAllocator
	{
		struct PoolSizeRatio
		{
			VkDescriptorType type;
			real32 ratio;
		};
		
		VkDescriptorPool pool;
		
		void initPool(VkDevice device, uint32 maxSets, std::span<PoolSizeRatio> poolRatios, const VkAllocationCallbacks* pAllocator)
		{
			std::vector<VkDescriptorPoolSize> poolSizes;
			for(PoolSizeRatio ratio : poolRatios)
			{
				poolSizes.push_back(VkDescriptorPoolSize {
					.type = ratio.type, .descriptorCount = uint32(ratio.ratio * maxSets)
				});
			}
			
			VkDescriptorPoolCreateInfo poolCreateInfo = {};
			poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolCreateInfo.flags = 0;
			poolCreateInfo.maxSets = maxSets;
			poolCreateInfo.poolSizeCount = (uint32)poolSizes.size();
			poolCreateInfo.pPoolSizes = poolSizes.data();
			
			VkResult descriptorPoolCreated = vkCreateDescriptorPool(device, &poolCreateInfo, pAllocator, &pool);
			printf("descriptor pool created: %s\n", string_VkResult(descriptorPoolCreated));
		}
		
		void clearDescriptors(VkDevice device)
		{
			vkResetDescriptorPool(device, pool, 0);
		}
		
		void destroyPool(VkDevice device, const VkAllocationCallbacks* pAllocator)
		{
			vkDestroyDescriptorPool(device, pool, pAllocator);
		}
		
		VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout)
		{
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.pNext = nullptr;
			allocInfo.descriptorPool = pool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &layout;
			
			VkDescriptorSet set;
			VkResult descriptorSetCreated = vkAllocateDescriptorSets(device, &allocInfo, &set);
			printf("descriptor set: %s\n", string_VkResult(descriptorSetCreated));
			return set;
		}
	};

	bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
	{
		// open the file. With cursor at the end
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			return false;
		}

		// find what the size of the file is by looking up the location of the cursor
		// because the cursor is at the end, it gives the size directly in bytes
		size_t fileSize = (size_t)file.tellg();

		// spirv expects the buffer to be on uint32, so make sure to reserve a int
		// vector big enough for the entire file
		std::vector<uint32> buffer(fileSize / sizeof(uint32));

		// put file cursor at beginning
		file.seekg(0);

		// load the entire file into the buffer
		file.read((char*)buffer.data(), fileSize);

		// now that the file is loaded into the buffer, we can close it
		file.close();

		// create a new shader module, using the buffer we loaded
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.pNext = nullptr;

		// codeSize has to be in bytes, so multply the ints in the buffer by size of
		// int to know the real size of the buffer
		createInfo.codeSize = buffer.size() * sizeof(uint32);
		createInfo.pCode = buffer.data();

		// check that the creation goes well.
		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
			return false;
		}
		*outShaderModule = shaderModule;
		return true;
	}
	
	VkResult createCommandPool(VkDevice device, uint32 queueFamily, const VkAllocationCallbacks* pAllocator, VkCommandPool *outCommandPool)
	{
		VkCommandPoolCreateInfo commandPoolCreateInfo = {};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.pNext = nullptr;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		commandPoolCreateInfo.queueFamilyIndex = queueFamily;
		
		return vkCreateCommandPool(device, &commandPoolCreateInfo, pAllocator, outCommandPool);
	}
	
	VkResult allocateCommandBuffer(VkDevice device, VkCommandPool commandPool, uint32 count, VkCommandBuffer *outBuffer)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.pNext = nullptr;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.commandBufferCount = count;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		return vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, outBuffer);
	}
}
