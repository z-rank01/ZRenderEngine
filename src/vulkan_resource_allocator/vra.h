#pragma once

#include "pool.h"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

namespace vra
{
    using ResourceId = uint64_t;


    enum class VraDataMemoryPattern
    {
        // Static Mode
        Static_GPU_Only, // Device Local, no CPU access
        Static_Upload,   // CPU write once, GPU read multiple times (use staging)

        // Dynamic Mode
        Dynamic_Sequential, // sequential access, e.g. UBO update
        Dynamic_Random,     // random access, automatically enable Host Cached

        // Special Usage
        Stream_Ring, // ring buffer mode
        Readback     // GPU write, CPU read
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

        enum class SyncStrategy
        {
            SingleBuffer, // single buffer (need synchronization)
            DoubleBuffer, // double buffer (CPU/GPU parallel)
            TripleBuffer, // triple buffer (minimum waiting)
            RingBuffer    // ring buffer (stream data)
        };

        UpdateFrequency frequency = UpdateFrequency::RarelyOrNever;
        SyncStrategy syncStrategy = SyncStrategy::SingleBuffer;
        bool allowPersistentMapping = false; // allow persistent mapping
    };

    struct VraBufferDesc
    {
        VkBufferUsageFlags usage_flags_;
        VkShaderStageFlags stage_flags_;
        VkSharingMode sharing_mode_;
        uint32_t queue_family_index_count_;
        const uint32_t *pQueueFamilyIndices_;
    };

    struct VraImageDesc
    {
        VkImageUsageFlags usage_flags_;
        VkSharingMode sharing_mode_;
    };

    struct RawData
    {
        void *pData_;
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
            desc.data_behavior_.syncStrategy = VraDataBehavior::SyncStrategy::SingleBuffer;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            desc.buffer_desc_.stage_flags_ = VK_SHADER_STAGE_VERTEX_BIT;
            return desc;
        }

        static VraDataDesc DynamicUniform()
        {
            VraDataDesc desc;
            desc.data_pattern_ = VraDataMemoryPattern::Dynamic_Sequential;
            desc.data_behavior_.frequency = VraDataBehavior::UpdateFrequency::PerFrame;
            desc.data_behavior_.syncStrategy = VraDataBehavior::SyncStrategy::DoubleBuffer;
            desc.data_behavior_.allowPersistentMapping = true;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            desc.buffer_desc_.stage_flags_ = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            return desc;
        }

        static VraDataDesc IndirectDrawCommands()
        {
            VraDataDesc desc;
            desc.data_pattern_ = VraDataMemoryPattern::Stream_Ring;
            desc.data_behavior_.frequency = VraDataBehavior::UpdateFrequency::PerFrame;
            desc.data_behavior_.syncStrategy = VraDataBehavior::SyncStrategy::RingBuffer;
            desc.data_behavior_.allowPersistentMapping = true;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            desc.buffer_desc_.stage_flags_ = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
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
        bool CollectBufferData(VraDataDesc desc, RawData data);
        bool CollectImageData(VraDataDesc desc, RawData data);

        // --- Data Access ---
        const VraDataDesc &GetBufferDesc(ResourceId id) const { return desc_map_.at(id); }
        const VraDataDesc &GetImageDesc(ResourceId id) const { return desc_map_.at(id); }
        const RawData &GetBufferData(ResourceId id) const { return data_map_.at(id); }
        const RawData &GetImageData(ResourceId id) const { return data_map_.at(id); }

    private:
        std::unordered_map<ResourceId, VraDataDesc> desc_map_;
        std::unordered_map<ResourceId, RawData> data_map_;
        ResourceId next_buffer_id_ = 0;
        ResourceId next_image_id_ = 0;

        static constexpr size_t MAX_BUFFER_COUNT = 1024;
        static constexpr size_t MAX_IMAGE_COUNT = 1024;

        VraBufferInfo2Vma ParseDataToBufferInfo();
        VraImageInfo2Vma ParseDataToImageViewInfo();
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