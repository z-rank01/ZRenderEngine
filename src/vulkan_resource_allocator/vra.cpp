#include "vra.h"

namespace vra
{
    VRA::VRA()
    {
    }

    VRA::~VRA()
    {
    }

    // --- Data Collector Implementation ---
    VraDataCollector::VraDataCollector()
    {
    }

    VraDataCollector::~VraDataCollector()
    {
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
        static_local_group_.Clear();
        static_upload_group_.Clear();
        dynamic_sequential_group_.Clear();
        // Clear other groups if added
    }

    void VraDataCollector::ClearAllCollectedData()
    {
        buffer_desc_map_.clear();
        buffer_data_map_.clear();
        image_desc_map_.clear();
        image_data_map_.clear();
        ClearGroupedBufferData();
    }

    void VraDataCollector::GroupAllBufferData()
    {
        ClearGroupedBufferData(); // Start fresh

        // 为合并向量预留空间以提高效率
        size_t estimated_static_local_size = 0;
        size_t estimated_static_upload_size = 0;
        size_t estimated_dynamic_sequential_size = 0;

        // 计算每种内存模式的大致总大小
        for (const auto &pair : buffer_data_map_)
        {
            const ResourceId id = pair.first;
            const VraRawData &data = pair.second;

            auto desc_it = buffer_desc_map_.find(id);
            if (desc_it != buffer_desc_map_.end())
            {
                const VraDataDesc &desc = desc_it->second;

                switch (desc.GetMemoryPattern())
                {
                case VraDataMemoryPattern::Static_Local:
                    estimated_static_local_size += data.size_;
                    break;
                case VraDataMemoryPattern::Static_Upload:
                    estimated_static_upload_size += data.size_;
                    break;
                case VraDataMemoryPattern::Dynamic_Sequential:
                    estimated_dynamic_sequential_size += data.size_;
                    break;
                default:
                    // 其他内存模式暂不处理
                    break;
                }
            }
        }

        // 为各组预留空间
        static_local_group_.consolidated_data.reserve(estimated_static_local_size);
        static_upload_group_.consolidated_data.reserve(estimated_static_upload_size);
        dynamic_sequential_group_.consolidated_data.reserve(estimated_dynamic_sequential_size);

        GroupStaticLocalData();
        GroupStaticUploadData();
        GroupDynamicSeqData();
        // Call other grouping methods if added
    }

    // --- Grouping Implementations ---

    void VraDataCollector::GroupStaticLocalData()
    {
        GroupedBufferData &group = static_local_group_; // Alias for convenience

        for (const auto &pair : buffer_desc_map_)
        {
            ResourceId id = pair.first;
            const VraDataDesc &desc = pair.second;

            if (desc.GetMemoryPattern() == VraDataMemoryPattern::Static_Local)
            {
                const VraRawData &raw_data = buffer_data_map_.at(id); // Use .at() for bounds checking if needed

                if (raw_data.pData_ != nullptr && raw_data.size_ > 0)
                {
                    // Record offset *before* appending data
                    group.offsets[id] = group.total_size;

                    // Append data
                    const uint8_t *data_ptr = static_cast<const uint8_t *>(raw_data.pData_);
                    group.consolidated_data.insert(group.consolidated_data.end(), data_ptr, data_ptr + raw_data.size_);

                    // Update total size
                    group.total_size += raw_data.size_;

                    // Combine usage flags
                    group.combined_usage_flags |= desc.GetBufferDesc().usage_flags_;
                }
                else
                {
                    // Handle zero-size data: Assign offset but don't copy data.
                    // Useful if a resource exists but has no data this frame/batch.
                    group.offsets[id] = group.total_size;
                    // Don't increment total_size or copy data.
                    // Still combine usage flags if necessary for the group buffer.
                    group.combined_usage_flags |= desc.GetBufferDesc().usage_flags_;
                }
            }
        }
        group.consolidated_data.shrink_to_fit();
    }

