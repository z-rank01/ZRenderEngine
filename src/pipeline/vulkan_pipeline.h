#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <map>

#include "vulkan_shader.h"
#include "initialization/vulkan_window.h"
#include "utility/logger.h"

struct SVulkanPipelineConfig
{
    const SVulkanSwapChainConfig* swap_chain_config;
    std::map<EShaderType, VkShaderModule> shader_module_map;
    VkRenderPass renderpass;
    VkVertexInputBindingDescription vertex_input_binding_description;
    std::vector<VkVertexInputAttributeDescription> vertex_input_attribute_descriptions;
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