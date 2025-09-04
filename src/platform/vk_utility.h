#if !defined(VK_UTIL_H)
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
	
	AllocatedBuffer createBuffer(VmaAllocator& vAllocator, size_t allocSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage)
	{
		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.pNext = nullptr;
		bufferCreateInfo.size = allocSize;
		bufferCreateInfo.usage = bufferUsage;
		
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = memoryUsage;
		allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
		
		AllocatedBuffer newBuffer;
		VK_CHECK(vmaCreateBuffer(vAllocator, &bufferCreateInfo, &allocCreateInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.allocationInfo));
		return newBuffer;
	}
	
	void destroyBuffer(VmaAllocator& vAllocator, AllocatedBuffer &buffer)
	{
		vmaDestroyBuffer(vAllocator, buffer.buffer, buffer.allocation);
	}
	
	VkPipelineLayoutCreateInfo defaultPipelineLayoutCreateInfo()
	{
		VkPipelineLayoutCreateInfo info {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.pNext = nullptr;

		// empty defaults
		info.flags = 0;
		info.setLayoutCount = 0;
		info.pSetLayouts = nullptr;
		info.pushConstantRangeCount = 0;
		info.pPushConstantRanges = nullptr;
		return info;
	}
	
	struct PipelineBuilder
	{
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		
		VkPipelineInputAssemblyStateCreateInfo 	inputAssemblyInfo;
		VkPipelineRasterizationStateCreateInfo 	rasterizationInfo;
		VkPipelineColorBlendAttachmentState 	colorBlendAttachmentState;
		VkPipelineMultisampleStateCreateInfo 	multisampleInfo;
		VkPipelineLayout						pipelineLayout;
		VkPipelineDepthStencilStateCreateInfo	depthStencilInfo;
		VkPipelineRenderingCreateInfo			renderingInfo;
		VkFormat								colorAttachmentFormat;
		
		PipelineBuilder()
		{
			// zero initialize everything but the sTypes
			clear(); 
		}
		
		void clear()
		{
			inputAssemblyInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
			rasterizationInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
			colorBlendAttachmentState = {};
			multisampleInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
			pipelineLayout = {};
			depthStencilInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
			renderingInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
			shaderStages.clear();
		}
		
		void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
		{
			shaderStages.clear();
			
			VkPipelineShaderStageCreateInfo vertexCreateInfo = {};
			vertexCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertexCreateInfo.pNext = nullptr;
			vertexCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertexCreateInfo.module = vertexShader;
			vertexCreateInfo.pName = "main";
			shaderStages.push_back(vertexCreateInfo);
			
			VkPipelineShaderStageCreateInfo fragmentCreateInfo = {};
			fragmentCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragmentCreateInfo.pNext = nullptr;
			fragmentCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragmentCreateInfo.module = fragmentShader;
			fragmentCreateInfo.pName = "main";
			shaderStages.push_back(fragmentCreateInfo);
		}
		
		void setInputTopology(VkPrimitiveTopology topology)
		{
			inputAssemblyInfo.topology = topology;
			// not currently enabling primitive restart
			inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
		}
		
		void setPolygonMode(VkPolygonMode polygonMode)
		{
			rasterizationInfo.polygonMode = polygonMode;
			rasterizationInfo.lineWidth = 1.f; // hardcoded
		}
		
		void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
		{
			rasterizationInfo.cullMode = cullMode;
			rasterizationInfo.frontFace = frontFace;
		}
		
		void setMultisamplingNone()
		{
			multisampleInfo.sampleShadingEnable = VK_FALSE;
			// one sample per pixel
			multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampleInfo.minSampleShading = 1.0f;
			multisampleInfo.pSampleMask = nullptr;
			// no alpha coverage
			multisampleInfo.alphaToCoverageEnable = VK_FALSE;
			multisampleInfo.alphaToOneEnable = VK_FALSE;
		}
		
		void setMultiSampling16()
		{
			assert(false);
		}
		
		void disableBlending()
		{
			// all the bits RGBA
			colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState.blendEnable = VK_FALSE;
		}
		
		void setColorAttachmentFormat(VkFormat format)
		{
			colorAttachmentFormat = format;
			// we're not doing deferred rendering with multiple images so just the one for now
			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
		}
		
		void setDepthFormat(VkFormat format)
		{
			renderingInfo.depthAttachmentFormat = format;
		}
		
		void disableDepthTest()
		{
			depthStencilInfo.depthTestEnable = VK_FALSE;
			depthStencilInfo.depthWriteEnable = VK_FALSE;
			depthStencilInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
			depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
			depthStencilInfo.stencilTestEnable = VK_FALSE;
			depthStencilInfo.front = {};
			depthStencilInfo.back = {};
			depthStencilInfo.minDepthBounds = 0.f;
			depthStencilInfo.maxDepthBounds = 1.f;
		}

		void enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp)
		{
			depthStencilInfo.depthTestEnable = VK_TRUE;
			depthStencilInfo.depthWriteEnable = depthWriteEnable;
			depthStencilInfo.depthCompareOp = compareOp;
			depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
			depthStencilInfo.stencilTestEnable = VK_FALSE;
			depthStencilInfo.front = {};
			depthStencilInfo.back = {};
			depthStencilInfo.minDepthBounds = 0.f;
			depthStencilInfo.maxDepthBounds = 1.f;
		}

		void enableBlendingAdditive()
		{
			colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState.blendEnable = VK_TRUE;
			colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		}

		void enableBlendingAlphaBlend()
		{
			colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachmentState.blendEnable = VK_TRUE;
			colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		}
		
		VkPipeline buildPipeline(VkDevice device, const VkAllocationCallbacks* pAllocator)
		{
			// we're going to use dynamic state for this so bare bones at this point
			VkPipelineViewportStateCreateInfo viewportStateInfo = {};
			viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateInfo.pNext = nullptr;
			viewportStateInfo.viewportCount = 1;
			viewportStateInfo.scissorCount = 1;
			
			// dummy color blending since we aren't handling transparent objects yet
			VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};
			colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendInfo.pNext = nullptr;
			colorBlendInfo.logicOpEnable = VK_FALSE;
			colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
			colorBlendInfo.attachmentCount = 1;
			colorBlendInfo.pAttachments = &colorBlendAttachmentState;
			
			// not actually using this since we're just sending a data array to the shader and indexing ourselves
			// open to change in the future
			VkPipelineVertexInputStateCreateInfo vertextInputInfo = {};
			vertextInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			
			// dynamic state
			VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
			dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicStateInfo.pDynamicStates = &state[0];
			dynamicStateInfo.dynamicStateCount = ArrayCount(state);
			
			// graphics pipeline
			VkGraphicsPipelineCreateInfo pipelineInfo = {};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.pNext = &renderingInfo;
			pipelineInfo.stageCount = (uint32)shaderStages.size();
			pipelineInfo.pStages = shaderStages.data();
			pipelineInfo.pVertexInputState = &vertextInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
			pipelineInfo.pViewportState = &viewportStateInfo;
			pipelineInfo.pRasterizationState = &rasterizationInfo;
			pipelineInfo.pMultisampleState = &multisampleInfo;
			pipelineInfo.pColorBlendState = &colorBlendInfo;
			pipelineInfo.pDepthStencilState = &depthStencilInfo;
			pipelineInfo.layout = pipelineLayout;
			pipelineInfo.pDynamicState = &dynamicStateInfo;
			
			VkPipeline newPipeline;
			VkResult pipelineCreated = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, pAllocator, &newPipeline);
			if (pipelineCreated == VK_SUCCESS)
			{
				return newPipeline;
			} else {
				printf("failed to create graphics pipeline: %s\n", string_VkResult(pipelineCreated));
				return VK_NULL_HANDLE;
			}
			
		}
	};
}
#define VK_UTIL_H
#endif