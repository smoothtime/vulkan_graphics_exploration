#if !defined(VK_DESCRIPTORS_H)

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


struct DescriptorWriter
{
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void writeImage(int32 binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
    {
        VkDescriptorImageInfo &info = imageInfos.emplace_back(
            VkDescriptorImageInfo {
                .sampler = sampler,
                .imageView = imageView,
                .imageLayout = layout
            }
        );

        VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //set in updateSet()
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pImageInfo = &info;

        writes.push_back(write);
    }

    void writeBuffer(int32 binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
    {
        VkDescriptorBufferInfo &info = bufferInfos.emplace_back(
            VkDescriptorBufferInfo {
             .buffer = buffer,
             .offset = offset,
             .range = size
        });

        VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //set in updateSet()
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pBufferInfo = &info;

        writes.push_back(write);
    }

    void clear()
    {
        imageInfos.clear();
        bufferInfos.clear();
        writes.clear();
    }

    void updateSet(VkDevice device, VkDescriptorSet set)
    {
        for (VkWriteDescriptorSet &write : writes)
        {
            write.dstSet = set;
        }

        vkUpdateDescriptorSets(device, (uint32) writes.size(), writes.data(), 0, nullptr);
    }
};

struct DescriptorAllocatorGrowable 
{
    struct PoolSizeRatio
    {
        VkDescriptorType type;
        real32 ratio;
    };

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> fullPools;
    std::vector<VkDescriptorPool> readyPools;
    uint32 setsPerPool;

    void init(VkDevice device, uint32 initialSets, std::span<PoolSizeRatio> poolRatios, const VkAllocationCallbacks* pAllocator)
    {
        ratios.clear();

        for(PoolSizeRatio ratio : poolRatios)
        {
            ratios.push_back(ratio);
        }

        VkDescriptorPool newPool = createPool(device, initialSets, ratios, pAllocator);
        setsPerPool = setsPerPool * 1.5;
        readyPools.push_back(newPool);
    }

    void clearPools(VkDevice device)
    {
        for (VkDescriptorPool pool : readyPools)
        {
            vkResetDescriptorPool(device, pool, 0);
        }

        for (VkDescriptorPool pool: fullPools)
        {
            vkResetDescriptorPool(device, pool, 0);
            readyPools.push_back(pool);
        }
        fullPools.clear();
    }

    void destroyPools(VkDevice device, const VkAllocationCallbacks* pAllocator)
    {
        for (VkDescriptorPool pool : readyPools)
        {
            vkDestroyDescriptorPool(device, pool, pAllocator);
        }
        readyPools.clear();

        for (VkDescriptorPool pool: fullPools)
        {
            vkDestroyDescriptorPool(device, pool, pAllocator);
        }
        fullPools.clear();
    }

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void *pNext, const VkAllocationCallbacks* pAllocator)
    {
        VkDescriptorPool poolToUse = getPool(device, pAllocator);

        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = pNext;
        allocInfo.descriptorPool = poolToUse;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet ds;
        VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);
        if(result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
        {
            fullPools.push_back(poolToUse);
            poolToUse = getPool(device, pAllocator);
            allocInfo.descriptorPool = poolToUse;
            VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
        }
        readyPools.push_back(poolToUse);
        return ds;
    }

    VkDescriptorPool getPool(VkDevice device, const VkAllocationCallbacks* pAllocator)
    {
        VkDescriptorPool newPool;
        if (readyPools.size() != 0)
        {
            newPool = readyPools.back();
            readyPools.pop_back();
        } else 
        {
            newPool = createPool(device, setsPerPool, ratios, pAllocator);

            setsPerPool = setsPerPool * 1.5;
            if (setsPerPool > 4092)
            {
                setsPerPool = 4092;
            }
        }

        return newPool;
    }

    VkDescriptorPool createPool(VkDevice device, uint32 setCount, std::span<PoolSizeRatio> poolRatios, const VkAllocationCallbacks* pAllocator)
    {
        std::vector<VkDescriptorPoolSize> poolSizes;
        for(PoolSizeRatio ratio : poolRatios)
        {
            poolSizes.push_back(VkDescriptorPoolSize {
                .type = ratio.type, .descriptorCount = uint32(ratio.ratio * setCount)
            });
        }
        
        VkDescriptorPoolCreateInfo poolCreateInfo = {};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.flags = 0;
        poolCreateInfo.maxSets = setCount;
        poolCreateInfo.poolSizeCount = (uint32)poolSizes.size();
        poolCreateInfo.pPoolSizes = poolSizes.data();
        
        VkDescriptorPool newPool;
        VkResult descriptorPoolCreated = vkCreateDescriptorPool(device, &poolCreateInfo, pAllocator, &newPool);
        return newPool;
    }
};

#define VK_DESCRIPTORS_H
#endif