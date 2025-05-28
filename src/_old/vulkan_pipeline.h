#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vector>
#include <string>
#include <map>

#include "vulkan_shader.h"
#include "vulkan_window.h"
#include "utility/logger.h"

struct SVulkanPipelineConfig
{
    VkExtent2D swap_chain_extent;
    std::map<EShaderType, VkShaderModule> shader_module_map;
    VkRenderPass renderpass;
    VkVertexInputBindingDescription vertex_input_binding_description;
    std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
};

class VulkanPipelineHelper
{
private:
    SVulkanPipelineConfig config_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;
    VkDevice device_;
public:
    VulkanPipelineHelper(SVulkanPipelineConfig config) : config_(config) {}
    ~VulkanPipelineHelper();

    bool CreatePipeline(VkDevice device);
    VkPipeline GetPipeline() const { return pipeline_; }
    VkPipelineLayout GetPipelineLayout() const { return pipeline_layout_; }
};