#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;

	class PipelineLayout
	{
	public:
		PipelineLayout();

		const vk::PipelineLayout& Get() const;
		const vk::Pipeline& GetGraphicsPipeline() const;

		bool Initialise(const Device& device, const std::string& name,
			const std::unordered_map<vk::ShaderStageFlagBits, std::vector<uint8_t>>& programs,
			const std::vector<vk::VertexInputBindingDescription>& bindingDescriptions,
			const std::vector<vk::VertexInputAttributeDescription>& attributeDescriptions,
			const std::vector<vk::Format>& attachmentFormats, vk::Format depthFormat,
			const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
			const std::vector<vk::PushConstantRange>& pushConstantRanges = {});

		bool Rebuild(const Device& device, const std::vector<vk::Format>& attachmentFormats, vk::Format depthFormat,
			const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts, const std::vector<vk::PushConstantRange>& pushConstantRanges = {});

	private:
		std::string m_name;
		vk::UniquePipelineLayout m_pipelineLayout;
		vk::UniquePipeline m_graphicsPipeline;
		std::vector<std::pair<vk::ShaderStageFlagBits, vk::UniqueShaderModule>> m_shaderModules;
		std::vector<vk::VertexInputBindingDescription> m_bindingDescriptions;
		std::vector<vk::VertexInputAttributeDescription> m_attributeDescriptions;
	};
}