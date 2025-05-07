#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h> // Include VMA header AFTER the implementation define
#include <algorithm>          // for std::find_if, std::remove_if if needed later
#include "vra.h"

namespace vra
{
    // -------------------------------------
    // --- Data Collector Implementation ---
    // -------------------------------------

    VraDataCollector::VraDataCollector()
    {
        RegisterDefaultGroups();
    }

    VraDataCollector::~VraDataCollector()
    {
    }

    void VraDataCollector::RegisterBufferGroup(
        std::string name,
        std::function<bool(const VraDataDesc &)> predicate,
        std::function<void(GroupedBufferData &, ResourceId, const VraDataDesc &, const VraRawData &, const VkPhysicalDeviceProperties &)> group_action)
    {
        // Check if group with this name already exists to prevent duplicates
        if (group_name_to_index_map_.count(name))
        {
            // Optionally throw an error or log a warning
            // For now, let's overwrite or ignore if names clash, but unique names are better.
            // For simplicity, we'll assume names are unique for now or the first registration wins.
            // A more robust system might throw or return a bool.
            return;
        }
        registered_groups_.push_back(VraBufferGroup{std::move(name), std::move(predicate), std::move(group_action)});
        group_name_to_index_map_[registered_groups_.back().name] = registered_groups_.size() - 1;
    }

    void VraDataCollector::RegisterDefaultGroups()
    {
        // Static Local Group
        RegisterBufferGroup(
            "static_local_group",
            [](const VraDataDesc &desc)
            { 
                return desc.GetMemoryPattern() == VraDataMemoryPattern::Static_Local; 
            },
            [](GroupedBufferData &group, ResourceId id, const VraDataDesc &desc, const VraRawData &raw_data, const VkPhysicalDeviceProperties & /*props*/)
            {
                if (raw_data.pData_ != nullptr && raw_data.size_ > 0)
                {
                    group.offsets[id] = group.total_size;
                    const uint8_t *data_ptr = static_cast<const uint8_t *>(raw_data.pData_);
                    group.consolidated_data.insert(group.consolidated_data.end(), data_ptr, data_ptr + raw_data.size_);
                    group.total_size += raw_data.size_;
                }
                else
                {
                    group.offsets[id] = group.total_size;
                }
                group.combined_usage_flags |= desc.GetBufferDesc().usage_flags_;
            });

        // Static Upload Group
        RegisterBufferGroup(
            "static_upload_group",
            [](const VraDataDesc &desc)
            { 
                return desc.GetMemoryPattern() == VraDataMemoryPattern::Static_Upload; 
            },
            [](GroupedBufferData &group, ResourceId id, const VraDataDesc &desc, const VraRawData &raw_data, const VkPhysicalDeviceProperties & /*props*/)
            {
                if (raw_data.pData_ != nullptr && raw_data.size_ > 0)
                {
                    group.offsets[id] = group.total_size;
                    const uint8_t *data_ptr = static_cast<const uint8_t *>(raw_data.pData_);
                    group.consolidated_data.insert(group.consolidated_data.end(), data_ptr, data_ptr + raw_data.size_);
                    group.total_size += raw_data.size_;
                }
                else
                {
                    group.offsets[id] = group.total_size;
                }
                group.combined_usage_flags |= desc.GetBufferDesc().usage_flags_;
            });

        // Dynamic Sequential Group
        RegisterBufferGroup(
            "dynamic_sequential_group",
            [](const VraDataDesc &desc)
            { 
                return desc.GetMemoryPattern() == VraDataMemoryPattern::Dynamic_Sequential; 
            },
            [](GroupedBufferData &group, ResourceId id, const VraDataDesc &desc, const VraRawData &raw_data, const VkPhysicalDeviceProperties &props)
            {
                if (raw_data.pData_ != nullptr && raw_data.size_ > 0)
                {
                    size_t current_offset_in_group = group.total_size;
                    // Use minUniformBufferOffsetAlignment for dynamic sequential UBOs, as an example.
                    // A more robust system might get specific alignment from VraBufferDesc or a group property.
                    size_t alignment = static_cast<size_t>(props.limits.minUniformBufferOffsetAlignment);

                    if (alignment > 0)
                    { // Ensure alignment is valid
                        size_t aligned_offset_in_group = (current_offset_in_group + alignment - 1) & ~(alignment - 1);
                        size_t padding = aligned_offset_in_group - current_offset_in_group;
                        if (padding > 0)
                        {
                            group.consolidated_data.insert(group.consolidated_data.end(), padding, 0); // Add padding bytes
                            group.total_size += padding;
                        }
                    }
                    group.offsets[id] = group.total_size;

                    const uint8_t *data_ptr = static_cast<const uint8_t *>(raw_data.pData_);
                    group.consolidated_data.insert(group.consolidated_data.end(), data_ptr, data_ptr + raw_data.size_);
                    group.total_size += raw_data.size_;
                }
                else
                {
                    group.offsets[id] = group.total_size;
                }
                group.combined_usage_flags |= desc.GetBufferDesc().usage_flags_;
            });
    }

