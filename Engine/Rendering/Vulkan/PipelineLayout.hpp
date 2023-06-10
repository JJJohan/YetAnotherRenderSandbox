#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include "Core/Logging/Logger.hpp"
#include "../Material.hpp"

struct SpvReflectInterfaceVariable;
struct SpvReflectShaderModule;

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class DescriptorPool;
	class ImageSampler;
	class ImageView;
	class Buffer;

	class PipelineLayout
	{
	public:
		PipelineLayout(const Material& material);

		const vk::PipelineLayout& Get() const;
		const vk::Pipeline& GetGraphicsPipeline() const;

		bool Initialise(const Device& device, uint32_t concurrentFrames);
		bool Update(const PhysicalDevice& physicalDevice, const Device& device, vk::Format swapchainFormat, vk::Format depthFormat);

		inline bool BindImageView(uint32_t binding, const std::unique_ptr<ImageView>& imageView)
		{
			return BindImageViewsImp(binding, { imageView.get() });
		}

		inline bool BindSampler(uint32_t binding, const std::unique_ptr<ImageSampler>& sampler)
		{
			return BindSamplersImp(binding, { sampler.get() });
		}

		inline bool BindStorageBuffer(uint32_t binding, const std::unique_ptr<Buffer>& storageBuffer)
		{
			return BindStorageBuffersImp(binding, { storageBuffer.get() });
		}

		inline bool BindImageView(uint32_t binding, const ImageView& imageView)
		{
			return BindImageViewsImp(binding, { &imageView });
		}

		inline bool BindSampler(uint32_t binding, const ImageSampler& sampler)
		{
			return BindSamplersImp(binding, { &sampler });
		}

		inline bool BindStorageBuffer(uint32_t binding, const Buffer& storageBuffer)
		{
			return BindStorageBuffersImp(binding, { &storageBuffer });
		}

		inline bool BindImageViews(uint32_t binding, const std::vector<std::unique_ptr<ImageView>>& imageViews)
		{
			std::vector<const ImageView*> imageViewPtrs(imageViews.size());
			for (size_t i = 0; i < imageViews.size(); ++i)
				imageViewPtrs[i] = imageViews[i].get();
			return BindImageViewsImp(binding, imageViewPtrs);
		}

		inline bool BindSamplers(uint32_t binding, const std::vector<std::unique_ptr<ImageSampler>>& samplers)
		{
			std::vector<const ImageSampler*> samplerPtrs(samplers.size());
			for (size_t i = 0; i < samplers.size(); ++i)
				samplerPtrs[i] = samplers[i].get();
			return BindSamplersImp(binding, samplerPtrs);
		}

		inline bool BindStorageBuffers(uint32_t binding, const std::vector<std::unique_ptr<Buffer>>& storageBuffers)
		{
			std::vector<const Buffer*> storageBufferPtrs(storageBuffers.size());
			for (size_t i = 0; i < storageBuffers.size(); ++i)
				storageBufferPtrs[i] = storageBuffers[i].get();
			return BindStorageBuffersImp(binding, storageBufferPtrs);
		}

		inline bool BindUniformBuffers(uint32_t binding, const std::vector<std::unique_ptr<Buffer>>& uniformBuffers)
		{
			std::vector<const Buffer*> uniformBufferPtrs(uniformBuffers.size());
			for (size_t i = 0; i < uniformBuffers.size(); ++i)
				uniformBufferPtrs[i] = uniformBuffers[i].get();
			return BindUniformBuffersImp(binding, uniformBufferPtrs);
		}

		void BindPipeline(const vk::CommandBuffer& commandBuffer, uint32_t frameIndex) const;

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

		bool ValidateInputsOutputs(std::unordered_map<vk::ShaderStageFlagBits, ReflectionData>& reflectionData);

		bool ValidateBufferBlockBinding(uint32_t binding, const std::vector<const Buffer*>& buffers,
			const DescriptorBindingInfo& bindingInfo) const;

		bool PerformReflection(vk::ShaderStageFlagBits stage, const std::vector<uint8_t>& data,
			SpvReflectShaderModule& module, ReflectionData& reflectionData);

		bool Rebuild(const PhysicalDevice& physicalDevice, const Device& device, vk::Format swapchainFormat, vk::Format depthFormat);

		bool CreateDescriptorSetLayout(const Device& device);

		bool BindImageViewsImp(uint32_t binding, const std::vector<const ImageView*>& imageViews);
		bool BindSamplersImp(uint32_t binding, const std::vector<const ImageSampler*>& samplers);
		bool BindStorageBuffersImp(uint32_t binding, const std::vector<const Buffer*>& storageBuffers);
		bool BindUniformBuffersImp(uint32_t binding, const std::vector<const Buffer*>& uniformBuffers);

		template <typename T>
		inline DescriptorBindingInfo* GetBindingInfo(uint32_t binding, const std::vector<T>& bindingData, vk::DescriptorType expectedType, const char* typeName)
		{
			auto& setInfo = m_descriptorSetInfos[0];
			const auto& bindingInfoSearch = setInfo.BindingInfos.find(binding);
			if (bindingInfoSearch == setInfo.BindingInfos.end())
			{
				Engine::Logging::Logger::Error("Binding at index {} for material '{}' does not exist.", binding, m_material.GetName());
				return nullptr;
			}

			DescriptorBindingInfo* bindingInfo = &bindingInfoSearch->second;
			if (bindingInfo->Binding.descriptorType != expectedType)
			{
				Engine::Logging::Logger::Error("Binding at index {} for material '{}' is not a {} type.", binding, m_material.GetName(), typeName);
				return nullptr;
			}

			if (bindingData.empty())
			{
				Engine::Logging::Logger::Error("Attempted to bind empty contents at index {} for material '{}'.", binding, m_material.GetName());
				return nullptr;
			}

			return bindingInfo;
		}

		uint32_t m_concurrentFrames;
		const Material& m_material;
		vk::UniquePipelineLayout m_pipelineLayout;
		vk::UniquePipeline m_graphicsPipeline;
		std::vector<std::pair<vk::ShaderStageFlagBits, vk::UniqueShaderModule>> m_shaderModules;
		std::vector<vk::Format> m_attachmentFormats;
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
	};
}