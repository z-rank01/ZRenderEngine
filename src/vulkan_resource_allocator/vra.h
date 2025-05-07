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
        // Static Mode
        Static_Local,       // Device Local, no CPU access
        Static_Upload,      // CPU write once, GPU read multiple times

        // Dynamic Mode
        Dynamic_Sequential, // sequential access, e.g. UBO update
        Dynamic_Random,     // random access, automatically enable Host Cached

        // Special Usage
        Stream_Ring,        // ring buffer mode
        Readback            // GPU write, CPU read
    };

    // --- Data Update Rate ---
    enum class VraDataUpdateRate
    {
        PerFrame,     // update per frame
        Occasional,   // update occasionally
        RarelyOrNever // update rarely or never
    };

#pragma region Vra vulkan native objects description

    struct VraBufferDesc
    {
        VkBufferUsageFlags usage_flags_ = 0;
        VkSharingMode sharing_mode_ = VK_SHARING_MODE_EXCLUSIVE;
        uint32_t queue_family_index_count_ = 0;
        uint32_t *pQueueFamilyIndices_ = nullptr;
    };
    struct VraImageDesc
    {
        VkImageUsageFlags usage_flags_ = 0;
        VkSharingMode sharing_mode_ = VK_SHARING_MODE_EXCLUSIVE;
    };
    struct VraRawData
    {
        const void *pData_ = nullptr;
        size_t size_ = 0;
    };

