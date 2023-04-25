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

	private:
		bool SetupUniformBufferDescriptorSetLayout(const Device& device);

		vk::UniquePipelineLayout m_pipelineLayout;
		vk::UniquePipeline m_graphicsPipeline;
		vk::UniqueDescriptorSetLayout m_uboDescriptorSetLayout;
	};
}