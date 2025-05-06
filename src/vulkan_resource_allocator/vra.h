#pragma once

#include "pool.h"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cassert>
#include <stdexcept>
#include <utility>
#include <limits>

namespace vra
{
    using ResourceId = uint64_t;

    // --- Forward Declarations ---
    class VraDataDesc;
    struct VraRawData;

    // --- Grouped Buffer Data Structure ---
    struct GroupedBufferData
    {
        std::vector<uint8_t> consolidated_data;
        std::unordered_map<ResourceId, size_t> offsets; // Map ResourceId to its offset in consolidated_data
        size_t total_size = 0;
        VkBufferUsageFlags combined_usage_flags = 0; // Combined usage flags for the group buffer

        void Clear()
        {
            consolidated_data.clear();
            offsets.clear();
            total_size = 0;
            combined_usage_flags = 0;
        }
    };


    enum class VraDataMemoryPattern
    {
        // Static Mode
        Static_Local,       // Device Local, no CPU access
        Static_Upload,      // CPU write once, GPU read multiple times (e.g. staging buffer)

        // Dynamic Mode
        Dynamic_Sequential, // sequential access, e.g. UBO update
        Dynamic_Random,     // random access, automatically enable Host Cached

        // Special Usage
        Stream_Ring,        // ring buffer mode
        Readback            // GPU write, CPU read
    };

    enum class VraGroupType
    {
        Vertex_Index, 
        Uniform, 
        Storage
    };

    // Buffer Behavior Configuration
    struct VraDataBehavior
    {
        enum class UpdateFrequency
        {
            PerFrame,     // update per frame
            Occasional,   // update occasionally
            RarelyOrNever // update rarely or never
        };

        UpdateFrequency frequency = UpdateFrequency::RarelyOrNever;
        bool allowPersistentMapping = false; // allow persistent mapping
    };

    struct VraBufferDesc
    {
        VkBufferUsageFlags usage_flags_;
        VkSharingMode sharing_mode_;
        uint32_t queue_family_index_count_;
        uint32_t *pQueueFamilyIndices_;
    };

    struct VraImageDesc
    {
        VkImageUsageFlags usage_flags_;
        VkSharingMode sharing_mode_;
    };

    struct VraRawData
    {
        const void *pData_;
        size_t size_;
    };

    class VraDataDesc
    {
    public:
        VraDataDesc() = default;
        VraDataDesc(VraDataMemoryPattern pattern, VraDataBehavior behavior, VraBufferDesc buffer_desc, VraImageDesc image_desc)
            : data_pattern_(pattern), data_behavior_(behavior), buffer_desc_(buffer_desc), image_desc_(image_desc) { }
        ~VraDataDesc() = default;

        // --- Copy and Move ---
        VraDataDesc(const VraDataDesc &other) = default;
        VraDataDesc &operator=(const VraDataDesc &other) = default;
        VraDataDesc(VraDataDesc &&other) noexcept = default;
        VraDataDesc &operator=(VraDataDesc &&other) noexcept = default;

        // --- Getters ---
        VraDataMemoryPattern GetMemoryPattern() const { return data_pattern_; }
        VraDataBehavior GetDataBehavior() const { return data_behavior_; }
        const VraBufferDesc &GetBufferDesc() const { return buffer_desc_; }
        const VraImageDesc &GetImageDesc() const { return image_desc_; }

    private:
        VraDataMemoryPattern data_pattern_;
        VraDataBehavior data_behavior_;
        VraBufferDesc buffer_desc_;
        VraImageDesc image_desc_;

            // preset configurations
        static VraDataDesc StaticVertex()
        {
            VraDataDesc desc;
            desc.data_pattern_ = VraDataMemoryPattern::Static_Upload;
            desc.data_behavior_.frequency = VraDataBehavior::UpdateFrequency::RarelyOrNever;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            return desc;
        }

        static VraDataDesc DynamicUniform()
        {
            VraDataDesc desc;
            desc.data_pattern_ = VraDataMemoryPattern::Dynamic_Sequential;
            desc.data_behavior_.frequency = VraDataBehavior::UpdateFrequency::PerFrame;
            desc.data_behavior_.allowPersistentMapping = true;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            return desc;
        }

        static VraDataDesc IndirectDrawCommands()
        {
            VraDataDesc desc;
            desc.data_pattern_ = VraDataMemoryPattern::Stream_Ring;
            desc.data_behavior_.frequency = VraDataBehavior::UpdateFrequency::PerFrame;
            desc.data_behavior_.allowPersistentMapping = true;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            return desc;
        }
    };

    struct VraBufferInfo2Vma
    {
        VkBufferCreateInfo buffer_create_info_;
        VmaAllocationCreateFlags allocation_create_flags_;
        void *pData_;
    };

    struct VraImageInfo2Vma
    {
        VkImageCreateInfo image_create_info_;
        VkImageViewCreateInfo image_view_create_info_;
        VmaAllocationCreateFlags allocation_create_flags_;
        void *pData_;
    };