#pragma endregion

    class VraDataDesc
    {
    public:
        VraDataDesc() = default;
        VraDataDesc(VraDataMemoryPattern pattern,
                      VraDataUpdateRate behavior,
                      VraBufferDesc buffer_desc = VraBufferDesc{},
                      VraImageDesc image_desc = VraImageDesc{})
            : data_pattern_(pattern), data_update_rate_(behavior), buffer_desc_(buffer_desc), image_desc_(image_desc)
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
        VraDataUpdateRate GetDataUpdateRate() const { return data_update_rate_; }
        const VraBufferDesc& GetBufferDesc() const { return buffer_desc_; }
        const VraImageDesc& GetImageDesc() const { return image_desc_; }

        // --- presets ---
        static VraDataDesc StaticVertex()
        {
            VraDataDesc desc;
            desc.data_pattern_ = VraDataMemoryPattern::Static_Upload;
            desc.data_update_rate_ = VraDataUpdateRate::RarelyOrNever;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            return desc;
        }
        static VraDataDesc StaticIndex()
        {
            VraDataDesc desc;
            desc.data_pattern_ = VraDataMemoryPattern::Static_Upload;
            desc.data_update_rate_ = VraDataUpdateRate::RarelyOrNever;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            return desc;
        }
        static VraDataDesc DynamicUniform()
        {
            VraDataDesc desc;
            desc.data_pattern_ = VraDataMemoryPattern::Dynamic_Sequential;
            desc.data_update_rate_ = VraDataUpdateRate::PerFrame;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            return desc;
        }
        static VraDataDesc IndirectDrawCommands()
        {
            VraDataDesc desc;
            desc.data_pattern_ = VraDataMemoryPattern::Stream_Ring;
            desc.data_update_rate_ = VraDataUpdateRate::PerFrame;
            desc.buffer_desc_.usage_flags_ = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
            return desc;
        }

    private:
        // --- main members ---
        VraDataMemoryPattern data_pattern_;
        VraDataUpdateRate data_update_rate_;
        VraBufferDesc buffer_desc_;
        VraImageDesc image_desc_;
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

#pragma region Group Data Defnition
    // --- Grouped Buffer Data Structure (public for VraBufferGroup) ---
    struct GroupedBufferData
    {
        std::vector<uint8_t> consolidated_data;
        std::unordered_map<ResourceId, size_t> offsets;
        size_t total_size = 0;
        VkBufferUsageFlags combined_usage_flags = 0;

        void Clear()
        {
            consolidated_data.clear();
            offsets.clear();
            total_size = 0;
            combined_usage_flags = 0;
        }
    };

    // --- Buffer Grouping Strategy Definition ---
    struct VraBufferGroup {
        std::string name;
        std::function<bool(const VraDataDesc&)> predicate;
        std::function<void(GroupedBufferData& group_data, 
                           ResourceId id, 
                           const VraDataDesc& desc, 
                           const VraRawData& raw_data, 
                           const VkPhysicalDeviceProperties& props)> group_action;
        GroupedBufferData grouped_data_handle; // Each strategy instance owns its data
    };
#pragma endregion

    class VraDataCollector
    {
    public:
        VraDataCollector();
        ~VraDataCollector();

        // --- Data Collection ---

        /// @brief Collects buffer data description and raw data pointer.
        /// @param desc Description of the buffer data (usage, memory pattern, etc.).
        /// @param data Raw data pointer and size. The pointer pData_ must remain valid until GroupAllBufferData is called.
        /// @return True if collected successfully, false if ID already exists or max count reached.
        bool CollectBufferData(VraDataDesc desc, VraRawData data, ResourceId& id);

        
        ///@brief Collects image data description and raw data pointer. (Not fully implemented in grouping)
        ///@param id A unique ResourceId for this image data.
        ///@param desc Description of the image data.
        ///@param data Raw data pointer and size.
        ///@return True if collected successfully, false if ID already exists or max count reached.
        bool CollectImageData(VraDataDesc desc, VraRawData data, ResourceId& id);

        // --- Data Processing ---

        /// @brief processes all collected buffer data, grouping them by memory pattern
        /// @param physical_device_properties physical device properties
        void GroupAllBufferData(const VkPhysicalDeviceProperties& physical_device_properties);

        // --- Data Access ---

        std::optional<std::reference_wrapper<const VraDataDesc>> GetBufferDesc(ResourceId id) const;
        std::optional<std::reference_wrapper<const VraRawData>> GetBufferData(ResourceId id) const;
        std::optional<std::reference_wrapper<const VraDataDesc>> GetImageDesc(ResourceId id) const;
        std::optional<std::reference_wrapper<const VraRawData>> GetImageData(ResourceId id) const;

        /// @brief get the offset of a specific buffer resource within its consolidated group buffer
        /// @param id the ResourceId of the buffer
        /// @return the offset in bytes, or std::numeric_limits<size_t>::max() if not found (e.g., ID invalid or not grouped)
        size_t GetBufferOffset(ResourceId id) const;
        
        /// @brief clear all collected data
        void ClearAllCollectedData();

        /// @brief register buffer group strategy
        /// @param name register name
        /// @param predicate predicate for grouping
        /// @param group_action action for grouping
        void RegisterBufferGroup(
            std::string name,
            std::function<bool(const VraDataDesc&)> predicate,
            std::function<void(GroupedBufferData& group_data, ResourceId id, const VraDataDesc& desc, const VraRawData& raw_data, const VkPhysicalDeviceProperties& props)> group_action
        );

        /// @brief get grouped buffer data by name (default included: static_local_group, static_upload_group, dynamic_sequential_group)
        /// @param name register name
        /// @return pointer to grouped buffer data
        const GroupedBufferData* GetGroupData(const std::string& name) const;

        /// @brief get all group names
        /// @return vector of group names
        std::vector<std::string> GetAllGroupNames() const 
        { 
            std::vector<std::string> names;
            for (const auto& group : registered_groups_) {
                names.push_back(group.name);
            }
            return names;
        }

        /// @brief get the index of a group by name
        /// @param name group name
        /// @return index of the group
        size_t GetGroupIndex(const std::string& name) const
        {
            auto it = group_name_to_index_map_.find(name);
            if (it != group_name_to_index_map_.end()) {
                return it->second;
            }
            return std::numeric_limits<size_t>::max();
        }

    private:
        // --- Buffer specific storage ---
        std::unordered_map<ResourceId, VraDataDesc> buffer_desc_map_;
        std::unordered_map<ResourceId, VraRawData> buffer_data_map_;

        // --- Image specific storage ---
        std::unordered_map<ResourceId, VraDataDesc> image_desc_map_;
        std::unordered_map<ResourceId, VraRawData> image_data_map_;

        // --- Limits ---
        static constexpr size_t MAX_BUFFER_COUNT = 4096;
        static constexpr size_t MAX_IMAGE_COUNT = 1024;

        // --- Registered Group Strategies ---
        std::vector<VraBufferGroup> registered_groups_;
        std::map<std::string, size_t> group_name_to_index_map_;

        // --- Helper to clear groups before regrouping ---
        void ClearGroupedBufferData();
        // --- Helper to register default grouping strategies ---
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

    class VraDispatcher
    {
    public:
        /// @brief generate all buffers from data collector
        /// @param data_collector data collector
        /// @param vma_allocator vma allocator
        /// @return true if success, false if failed
        bool GenerateAllBuffers(const VraDataCollector &data_collector, const VmaAllocator &vma_allocator);

        /// @brief generate all images from data collector
        /// @param data_collector data collector
        /// @param vma_allocator vma allocator
        /// @return true if success, false if failed
        bool GenerateAllImages(const VraDataCollector &data_collector, const VmaAllocator &vma_allocator);

    private:
        std::unordered_map<std::string, std::vector<ManagedVmaBuffer>> buffer_map_;
        std::unordered_map<std::string, std::vector<ManagedVmaImage>> image_map_;

        VmaAllocationCreateFlags GetAllocationCreateFlags(VraDataMemoryPattern pattern);
    };

    class VRA
    {
    public:
        VRA();
        ~VRA();
    };
}