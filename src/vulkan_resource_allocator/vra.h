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
#include <functional> // Required for std::function
#include <string>     // Required for std::string
#include <map>        // Required for std::map
#include <iostream>
namespace vra
{
    using ResourceId = uint64_t;

    // --- Forward Declarations ---
    class VraDataDesc;
    struct VraRawData;

    // --- Data Memory Pattern ---
    enum class VraDataMemoryPattern
    {
        // Default
        Default,

        // - Device Local, no CPU access
        GPU_Only,

        // - CPU and GPU sequential access, e.g. UBO update
        CPU_GPU,

        // - CPU and GPU random access; 
        // - Always has Host-Cached
        GPU_CPU,

        // - CPU and GPU access with unified memory architecture
        SOC,

        // - ring buffer mode, e.g. indirect draw command; 
        // - Always has Host-Cached
        Stream_Ring,
    };

    // --- Data Update Rate ---
    enum class VraDataUpdateRate
    {
        // Default
        Default,

        // - Update frequently, e.g. UBO update; 
        // - Always has Host-Coherent
        Frequent,

        // - Update rarely or never, e.g. static and uniform data;
        // - Require to flush memory before using if there is a transfer
        RarelyOrNever
    };

    struct VraRawData
    {
        const void *pData_ = nullptr;
        size_t size_ = 0;
    };

    class VraDataDesc
    {
    public:
        VraDataDesc() = default;
        VraDataDesc(VraDataMemoryPattern pattern,
                    VraDataUpdateRate update_rate,
                    VkBufferCreateInfo buffer_create_info = VkBufferCreateInfo{})
            : data_pattern_(pattern), data_update_rate_(update_rate), buffer_create_info_(buffer_create_info)
        {
        }
        ~VraDataDesc() = default;

        // --- Copy and Move ---
        VraDataDesc(const VraDataDesc &other) = default;
        VraDataDesc &operator=(const VraDataDesc &other) = default;
        VraDataDesc(VraDataDesc &&other) noexcept = default;
        VraDataDesc &operator=(VraDataDesc &&other) noexcept = default;

        // --- Getter Method ---
        VraDataMemoryPattern GetMemoryPattern() const { return data_pattern_; }
        VraDataUpdateRate GetUpdateRate() const { return data_update_rate_; }
        const VkBufferCreateInfo& GetBufferCreateInfo() const { return buffer_create_info_; }
        VkBufferCreateInfo& GetBufferCreateInfo() { return buffer_create_info_; }

        // --- presets ---
        static VraDataDesc StaticVertex()
        {
            VraDataDesc data_desc;
            data_desc.data_pattern_ = VraDataMemoryPattern::GPU_Only;
            data_desc.buffer_create_info_.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            return data_desc;
        }
        static VraDataDesc StaticIndex()
        {
            VraDataDesc data_desc;
            data_desc.data_pattern_ = VraDataMemoryPattern::GPU_Only;
            data_desc.buffer_create_info_.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            return data_desc;
        }
        static VraDataDesc DynamicUniform()
        {
            VraDataDesc data_desc;
            data_desc.data_pattern_ = VraDataMemoryPattern::CPU_GPU;
            data_desc.buffer_create_info_.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            return data_desc;
        }
        static VraDataDesc IndirectDrawCommands()
        {
            VraDataDesc data_desc;
            data_desc.data_pattern_ = VraDataMemoryPattern::Stream_Ring;
            data_desc.buffer_create_info_.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            return data_desc;
        }

    private:
        // --- main members ---
        VraDataMemoryPattern data_pattern_;
        VraDataUpdateRate data_update_rate_;
        VkBufferCreateInfo buffer_create_info_;
    };

#pragma region Group Data Defnition
    // --- Grouped Buffer Data Structure (public for VraDataGroup) ---
    struct VraGroupedDataHandle
    {
        bool initialized = false;
        std::vector<uint8_t> consolidated_data;
        std::unordered_map<ResourceId, size_t> offsets;
        VraDataDesc data_desc;

        void Clear()
        {
            initialized = false;
            consolidated_data.clear();
            offsets.clear();
            data_desc = VraDataDesc{};
        }
    };

    // --- Buffer Grouping Strategy Definition ---
    struct VraDataGroup {
        std::string name;
        std::function<bool(const VraDataDesc&)> predicate;
        std::function<void(ResourceId id, 
                           VraGroupedDataHandle& group, 
                           const VraDataDesc& data_desc, 
                           const VraRawData& data)> group_method;
        VraGroupedDataHandle grouped_data_handle; // Each strategy instance owns its data
    };
#pragma endregion

    class VraDataBatcher
    {
    public:
        VraDataBatcher() = delete;
        VraDataBatcher(VkPhysicalDevice physical_device) : physical_device_handle_(physical_device) 
        {
            vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties_);
            RegisterDefaultGroups();
        }
        ~VraDataBatcher();

        // --- Data Process ---

        /// @brief Collects buffer data description and raw data pointer.
        /// @param data_desc Description of the buffer data (usage, memory pattern, etc.).
        /// @param data Raw data pointer and size. The pointer pData_ must remain valid until Execute is called.
        /// @return True if collected successfully, false if ID already exists or max count reached.
        bool Collect(VraDataDesc data_desc, VraRawData data, ResourceId& id);