    class ManagedVmaBuffer
    {
        // vulkan native
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VkBufferUsageFlags usage_ = 0;

        // vma allocations
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE; // neccessary for destruction
        VmaAllocationInfo allocationInfo_{};      // store allocation info (size, mapped pointer, etc.)

        // constructor
        ManagedVmaBuffer(VkBuffer buf, VkBufferUsageFlags usage, VmaAllocation alloc, VmaAllocator allocatr, const VmaAllocationInfo &info)
            : buffer_(buf), usage_(usage), allocation_(alloc), allocator_(allocatr), allocationInfo_(info)
        {
            assert(allocator_ != VK_NULL_HANDLE && "Allocator must be valid for destruction!");
        }
        // default destructor - cleanup work is done by shared_ptr's deleter
        ~ManagedVmaBuffer() = default;

        // --- operation ---
        ManagedVmaBuffer(const ManagedVmaBuffer &) = delete;
        ManagedVmaBuffer &operator=(const ManagedVmaBuffer &) = delete;
        ManagedVmaBuffer(ManagedVmaBuffer &&other) noexcept
            : buffer_(std::exchange(other.buffer_, VK_NULL_HANDLE)),
              usage_(std::exchange(other.usage_, 0)),
              allocation_(std::exchange(other.allocation_, VK_NULL_HANDLE)),
              allocator_(std::exchange(other.allocator_, VK_NULL_HANDLE)), // transfer ownership
              allocationInfo_(other.allocationInfo_)                       // VmaAllocationInfo is copyable/movable
        {
        }
        ManagedVmaBuffer &operator=(ManagedVmaBuffer &&other) noexcept
        {
            if (this != &other)
            {
                if (buffer_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE)
                {
                    vmaDestroyBuffer(allocator_, buffer_, allocation_);
                }

                buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
                usage_ = std::exchange(other.usage_, 0);
                allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
                allocator_ = std::exchange(other.allocator_, VK_NULL_HANDLE);
                allocationInfo_ = other.allocationInfo_;
            }
            return *this;
        }

        // --- Helper methods (example) ---
        VkBuffer Get() const { return buffer_; }
        VkDeviceSize GetSize() const { return allocationInfo_.size; }
        VkBufferUsageFlags GetUsage() const { return usage_; }
    };

    class ManagedVmaImage
    {
        // vulkan native
        VkImage image_ = VK_NULL_HANDLE;
        VkImageView imageView_ = VK_NULL_HANDLE;
        VkFormat format_ = VK_FORMAT_UNDEFINED;
        VkExtent3D extent_ = {};
        VkDevice device_ = VK_NULL_HANDLE; // handle for destruction

        // vma allocations
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo_{};

        // constructor
        ManagedVmaImage(VkImage img, VkImageView view, VkFormat fmt, VkExtent3D ext, VmaAllocation alloc, VmaAllocator allocatr, VkDevice dev, const VmaAllocationInfo &info)
            : image_(img), imageView_(view), format_(fmt), extent_(ext), allocation_(alloc), allocator_(allocatr), device_(dev), allocationInfo_(info)
        {
            assert(allocator_ != VK_NULL_HANDLE && "Allocator must be valid for destruction!");
            assert(device_ != VK_NULL_HANDLE && "Device must be valid for ImageView destruction!");
        }

        // default destructor - cleanup work is done by shared_ptr's deleter
        ~ManagedVmaImage() = default;

        // --- operation ---
        ManagedVmaImage(const ManagedVmaImage &) = delete;
        ManagedVmaImage &operator=(const ManagedVmaImage &) = delete;
        ManagedVmaImage(ManagedVmaImage &&other) noexcept
            : image_(std::exchange(other.image_, VK_NULL_HANDLE)),
              imageView_(std::exchange(other.imageView_, VK_NULL_HANDLE)),
              format_(std::exchange(other.format_, VK_FORMAT_UNDEFINED)),
              extent_(std::exchange(other.extent_, {})),
              allocation_(std::exchange(other.allocation_, VK_NULL_HANDLE)),
              allocator_(std::exchange(other.allocator_, VK_NULL_HANDLE)),
              device_(std::exchange(other.device_, VK_NULL_HANDLE)),
              allocationInfo_(other.allocationInfo_)
        {
        }

        ManagedVmaImage &operator=(ManagedVmaImage &&other) noexcept
        {
            if (this != &other)
            {
                image_ = std::exchange(other.image_, VK_NULL_HANDLE);
                imageView_ = std::exchange(other.imageView_, VK_NULL_HANDLE);
                format_ = std::exchange(other.format_, VK_FORMAT_UNDEFINED);
                extent_ = std::exchange(other.extent_, {});
                allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
                allocator_ = std::exchange(other.allocator_, VK_NULL_HANDLE);
                device_ = std::exchange(other.device_, VK_NULL_HANDLE);
                allocationInfo_ = other.allocationInfo_;
            }
            return *this;
        }

        // --- Helper methods (example) ---
        VkImage GetImage() const { return image_; }
        VkImageView GetView() const { return imageView_; }
        VkFormat GetFormat() const { return format_; }
        VkExtent3D GetExtent() const { return extent_; }
        VkDeviceSize GetSize() const { return allocationInfo_.size; }
    };

