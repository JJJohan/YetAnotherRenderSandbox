#pragma once

#include "../Shader.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

namespace Engine::Rendering::Vulkan
{
	class Device;
	class RenderPass;

	class PipelineLayout : public Shader
	{
	public:
		PipelineLayout();

		virtual bool IsValid() const;
		const vk::PipelineLayout& Get() const;
		const vk::Pipeline& GetGraphicsPipeline() const;
		std::vector<vk::DescriptorSetLayout> GetDescriptorSetLayouts() const;

		bool Initialise(const Device& device, const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs, 
			const RenderPass& renderPass);

		bool Rebuild(const Device& device, const RenderPass& renderPass, uint32_t imageCount);

	private:
		bool SetupDescriptorSetLayout(const Device& device);

		uint32_t m_imageCount;
		vk::UniquePipelineLayout m_pipelineLayout;
		vk::UniquePipeline m_graphicsPipeline;
		vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
		std::vector<std::pair<vk::ShaderStageFlagBits, vk::UniqueShaderModule>> m_shaderModules;
	};
}