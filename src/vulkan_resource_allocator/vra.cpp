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

    VraBufferInfo2Vma VraDataCollector::ParseDataToBufferInfo()
    {
        return VraBufferInfo2Vma();
    }

    VraImageInfo2Vma VraDataCollector::ParseDataToImageViewInfo()
    {
        return VraImageInfo2Vma();
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
}
