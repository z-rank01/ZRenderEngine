#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h> // Include VMA header AFTER the implementation define
#include <algorithm>          // for std::find_if, std::remove_if if needed later
#include "vra.h"

namespace vra
{
    // -------------------------------------
    // --- Data Collector Implementation ---
    // -------------------------------------

    VraDataBatcher::~VraDataBatcher()
    {
        Clear();
    }

    void VraDataBatcher::RegisterBatcher(
        const BatchId group_id,
        std::function<bool(const VraDataDesc &)> predicate,
        std::function<void(ResourceId, VraBatchHandle &, const VraDataDesc &, const VraRawData &)> group_action)
    {
        // Check if group with this name already exists to prevent duplicates
        if (batch_id_to_index_map_.count(group_id))
        {
            // Optionally throw an error or log a warning
            // For now, let's overwrite or ignore if names clash, but unique names are better.
            // For simplicity, we'll assume names are unique for now or the first registration wins.
            // A more robust system might throw or return a bool.
            return;
        }
        registered_batchers_.push_back(VraBatcher{std::move(group_id), std::move(predicate), std::move(group_action)});
        batch_id_to_index_map_[registered_batchers_.back().batch_id] = registered_batchers_.size() - 1;
    }

    void VraDataBatcher::RegisterDefaultBatcher()
    {
        // Static Local Group
        RegisterBatcher(
            vra::VraBuiltInBatchIds::GPU_Only,
            [](const VraDataDesc &desc)
            {
                return desc.GetMemoryPattern() == VraDataMemoryPattern::GPU_Only;
            },
            [](ResourceId id, VraBatchHandle &group, const VraDataDesc &desc, const VraRawData &raw_data)
            {
                const auto &current_item_buffer_create_info = desc.GetBufferCreateInfo();

                if (!group.initialized)
                {
                    group.data_desc = desc; // Initialize the group's entire VraDataDesc
                    group.initialized = true;
                }
                else
                {
                    auto &group_ci_ref = group.data_desc.GetBufferCreateInfo();
                    if (current_item_buffer_create_info.usage == 0 ||
                        current_item_buffer_create_info.sharingMode != group_ci_ref.sharingMode)
                    {
                        return; // Incompatible
                    }

                    group_ci_ref.usage |= current_item_buffer_create_info.usage;
                    group_ci_ref.flags |= current_item_buffer_create_info.flags;
                    if (current_item_buffer_create_info.queueFamilyIndexCount > group_ci_ref.queueFamilyIndexCount &&
                        current_item_buffer_create_info.sharingMode == VK_SHARING_MODE_CONCURRENT)
                    {
                        group_ci_ref.queueFamilyIndexCount = current_item_buffer_create_info.queueFamilyIndexCount;
                        group_ci_ref.pQueueFamilyIndices = current_item_buffer_create_info.pQueueFamilyIndices;
                    }
                }

                if (raw_data.pData_ != nullptr && raw_data.size_ > 0)
                {
                    group.offsets[id] = group.consolidated_data.size();
                    const uint8_t *data_ptr = static_cast<const uint8_t *>(raw_data.pData_);
                    group.consolidated_data.insert(group.consolidated_data.end(), data_ptr, data_ptr + raw_data.size_);
                }
                else
                {
                    group.offsets[id] = group.consolidated_data.size();
                }

                group.data_desc.GetBufferCreateInfo().size = group.consolidated_data.size();
            });

        // Dynamic Sequential Group
        RegisterBatcher(
            "CPU_GPU",
            [](const VraDataDesc &desc)
            {
                return desc.GetMemoryPattern() == VraDataMemoryPattern::CPU_GPU;
            },
            [&physical_device_properties = this->physical_device_properties_](ResourceId id, VraBatchHandle &group, const VraDataDesc &desc, const VraRawData &raw_data)
            {
                const auto &current_item_buffer_create_info = desc.GetBufferCreateInfo();
                // Get a modifiable reference to the group's VkBufferCreateInfo
                auto &group_ci_ref = group.data_desc.GetBufferCreateInfo();

                if (!group.initialized)
                {
                    group.data_desc = desc; // Initialize the group's entire VraDataDesc
                    group.initialized = true;
                    // group_ci_ref now refers to the CI within the newly set group.data_desc
                }
                else if (current_item_buffer_create_info.usage == 0 || 
                         current_item_buffer_create_info.sharingMode != group_ci_ref.sharingMode)
                {
                    return; // Incompatible
                }
                else
                {
                    // Merge into group_ci_ref
                    group_ci_ref.usage |= current_item_buffer_create_info.usage;
                    group_ci_ref.flags |= current_item_buffer_create_info.flags;
                    if (current_item_buffer_create_info.queueFamilyIndexCount > group_ci_ref.queueFamilyIndexCount &&
                        current_item_buffer_create_info.sharingMode == VK_SHARING_MODE_CONCURRENT)
                    {
                        group_ci_ref.queueFamilyIndexCount = current_item_buffer_create_info.queueFamilyIndexCount;
                        group_ci_ref.pQueueFamilyIndices = current_item_buffer_create_info.pQueueFamilyIndices;
                    }
                }

                // Combine data with alignment
                size_t base_offset_for_item = group.consolidated_data.size();
                size_t aligned_offset_for_item = base_offset_for_item;

                // Apply alignment if the group's buffer usage suggests it (e.g., UBO, SSBO)
                if ((group_ci_ref.usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) != 0)
                {
                    size_t alignment_requirement = physical_device_properties.limits.minUniformBufferOffsetAlignment; 
                    // TODO: Consider other alignment requirements if necessary, e.g., minStorageBufferOffsetAlignment
                    if (alignment_requirement > 0)
                    {
                        aligned_offset_for_item = (base_offset_for_item + alignment_requirement - 1) & ~(alignment_requirement - 1);
                    }
                }

                size_t padding_needed = aligned_offset_for_item - base_offset_for_item;
                if (padding_needed > 0)
                {
                    group.consolidated_data.insert(group.consolidated_data.end(), padding_needed, (uint8_t)0); // Pad with zeros
                }

                group.offsets[id] = aligned_offset_for_item; // Store the correctly aligned offset

                if (raw_data.pData_ != nullptr && raw_data.size_ > 0)
                {
                    const uint8_t *data_ptr = static_cast<const uint8_t *>(raw_data.pData_);
                    group.consolidated_data.insert(group.consolidated_data.end(), data_ptr, data_ptr + raw_data.size_);
                }
                
                group_ci_ref.size = group.consolidated_data.size();
            });
    }

