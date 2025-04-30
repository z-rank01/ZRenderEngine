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

    bool VraDataCollector::GroupDataIfPossible()
    {
        TryGroupVertexIndexData();
        TryGroupUniformData();
        TryGroupStorageData();
        TryGroupIndirectDrawCommands();
        return true;
    }

    VraBufferInfo2Vma VraDataCollector::ParseDataToBufferInfo()
    {
        VraBufferInfo2Vma buffer_info;
        buffer_info.buffer_create_info_ = VkBufferCreateInfo();
        buffer_info.allocation_create_flags_ = VmaAllocationCreateFlags();
        buffer_info.pData_ = nullptr;
        return buffer_info;
    }

    VraImageInfo2Vma VraDataCollector::ParseDataToImageViewInfo()
    {
        VraImageInfo2Vma image_info;
        image_info.image_create_info_ = VkImageCreateInfo();
        image_info.image_view_create_info_ = VkImageViewCreateInfo();
        image_info.allocation_create_flags_ = VmaAllocationCreateFlags();
        image_info.pData_ = nullptr;
        return image_info;
    }

    bool VraDataCollector::CollectBufferData(VraDataDesc desc, RawData data)
    {
        if (next_buffer_id_ >= MAX_BUFFER_COUNT)
        {
            return false;
        }
        desc_map_[next_buffer_id_] = desc;
        data_map_[next_buffer_id_] = data;
        next_buffer_id_++;
        return true;
    }

    bool VraDataCollector::CollectImageData(VraDataDesc desc, RawData data)
    {
        if (next_image_id_ >= MAX_IMAGE_COUNT)
        {
            return false;
        }
        desc_map_[next_image_id_] = desc;
        data_map_[next_image_id_] = data;
        next_image_id_++;
        return true;
    }

    bool VraDataCollector::TryGroupVertexIndexData()
    {
        return false;
    }

    bool VraDataCollector::TryGroupUniformData()
    {
        return false;
    }

    bool VraDataCollector::TryGroupStorageData()
    {
        return false;
    }

    bool VraDataCollector::TryGroupIndirectDrawCommands()
    {
        return false;
    }
}
