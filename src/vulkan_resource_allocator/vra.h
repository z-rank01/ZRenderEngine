#pragma once

#include "pool.h"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace vra
{
    class ManagedVmaBuffer
    {
        // vulkan native
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VkBufferUsageFlags usage_ = 0;

        // vma allocations
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;    // neccessary for destruction
        VmaAllocationInfo allocationInfo_{};         // store allocation info (size, mapped pointer, etc.)

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
              allocator_(std::exchange(other.allocator_, VK_NULL_HANDLE)),      // transfer ownership
              allocationInfo_(other.allocationInfo_)                                          // VmaAllocationInfo is copyable/movable
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

    class DescriptorAllocator
    {
        VkPhysicalDeviceProperties gpu_properties_;
        VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;

        // constructor
        DescriptorAllocator(VkPhysicalDeviceProperties gpu_properties, VkDevice device)
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