    bool VraDataBatcher::Collect(VraDataDesc desc, VraRawData data, ResourceId &id)
    {
        // check buffer conditions
        if (desc.GetBufferCreateInfo().usage == 0 ||
            buffer_desc_map_.size() >= MAX_BUFFER_COUNT ||
            buffer_desc_map_.count(id) ||
            data.pData_ == nullptr ||
            data.size_ == 0)
        {
            return false;
        }

        // store buffer data
        buffer_desc_map_[id] = desc;
        buffer_data_map_[id] = data;

        return true;
    }

    VkMemoryPropertyFlags VraDataBatcher::GetSuggestMemoryFlags(BatchId group_id)
    {
        if (batch_id_to_index_map_.find(group_id) == batch_id_to_index_map_.cend())
        {
            std::cerr << "Group id " << group_id << " not found" << std::endl;
            return VkMemoryPropertyFlagBits();
        }
        const VraDataDesc &data_desc = registered_batchers_[batch_id_to_index_map_[group_id]].batch_handle.data_desc;
        const VraDataMemoryPattern &data_pattern = data_desc.GetMemoryPattern();
        const VraDataUpdateRate &data_update_rate = data_desc.GetUpdateRate();
        switch (data_pattern)
        {
        case VraDataMemoryPattern::GPU_Only:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        case VraDataMemoryPattern::CPU_GPU:
            if (data_update_rate == VraDataUpdateRate::Frequent)
                return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            else if (data_update_rate == VraDataUpdateRate::RarelyOrNever)
                return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

        case VraDataMemoryPattern::GPU_CPU:
            if (data_update_rate == VraDataUpdateRate::Frequent)
                return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            else if (data_update_rate == VraDataUpdateRate::RarelyOrNever)
                return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

        case VraDataMemoryPattern::SOC:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

        case VraDataMemoryPattern::Stream_Ring:
            if (data_update_rate == VraDataUpdateRate::Frequent)
                return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            else if (data_update_rate == VraDataUpdateRate::RarelyOrNever)
                return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

        default:
            return VkMemoryPropertyFlagBits();
        }
    }