    bool VraDataCollector::CollectBufferData(VraDataDesc desc, VraRawData data, ResourceId &id)
    {
        if (buffer_desc_map_.size() >= MAX_BUFFER_COUNT)
        {
            // Optional: Log warning or error
            return false;
        }
        if (buffer_desc_map_.count(id))
        {
            // Optional: Log warning or error - ID already exists
            return false; // Don't overwrite, require unique IDs
        }
        if (data.pData_ == nullptr || data.size_ == 0)
        {
            // Optional: Log warning - Empty data collected
            // Allow collecting zero-size buffers if it's a valid use case, otherwise return false
        }

        buffer_desc_map_[id] = desc;
        buffer_data_map_[id] = data;
        return true;
    }

    bool VraDataCollector::CollectImageData(VraDataDesc desc, VraRawData data, ResourceId &id)
    {
        if (image_desc_map_.size() >= MAX_IMAGE_COUNT)
        {
            // Optional: Log warning or error
            return false;
        }
        if (image_desc_map_.count(id))
        {
            // Optional: Log warning or error - ID already exists
            return false;
        }
        // Add validation for image data/desc if necessary
        image_desc_map_[id] = desc;
        image_data_map_[id] = data;
        return true;
    }

    std::optional<std::reference_wrapper<const VraDataDesc>> VraDataCollector::GetBufferDesc(ResourceId id) const
    {
        auto it = buffer_desc_map_.find(id);
        if (it != buffer_desc_map_.end())
        {
            return std::cref(it->second);
        }
        return std::nullopt;
    }

    std::optional<std::reference_wrapper<const VraRawData>> VraDataCollector::GetBufferData(ResourceId id) const
    {
        auto it = buffer_data_map_.find(id);
        if (it != buffer_data_map_.end())
        {
            return std::cref(it->second);
        }
        return std::nullopt;
    }

    std::optional<std::reference_wrapper<const VraDataDesc>> VraDataCollector::GetImageDesc(ResourceId id) const
    {
        auto it = image_desc_map_.find(id);
        if (it != image_desc_map_.end())
        {
            return std::cref(it->second);
        }
        return std::nullopt;
    }

    std::optional<std::reference_wrapper<const VraRawData>> VraDataCollector::GetImageData(ResourceId id) const
    {
        auto it = image_data_map_.find(id);
        if (it != image_data_map_.end())
        {
            return std::cref(it->second);
        }
        return std::nullopt;
    }

    void VraDataCollector::ClearGroupedBufferData()
    {
        for (auto &strategy : registered_groups_)
        {
            strategy.grouped_data_handle.Clear();
        }
    }

    void VraDataCollector::ClearAllCollectedData()
    {
        buffer_desc_map_.clear();
        buffer_data_map_.clear();
        image_desc_map_.clear();
        image_data_map_.clear();
        ClearGroupedBufferData();
    }

    void VraDataCollector::GroupAllBufferData(const VkPhysicalDeviceProperties &physical_device_properties)
    {
        ClearGroupedBufferData();

        // Optional: Estimate sizes and reserve capacity
        std::vector<size_t> estimated_group_sizes(registered_groups_.size(), 0);
        for (const auto &buffer_pair : buffer_data_map_)
        {
            const VraRawData &raw_data = buffer_pair.second;
            auto desc_it = buffer_desc_map_.find(buffer_pair.first);
            if (desc_it == buffer_desc_map_.end())
                continue;
            const VraDataDesc &desc = desc_it->second;

            for (size_t i = 0; i < registered_groups_.size(); ++i)
            {
                if (registered_groups_[i].predicate(desc))
                {
                    // Basic estimation, doesn't account for padding in dynamic groups yet.
                    // For a more accurate estimation, the predicate or a dedicated estimation function
                    // in the strategy would be needed if padding is significant.
                    estimated_group_sizes[i] += raw_data.size_;
                    break;
                }
            }
        }
        for (size_t i = 0; i < registered_groups_.size(); ++i)
        {
            if (estimated_group_sizes[i] > 0)
            { // Only reserve if there's an estimate
                registered_groups_[i].grouped_data_handle.consolidated_data.reserve(estimated_group_sizes[i]);
            }
        }

        // Group data
        for (const auto &pair : buffer_data_map_)
        {
            ResourceId id = pair.first;
            const VraRawData &raw_data = pair.second;
            auto desc_it = buffer_desc_map_.find(id);
            if (desc_it == buffer_desc_map_.end())
                continue;
            const VraDataDesc &desc = desc_it->second;

            for (auto &strategy : registered_groups_)
            {
                if (strategy.predicate(desc))
                {
                    strategy.group_action(strategy.grouped_data_handle, id, desc, raw_data, physical_device_properties);
                    break; // Assume buffer belongs to only one group based on predicate
                }
            }
        }

        // Shrink to fit for non-dynamic groups (as per original logic)
        for (auto &strategy : registered_groups_)
        {
            // The original dynamic_sequential_group did not shrink_to_fit.
            // You might want a flag in VraBufferGroup to control this behavior.
            if (strategy.name != "dynamic_sequential_group")
            {
                strategy.grouped_data_handle.consolidated_data.shrink_to_fit();
            }
        }
    }