        /// @brief processes all collected buffer data, grouping them by memory pattern
        void Group();

        /// @brief clear all collected data and grouped data
        void Clear();

        // --- Data Access ---

        /// @brief register buffer group strategy
        /// @param name register name
        /// @param predicate predicate before grouping
        /// @param group_method action when grouping
        void RegisterDataGroup(
            std::string name,
            std::function<bool(const VraDataDesc &)> predicate,
            std::function<void(ResourceId id, VraGroupedDataHandle &group, const VraDataDesc &data_desc, const VraRawData &data)> group_method);

        /// @brief get group index
        /// @param name group name
        /// @return group index
        size_t GetGroupIndex(const std::string &name) const
        {
            auto it = group_name_to_index_map_.find(name);
            if (it != group_name_to_index_map_.end())
            {
                return it->second;
            }
            return std::numeric_limits<size_t>::max();
        }

        /// @brief get group data
        /// @param name group name
        /// @return group data
        const VraGroupedDataHandle* GetGroupData(const std::string& name) const
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
        
        /// @brief get all group names
        /// @return all group names
        std::vector<std::string> GetAllGroupNames() const 
        { 
            std::vector<std::string> names;
            for (const auto& group : registered_groups_) {
                names.push_back(group.name);
            }
            return names;
        }

        size_t GetResourceOffset(std::string group_name, ResourceId id) const
        {
            return GetGroupData(group_name)->offsets.at(id);
        }
        
        /// @brief get suggest memory flags for vulkan
        /// @param id resource id
        /// @return suggest memory flags
        VkMemoryPropertyFlags GetSuggestMemoryFlags(std::string group_name);

        /// @brief get suggest vma memory flags for vulkan
        /// @param id resource id
        /// @return suggest vma memory flags
        VmaAllocationCreateFlags GetSuggestVmaMemoryFlags(std::string group_name);

    private :
        // --- Vulkan Native Objects Cache ---
        
        VkPhysicalDevice physical_device_handle_;
        VkPhysicalDeviceProperties physical_device_properties_;

        // --- Buffer specific storage ---
        
        std::unordered_map<ResourceId, VraDataDesc> buffer_desc_map_;   // TODO: Optimize to use vector
        std::unordered_map<ResourceId, VraRawData> buffer_data_map_;    // TODO: Optimize to use vector

        // --- Limits ---
        
        static constexpr size_t MAX_BUFFER_COUNT = 4096;

        // --- Registered Group Strategies ---

        std::vector<VraDataGroup> registered_groups_;
        std::map<std::string, size_t> group_name_to_index_map_;

        /// @brief clear grouped buffer data
        void ClearGroupedBufferData();

        /// @brief register default grouping strategies
        void RegisterDefaultGroups();
    };

    class ManagedVmaBuffer
    {
    private:
        // vulkan native
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VkBufferUsageFlags usage_ = 0;

        // vma allocations
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE; // neccessary for destruction
        VmaAllocationInfo allocationInfo_{};      // store allocation info (size, mapped pointer, etc.)
    
    public:
        // constructor
        ManagedVmaBuffer() = default;
        ManagedVmaBuffer(VkBuffer buf, VkBufferUsageFlags usage, VmaAllocation alloc, VmaAllocator allocatr, const VmaAllocationInfo &info)
            : buffer_(buf), usage_(usage), allocation_(alloc), allocator_(allocatr), allocationInfo_(info)
        {
            assert(allocator_ != VK_NULL_HANDLE && "Allocator must be valid for destruction!");
        }
        // default destructor - cleanup work is done by shared_ptr's deleter
        ~ManagedVmaBuffer() = default;

        // --- Copy and Move ---
        ManagedVmaBuffer(const ManagedVmaBuffer &) = delete;
        ManagedVmaBuffer &operator=(const ManagedVmaBuffer &) = delete;
        ManagedVmaBuffer(ManagedVmaBuffer &&other) noexcept
            : buffer_(std::exchange(other.buffer_, VK_NULL_HANDLE)),
              usage_(std::exchange(other.usage_, 0)),
              allocation_(std::exchange(other.allocation_, VK_NULL_HANDLE)),
              allocator_(std::exchange(other.allocator_, VK_NULL_HANDLE)),
              allocationInfo_(other.allocationInfo_)
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
    private:
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

    public:
        // constructor
        ManagedVmaImage() = default;
        ManagedVmaImage(VkImage img, VkImageView view, VkFormat fmt, VkExtent3D ext, VmaAllocation alloc, VmaAllocator allocatr, VkDevice dev, const VmaAllocationInfo &info)
            : image_(img), imageView_(view), format_(fmt), extent_(ext), allocation_(alloc), allocator_(allocatr), device_(dev), allocationInfo_(info)
        {
            assert(allocator_ != VK_NULL_HANDLE && "Allocator must be valid for destruction!");
            assert(device_ != VK_NULL_HANDLE && "Device must be valid for ImageView destruction!");
        }

        // default destructor - cleanup work is done by shared_ptr's deleter
        ~ManagedVmaImage() = default;

        // --- Copy and Move ---
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

    class VraDescriptorAllocator
    {
        VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;

        // constructor
        VraDescriptorAllocator(VkDevice device)
            : device_(device)
        {
        }

        // --- Helper methods ---

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