    VmaAllocationCreateFlags VraDataBatcher::GetSuggestVmaMemoryFlags(BatchId group_id)
    {
        if (batch_id_to_index_map_.find(group_id) == batch_id_to_index_map_.cend())
        {
            std::cerr << "Group id " << group_id << " not found" << std::endl;
            return VmaAllocationCreateFlags();
        }
        const VraDataDesc &data_desc = registered_batchers_[batch_id_to_index_map_[group_id]].batch_handle.data_desc;
        const VraDataMemoryPattern &data_pattern = data_desc.GetMemoryPattern();
        const VraDataUpdateRate &data_update_rate = data_desc.GetUpdateRate();
        switch (data_pattern)
        {
        case VraDataMemoryPattern::GPU_Only:
            return VmaAllocationCreateFlags();

        case VraDataMemoryPattern::CPU_GPU:
            if (data_update_rate == VraDataUpdateRate::Frequent)
                return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            else if (data_update_rate == VraDataUpdateRate::RarelyOrNever)
                return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        case VraDataMemoryPattern::GPU_CPU:
            if (data_update_rate == VraDataUpdateRate::Frequent)
                return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            else if (data_update_rate == VraDataUpdateRate::RarelyOrNever)
                return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

        case VraDataMemoryPattern::SOC:
            return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        case VraDataMemoryPattern::Stream_Ring:
            if (data_update_rate == VraDataUpdateRate::Frequent)
                return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            else if (data_update_rate == VraDataUpdateRate::RarelyOrNever)
                return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        default:
            return VmaAllocationCreateFlags();
        }
    }

    void VraDataBatcher::ClearBatch()
    {
        for (auto &strategy : registered_batchers_)
        {
            strategy.batch_handle.Clear();
        }
    }

    void VraDataBatcher::Clear()
    {
        buffer_desc_map_.clear();
        buffer_data_map_.clear();
        ClearBatch();
    }

    void VraDataBatcher::Batch()
    {
        ClearBatch();

        // Optional: Estimate sizes and reserve capacity
        std::vector<size_t> estimated_group_sizes(registered_batchers_.size(), 0);
        for (const auto &buffer_pair : buffer_data_map_)
        {
            const VraRawData &raw_data = buffer_pair.second;
            auto desc_it = buffer_desc_map_.find(buffer_pair.first);
            if (desc_it == buffer_desc_map_.end())
                continue;
            const VraDataDesc &desc = desc_it->second;

            for (size_t i = 0; i < registered_batchers_.size(); ++i)
            {
                if (registered_batchers_[i].predicate(desc))
                {
                    // Basic estimation, doesn't account for padding in dynamic groups yet.
                    // For a more accurate estimation, the predicate or a dedicated estimation function
                    // in the strategy would be needed if padding is significant.
                    estimated_group_sizes[i] += raw_data.size_;
                    break;
                }
            }
        }
        for (size_t i = 0; i < registered_batchers_.size(); ++i)
        {
            if (estimated_group_sizes[i] > 0)
            { // Only reserve if there's an estimate
                registered_batchers_[i].batch_handle.consolidated_data.reserve(estimated_group_sizes[i]);
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

            for (auto &strategy : registered_batchers_)
            {
                if (strategy.predicate(desc))
                {
                    strategy.batch_method(id, strategy.batch_handle, desc, raw_data);
                    break; // Assume buffer belongs to only one group based on predicate
                }
            }
        }

        // Shrink to fit for non-dynamic groups (as per original logic)
        for (auto &strategy : registered_batchers_)
        {
            // The original dynamic_sequential_group did not shrink_to_fit.
            // You might want a flag in VraBatcher to control this behavior.
            if (strategy.batch_id != "CPU_GPU")
            {
                strategy.batch_handle.consolidated_data.shrink_to_fit();
            }
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
