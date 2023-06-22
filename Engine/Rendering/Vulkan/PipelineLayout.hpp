#pragma once

#include <vulkan/vulkan.hpp>
#include "../Resources/Material.hpp"
#include "Core/Logging/Logger.hpp"

struct SpvReflectInterfaceVariable;
struct SpvReflectShaderModule;

namespace Engine::Rendering
{
	class IImageView;
	class IImageSampler;
	class IBuffer;
	class ICommandBuffer;
	class IDevice;
	class IPhysicalDevice;
}

namespace Engine::Rendering::Vulkan
{
	class DescriptorPool;

	class PipelineLayout : public Material
	{
	public:
		PipelineLayout();

		const vk::PipelineLayout& Get() const;
		const vk::Pipeline& GetGraphicsPipeline() const;

		bool Initialise(const IDevice& device, uint32_t concurrentFrames);
		bool Update(const IPhysicalDevice& physicalDevice, const IDevice& device, const vk::PipelineCache& pipelineCache,
			Format swapchainFormat, Format depthFormat);

		virtual bool SetSpecialisationConstant(std::string name, int32_t value) override;

		virtual void BindMaterial(const ICommandBuffer& commandBuffer, uint32_t frameIndex) const override;

		inline bool IsDirty() const { return m_specConstantsDirty || !m_writeDescriptorSets.empty(); };

	private:
		struct ReflectionData
		{
			std::vector<SpvReflectInterfaceVariable*> InputVariables;
			std::vector<SpvReflectInterfaceVariable*> OutputVariables;
			const std::vector<uint8_t>& Data;
		};

		struct DescriptorBindingInfo
		{
			uint32_t BlockSize;
			std::vector<uint32_t> BlockMemberOffsets;
			vk::DescriptorSetLayoutBinding Binding;
		};

		struct DescriptorSetInfo
		{
			std::unordered_map<uint32_t, DescriptorBindingInfo> BindingInfos;
		};

		struct SpecialisationConstantInfo
		{
			vk::ShaderStageFlags stageFlags;
			vk::SpecializationMapEntry entry;
			int32_t value;
		};

		bool ValidateInputsOutputs(std::unordered_map<vk::ShaderStageFlagBits, ReflectionData>& reflectionData);

		bool ValidateBufferBlockBinding(uint32_t binding, const std::vector<const IBuffer*>& buffers,
			const DescriptorBindingInfo& bindingInfo) const;

		bool PerformReflection(vk::ShaderStageFlagBits stage, const std::vector<uint8_t>& data,
			SpvReflectShaderModule& module, ReflectionData& reflectionData);

		bool Rebuild(const IPhysicalDevice& physicalDevice, const IDevice& device, const vk::PipelineCache& pipelineCache,
			Format swapchainFormat, Format depthFormat);

		bool CreateDescriptorSetLayout(const IDevice& device);

		virtual bool BindImageViewsImp(uint32_t binding, const std::vector<const IImageView*>& imageViews) override;
		virtual bool BindSamplersImp(uint32_t binding, const std::vector<const IImageSampler*>& samplers) override;
		virtual bool BindStorageBuffersImp(uint32_t binding, const std::vector<const IBuffer*>& storageBuffers) override;
		virtual bool BindUniformBuffersImp(uint32_t binding, const std::vector<const IBuffer*>& uniformBuffers) override;

		template <typename T>
		inline DescriptorBindingInfo* GetBindingInfo(uint32_t binding, const std::vector<T>& bindingData,
			vk::DescriptorType expectedType, const char* typeName)
		{
			auto& setInfo = m_descriptorSetInfos[0];
			const auto& bindingInfoSearch = setInfo.BindingInfos.find(binding);
			if (bindingInfoSearch == setInfo.BindingInfos.end())
			{
				Engine::Logging::Logger::Error("Binding at index {} for material '{}' does not exist.", binding, GetName());
				return nullptr;
			}

			DescriptorBindingInfo* bindingInfo = &bindingInfoSearch->second;
			if (bindingInfo->Binding.descriptorType != expectedType)
			{
				Engine::Logging::Logger::Error("Binding at index {} for material '{}' is not a {} type.", binding, GetName(), typeName);
				return nullptr;
			}

			if (bindingData.empty())
			{
				Engine::Logging::Logger::Error("Attempted to bind empty contents at index {} for material '{}'.", binding, GetName());
				return nullptr;
			}

			return bindingInfo;
		}

		uint32_t m_concurrentFrames;
		bool m_specConstantsDirty;
		vk::UniquePipelineLayout m_pipelineLayout;
		vk::UniquePipeline m_graphicsPipeline;
		std::vector<std::pair<vk::ShaderStageFlagBits, vk::UniqueShaderModule>> m_shaderModules;
		std::vector<Format> m_attachmentFormats;
		std::vector<uint32_t> m_swapchainFormatIndices;
		std::vector<vk::VertexInputBindingDescription> m_bindingDescriptions;
		std::vector<vk::VertexInputAttributeDescription> m_attributeDescriptions;
		std::vector<vk::PushConstantRange> m_pushConstantRanges;
		vk::UniqueDescriptorSetLayout m_descriptorSetLayout;
		std::vector<vk::DescriptorSet> m_descriptorSets;
		std::vector<std::vector<vk::WriteDescriptorSet>> m_writeDescriptorSets;
		std::unique_ptr<DescriptorPool> m_descriptorPool;
		std::unordered_map<uint32_t, DescriptorSetInfo> m_descriptorSetInfos;
		std::vector<std::vector<vk::DescriptorBufferInfo>> m_descriptorBufferInfos;
		std::vector<std::vector<vk::DescriptorImageInfo>> m_descriptorImageInfos;
		std::unordered_map<std::string, SpecialisationConstantInfo> m_specialisationConstants;
	};
}