    class VraDataCollector
    {
    public:
        VraDataCollector();
        ~VraDataCollector();

        // --- Data Collection ---
        /**
         * @brief Collects buffer data description and raw data pointer.
         * @param id A unique ResourceId for this buffer data.
         * @param desc Description of the buffer data (usage, memory pattern, etc.).
         * @param data Raw data pointer and size. The pointer pData_ must remain valid until GroupAllBufferData is called.
         * @return True if collected successfully, false if ID already exists or max count reached.
         */
        bool CollectBufferData(VraDataDesc desc, VraRawData data, ResourceId& id);

        /**
         * @brief Collects image data description and raw data pointer. (Not fully implemented in grouping)
         * @param id A unique ResourceId for this image data.
         * @param desc Description of the image data.
         * @param data Raw data pointer and size.
         * @return True if collected successfully, false if ID already exists or max count reached.
         */
        bool CollectImageData(VraDataDesc desc, VraRawData data, ResourceId& id);

        // --- Data Processing ---
        /**
         * @brief Processes all collected buffer data, grouping them by memory pattern.
         * Must be called after collecting data for a batch/frame and before accessing grouped data.
         */
        void GroupAllBufferData();

        // --- Data Access ---
        // Access individual resource data (usually before grouping)
        std::optional<std::reference_wrapper<const VraDataDesc>> GetBufferDesc(ResourceId id) const;
        std::optional<std::reference_wrapper<const VraRawData>> GetBufferData(ResourceId id) const;
        std::optional<std::reference_wrapper<const VraDataDesc>> GetImageDesc(ResourceId id) const; // Declaration added
        std::optional<std::reference_wrapper<const VraRawData>> GetImageData(ResourceId id) const;  // Declaration added

        /**
         * @brief Gets the offset of a specific buffer resource within its consolidated group buffer.
         * Call this *after* GroupAllBufferData().
         * @param id The ResourceId of the buffer.
         * @return The offset in bytes, or std::numeric_limits<size_t>::max() if not found (e.g., ID invalid or not grouped).
         */
        size_t GetBufferOffset(ResourceId id) const;

        // Access grouped data (call *after* GroupAllBufferData)
        const GroupedBufferData &GetStaticLocalGroup() const { return static_local_group_; }
        const GroupedBufferData &GetStaticUploadGroup() const { return static_upload_group_; }
        const GroupedBufferData &GetDynamicSequentialGroup() const { return dynamic_sequential_group_; }

        /**
         * @brief Clears all collected buffer and image data, and resets grouping results.
         */
        void ClearAllCollectedData();

    private:
        // Buffer specific storage
        std::unordered_map<ResourceId, VraDataDesc> buffer_desc_map_;
        std::unordered_map<ResourceId, VraRawData> buffer_data_map_;

        // Image specific storage
        std::unordered_map<ResourceId, VraDataDesc> image_desc_map_;
        std::unordered_map<ResourceId, VraRawData> image_data_map_;

        // --- Limits ---
        // Consider making these configurable
        static constexpr size_t MAX_BUFFER_COUNT = 4096;
        static constexpr size_t MAX_IMAGE_COUNT = 1024;

        // Grouped data structures
        GroupedBufferData static_local_group_;
        GroupedBufferData static_upload_group_;
        GroupedBufferData dynamic_sequential_group_;
        // Add groups for other patterns (Dynamic_Random, Stream_Ring, Readback) if needed later

        // --- Private Grouping Methods ---
        void GroupStaticLocalData();
        void GroupStaticUploadData();
        void GroupDynamicSeqData();
        // Add grouping methods for other patterns if needed later

        // Helper to clear groups before regrouping
        void ClearGroupedBufferData();
    };

    class VraDescriptorAllocator
    {
        VkPhysicalDeviceProperties gpu_properties_;
        VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;

        // constructor
        VraDescriptorAllocator(VkPhysicalDeviceProperties gpu_properties, VkDevice device)
            : gpu_properties_(gpu_properties), device_(device)
        {
        }

        // --- Helper methods ---
        size_t AlignUniformBufferSize(size_t original_size)
        {
            // Calculate required alignment based on minimum device offset alignment
            size_t min_ubo_alignment = gpu_properties_.limits.minUniformBufferOffsetAlignment;
            size_t aligned_size = original_size;
            if (min_ubo_alignment > 0)
            {
                aligned_size = (aligned_size + min_ubo_alignment - 1) & ~(min_ubo_alignment - 1);
            }
            return aligned_size;
        }

        // --- Descriptor methods ---
        VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout layout)
        {
            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = descriptor_pool_;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &layout;

            VkDescriptorSet descriptor_set;
            VkResult result = vkAllocateDescriptorSets(device_, &alloc_info, &descriptor_set);
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate descriptor set!");
            }

            return descriptor_set;
        }
    };

    class VRA
    {
    public:
        VRA();
        ~VRA();
    };
}