    void VraDataCollector::GroupStaticUploadData()
    {
        GroupedBufferData &group = static_upload_group_;

        for (const auto &pair : buffer_desc_map_)
        {
            ResourceId id = pair.first;
            const VraDataDesc &desc = pair.second;
            if (desc.GetMemoryPattern() != VraDataMemoryPattern::Static_Upload)
                continue;

            const VraRawData &raw_data = buffer_data_map_.at(id);
            if (raw_data.pData_ != nullptr && raw_data.size_ > 0)
            {
                group.offsets[id] = group.total_size;
                const uint8_t *data_ptr = static_cast<const uint8_t *>(raw_data.pData_);
                group.consolidated_data.insert(group.consolidated_data.end(), data_ptr, data_ptr + raw_data.size_);
                group.total_size += raw_data.size_;
                group.combined_usage_flags |= desc.GetBufferDesc().usage_flags_;
            }
            else
            {
                group.offsets[id] = group.total_size;
                group.combined_usage_flags |= desc.GetBufferDesc().usage_flags_;
            }
        }
        group.consolidated_data.shrink_to_fit();
    }

    void VraDataCollector::GroupDynamicSeqData()
    {
        GroupedBufferData &group = dynamic_sequential_group_;

        for (const auto &pair : buffer_desc_map_)
        {
            ResourceId id = pair.first;
            const VraDataDesc &desc = pair.second;
            if (desc.GetMemoryPattern() != VraDataMemoryPattern::Dynamic_Sequential)continue;

            const VraRawData &raw_data = buffer_data_map_.at(id);
            if (raw_data.pData_ != nullptr && raw_data.size_ > 0)
            {
                // Alignment might be important for dynamic uniform buffers
                // Consider adding alignment padding here if needed, based on device limits.
                // size_t current_offset = group.total_size;
                // size_t alignment = GetAlignmentForBuffer(desc.GetBufferDesc()); // Need a function for this
                // size_t aligned_offset = (current_offset + alignment - 1) & ~(alignment - 1);
                // size_t padding = aligned_offset - current_offset;
                // if (padding > 0) {
                //     group.consolidated_data.insert(group.consolidated_data.end(), padding, 0); // Add padding bytes
                //     group.total_size += padding;
                // }
                // group.offsets[id] = group.total_size; // Offset after alignment

                // --- Simplified version without alignment handling during grouping ---
                // Alignment should be handled when sub-allocating or binding if using dynamic offsets,
                // or ensure the *entire* grouped buffer meets max alignment reqs.
                group.offsets[id] = group.total_size;
                // --- End Simplified ---

                const uint8_t *data_ptr = static_cast<const uint8_t *>(raw_data.pData_);
                group.consolidated_data.insert(group.consolidated_data.end(), data_ptr, data_ptr + raw_data.size_);
                group.total_size += raw_data.size_;
                group.combined_usage_flags |= desc.GetBufferDesc().usage_flags_;
            }
            else
            {
                group.offsets[id] = group.total_size; // Assign offset even for zero size
                group.combined_usage_flags |= desc.GetBufferDesc().usage_flags_;
            }
        }
        // group.consolidated_data.shrink_to_fit();
    }

    size_t VraDataCollector::GetBufferOffset(ResourceId id) const
    {
        // Check each group for the ID
        auto it_sl = static_local_group_.offsets.find(id);
        if (it_sl != static_local_group_.offsets.end())
            return it_sl->second;

        auto it_su = static_upload_group_.offsets.find(id);
        if (it_su != static_upload_group_.offsets.end())
            return it_su->second;

        auto it_ds = dynamic_sequential_group_.offsets.find(id);
        if (it_ds != dynamic_sequential_group_.offsets.end())
            return it_ds->second;

        // Check other groups if added...

        // Not found in any group
        return std::numeric_limits<size_t>::max(); // Indicate not found
    }
}