    size_t VraDataCollector::GetBufferOffset(ResourceId id) const
    {
        for (const auto &strategy : registered_groups_)
        {
            auto it = strategy.grouped_data_handle.offsets.find(id);
            if (it != strategy.grouped_data_handle.offsets.end())
            {
                return it->second;
            }
        }
        return std::numeric_limits<size_t>::max();
    }

    const GroupedBufferData *VraDataCollector::GetGroupData(const std::string &name) const
    {
        auto it = group_name_to_index_map_.find(name);
        if (it != group_name_to_index_map_.end())
        {
            if (it->second < registered_groups_.size())
            { // Boundary check
                return &registered_groups_[it->second].grouped_data_handle;
            }
        }
        return nullptr;
    }

    // -------------------------------------
    // --- Data Dispatcher Implementation ---
    // -------------------------------------

    bool VraDispatcher::GenerateAllBuffers(const VraDataCollector &data_collector, const VmaAllocator &vma_allocator)
    {
        auto group_names = data_collector.GetAllGroupNames();
        for (const auto &group_name : group_names)
        {
            auto group_data = data_collector.GetGroupData(group_name);
            if (!group_data || group_data->offsets.size() == 0)
                continue;
            auto data_desc = data_collector.GetBufferDesc(group_data->offsets.begin()->first).value().get();

            VkBufferCreateInfo buffer_info = {};
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_info.size = group_data->total_size;
            buffer_info.usage = group_data->combined_usage_flags;
            buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            buffer_info.queueFamilyIndexCount = data_desc.GetBufferDesc().queue_family_index_count_;
            buffer_info.pQueueFamilyIndices = data_desc.GetBufferDesc().pQueueFamilyIndices_;

            VmaAllocationCreateInfo alloc_info = {};
            alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
            alloc_info.flags = GetAllocationCreateFlags(data_desc.GetMemoryPattern());

            VkBuffer buffer;
            VmaAllocation allocation;
            VmaAllocationInfo allocation_info;
            if (vmaCreateBuffer(vma_allocator, &buffer_info, &alloc_info, &buffer, &allocation, &allocation_info) != VK_SUCCESS)
            {
                std::cerr << "Failed to create buffer" << std::endl;
                return false;
            }

            // create a managed buffer
            ManagedVmaBuffer managed_buffer{
                buffer,
                buffer_info.usage,
                allocation,
                vma_allocator,
                allocation_info};
            buffer_map_[group_name].push_back(std::move(managed_buffer));

            // create a staging buffer if needed
            if (data_desc.GetMemoryPattern() == VraDataMemoryPattern::Static_Upload)
            {
                // create a staging buffer
                VkBufferCreateInfo staging_buffer_info = {};
                staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                staging_buffer_info.size = group_data->total_size;
                staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                staging_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                staging_buffer_info.queueFamilyIndexCount = data_desc.GetBufferDesc().queue_family_index_count_;
                staging_buffer_info.pQueueFamilyIndices = data_desc.GetBufferDesc().pQueueFamilyIndices_;

                VmaAllocationCreateInfo staging_alloc_info = {};
                staging_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
                staging_alloc_info.flags = GetAllocationCreateFlags(VraDataMemoryPattern::Dynamic_Sequential);

                VkBuffer staging_buffer;
                VmaAllocation staging_allocation;
                VmaAllocationInfo staging_allocation_info;
                if (vmaCreateBuffer(vma_allocator, &staging_buffer_info, &staging_alloc_info, &staging_buffer, &staging_allocation, &staging_allocation_info) != VK_SUCCESS)
                {
                    std::cerr << "Failed to create staging buffer" << std::endl;
                    return false;
                }

                // create a managed staging buffer
                ManagedVmaBuffer managed_staging_buffer{
                    staging_buffer,
                    staging_buffer_info.usage,
                    staging_allocation,
                    vma_allocator,
                    staging_allocation_info};
                buffer_map_[group_name].push_back(std::move(managed_staging_buffer));
            }
        }
        return true;
    }

    VmaAllocationCreateFlags VraDispatcher::GetAllocationCreateFlags(VraDataMemoryPattern pattern)
    {
        switch (pattern)
        {
        case VraDataMemoryPattern::Dynamic_Sequential:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        case VraDataMemoryPattern::Dynamic_Random:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        case VraDataMemoryPattern::Static_Local:
        case VraDataMemoryPattern::Static_Upload:
        default:
            return 0;
        }
    }

    // -------------------------------------
    // --- VRA Implementation ---
    // -------------------------------------

    VRA::VRA()
    {
    }

    VRA::~VRA()
    {
    }
}
