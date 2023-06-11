#include "PipelineLayout.hpp"
#include "Device.hpp"
#include "PhysicalDevice.hpp"
#include "DescriptorPool.hpp"
#include "ImageSampler.hpp"
#include "ImageView.hpp"
#include "Buffer.hpp"
#include "OS/Files.hpp"
#include "VulkanFormatInterop.hpp"
#include <filesystem>
#include <glm/glm.hpp>
#include <spirv_reflect.h>
#include <algorithm>

using namespace Engine::Logging;
using namespace Engine::OS;
using namespace Engine::Rendering;

namespace Engine::Rendering::Vulkan
{
	PipelineLayout::PipelineLayout(const Material& material)
		: m_material(material)
		, m_attachmentFormats()
		, m_swapchainFormatIndices()
		, m_pipelineLayout(nullptr)
		, m_graphicsPipeline(nullptr)
		, m_shaderModules()
		, m_bindingDescriptions()
		, m_attributeDescriptions()
		, m_pushConstantRanges()
		, m_descriptorSetLayout()
		, m_descriptorSets()
		, m_descriptorSetInfos()
		, m_descriptorPool()
		, m_writeDescriptorSets()
		, m_descriptorBufferInfos()
		, m_descriptorImageInfos()
		, m_concurrentFrames(0)
		, m_specialisationConstants()
		, m_specConstantsDirty(false)
	{
	}

	const vk::PipelineLayout& PipelineLayout::Get() const
	{
		return m_pipelineLayout.get();
	}

	const vk::Pipeline& PipelineLayout::GetGraphicsPipeline() const
	{
		return m_graphicsPipeline.get();
	}

	bool PipelineLayout::Rebuild(const PhysicalDevice& physicalDevice, const Device& device, const vk::PipelineCache& pipelineCache,
		vk::Format swapchainFormat, vk::Format depthFormat)
	{
		m_graphicsPipeline.reset();
		m_pipelineLayout.reset();

		const vk::Device& deviceImp = device.Get();

		m_specConstantsDirty = false;
		std::vector<int32_t> specialisationData;
		vk::SpecializationInfo specialisationInfo{};
		std::vector<vk::SpecializationMapEntry> specialisationMapEntries;
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStageInfos;
		shaderStageInfos.reserve(m_shaderModules.size());
		for (const auto& module : m_shaderModules)
		{
			vk::PipelineShaderStageCreateInfo& shaderStageInfo = shaderStageInfos.emplace_back(vk::PipelineShaderStageCreateInfo());
			shaderStageInfo.stage = module.first;
			shaderStageInfo.module = module.second.get();
			shaderStageInfo.pName = "main";
			shaderStageInfo.pSpecializationInfo = &specialisationInfo;

			for (const auto& entry : m_specialisationConstants)
			{
				if ((entry.second.stageFlags & module.first) == module.first)
				{
					specialisationMapEntries.emplace_back(entry.second.entry);
					specialisationData.emplace_back(entry.second.value);
				}
			}

			specialisationInfo.mapEntryCount = static_cast<uint32_t>(specialisationMapEntries.size());
			specialisationInfo.pMapEntries = specialisationMapEntries.data();
			specialisationInfo.dataSize = static_cast<uint32_t>(specialisationData.size() * sizeof(uint32_t));
			specialisationInfo.pData = specialisationData.data();
		}

		std::vector<vk::DynamicState> dynamicStates =
		{
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};

		vk::PipelineDynamicStateCreateInfo dynamicState(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

		// Vertex input state
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo(vk::PipelineVertexInputStateCreateFlags(), m_bindingDescriptions, m_attributeDescriptions);

		// Input assembly state
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
		inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// Viewport state
		vk::PipelineViewportStateCreateInfo viewportState;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		// Rasterizer state
		vk::PipelineRasterizationStateCreateInfo rasterizer;
		rasterizer.polygonMode = vk::PolygonMode::eFill;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = vk::CullModeFlagBits::eBack;
		rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
		rasterizer.depthClampEnable = physicalDevice.GetFeatures().depthBiasClamp;

		// Multisampling state
		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		// Depth and stencil state
		vk::PipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.depthCompareOp = vk::CompareOp::eLess;
		depthStencil.depthWriteEnable = m_material.DepthWrite();
		depthStencil.depthTestEnable = m_material.DepthTest();
		depthStencil.stencilTestEnable = VK_FALSE;

		// Color blend attachment state
		vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne; // Optional
		colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero; // Optional
		colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
		colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd; // Optional

		std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(m_attachmentFormats.size());
		for (size_t i = 0; i < m_attachmentFormats.size(); ++i)
			blendAttachments[i] = colorBlendAttachment;

		// Color blend state
		vk::PipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
		colorBlending.attachmentCount = static_cast<uint32_t>(m_attachmentFormats.size());
		colorBlending.pAttachments = blendAttachments.data();

		// Pipeline layout
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout.get();
		pipelineLayoutInfo.pPushConstantRanges = m_pushConstantRanges.data();
		pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size());

		m_pipelineLayout = deviceImp.createPipelineLayoutUnique(pipelineLayoutInfo);
		if (!m_pipelineLayout.get())
		{
			Logger::Error("Failed to create pipeline layout.");
			return false;
		}

		// Assign swapchain format now.
		for (uint32_t index : m_swapchainFormatIndices)
			m_attachmentFormats[index] = swapchainFormat;

		vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
		pipelineRenderingCreateInfo.colorAttachmentCount = static_cast<uint32_t>(m_attachmentFormats.size());
		pipelineRenderingCreateInfo.pColorAttachmentFormats = m_attachmentFormats.data();
		pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

		vk::GraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfos.size());
		pipelineInfo.pStages = shaderStageInfos.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_pipelineLayout.get();
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional
		pipelineInfo.pNext = &pipelineRenderingCreateInfo;

		vk::ResultValue<vk::UniquePipeline> result = deviceImp.createGraphicsPipelineUnique(pipelineCache, pipelineInfo);
		if (result.result != vk::Result::eSuccess)
		{
			Logger::Error("Failed to create graphics pipeline.");
			return false;
		}

		m_graphicsPipeline = std::move(result.value);
		return true;
	}

	bool PipelineLayout::BindImageViewsImp(uint32_t binding, const std::vector<const ImageView*>& imageViews)
	{
		DescriptorBindingInfo* bindingInfo = GetBindingInfo(binding, imageViews, vk::DescriptorType::eSampledImage, "sampled image");
		if (!bindingInfo)
			return false;

		std::vector<vk::DescriptorImageInfo>& imageInfos = m_descriptorImageInfos.emplace_back(std::vector<vk::DescriptorImageInfo>(imageViews.size()));
		for (size_t i = 0; i < imageViews.size(); ++i)
			imageInfos[i] = vk::DescriptorImageInfo(nullptr, imageViews[i]->Get(), vk::ImageLayout::eShaderReadOnlyOptimal);

		if (m_writeDescriptorSets.empty())
			m_writeDescriptorSets.resize(m_concurrentFrames);

		bindingInfo->Binding.descriptorCount = static_cast<uint32_t>(imageViews.size());
		for (uint32_t i = 0; i < m_concurrentFrames; ++i)
			m_writeDescriptorSets[i].emplace_back(vk::WriteDescriptorSet(nullptr, binding, 0, vk::DescriptorType::eSampledImage, imageInfos));
		return true;
	}

	bool PipelineLayout::BindSamplersImp(uint32_t binding, const std::vector<const ImageSampler*>& samplers)
	{
		DescriptorBindingInfo* bindingInfo = GetBindingInfo(binding, samplers, vk::DescriptorType::eSampler, "sampler");
		if (!bindingInfo)
			return false;

		std::vector<vk::DescriptorImageInfo>& samplerInfos = m_descriptorImageInfos.emplace_back(std::vector<vk::DescriptorImageInfo>(samplers.size()));
		for (size_t i = 0; i < samplers.size(); ++i)
			samplerInfos[i] = vk::DescriptorImageInfo(samplers[i]->Get(), nullptr, vk::ImageLayout::eShaderReadOnlyOptimal);

		if (m_writeDescriptorSets.empty())
			m_writeDescriptorSets.resize(m_concurrentFrames);

		bindingInfo->Binding.descriptorCount = static_cast<uint32_t>(samplers.size());
		for (uint32_t i = 0; i < m_concurrentFrames; ++i)
			m_writeDescriptorSets[i].emplace_back(vk::WriteDescriptorSet(nullptr, binding, 0, vk::DescriptorType::eSampler, samplerInfos));

		return true;
	}

	bool PipelineLayout::ValidateBufferBlockBinding(uint32_t binding, const std::vector<const Buffer*>& buffers, const DescriptorBindingInfo& bindingInfo) const
	{
		size_t size = buffers.front()->Size();
		for (size_t i = 1; i < buffers.size(); ++i)
		{
			if (buffers[i]->Size() != size)
			{
				Logger::Error("Binding at index {} for material '{}' has buffers with inconsistent sizes ({} != {}).",
					binding, m_material.GetName(), bindingInfo.BlockSize, size);
				return false;
			}
		}

		return true;
	}

	bool PipelineLayout::BindStorageBuffersImp(uint32_t binding, const std::vector<const Buffer*>& storageBuffers)
	{
		DescriptorBindingInfo* bindingInfo = GetBindingInfo(binding, storageBuffers, vk::DescriptorType::eStorageBuffer, "storage buffer");
		if (!bindingInfo)
			return false;

		if (!ValidateBufferBlockBinding(binding, storageBuffers, *bindingInfo))
			return false;

		std::vector<vk::DescriptorBufferInfo>& bufferInfos = m_descriptorBufferInfos.emplace_back(std::vector<vk::DescriptorBufferInfo>(storageBuffers.size()));
		for (size_t i = 0; i < storageBuffers.size(); ++i)
			bufferInfos[i] = vk::DescriptorBufferInfo(storageBuffers[i]->Get(), 0, storageBuffers[i]->Size());

		if (m_writeDescriptorSets.empty())
			m_writeDescriptorSets.resize(m_concurrentFrames);

		bindingInfo->Binding.descriptorCount = static_cast<uint32_t>(storageBuffers.size());
		for (uint32_t i = 0; i < m_concurrentFrames; ++i)
			m_writeDescriptorSets[i].emplace_back(vk::WriteDescriptorSet(nullptr, binding, 0, vk::DescriptorType::eStorageBuffer, nullptr, bufferInfos));

		return true;
	}

	bool PipelineLayout::BindUniformBuffersImp(uint32_t binding, const std::vector<const Buffer*>& uniformBuffers)
	{
		DescriptorBindingInfo* bindingInfo = GetBindingInfo(binding, uniformBuffers, vk::DescriptorType::eUniformBuffer, "uniform buffer");
		if (!bindingInfo)
			return false;

		if (uniformBuffers.size() != m_concurrentFrames)
		{
			Logger::Error("Uniform buffer binding count should match concurrent frame count of {}.", m_concurrentFrames);
			return false;
		}

		if (!ValidateBufferBlockBinding(binding, uniformBuffers, *bindingInfo))
			return false;

		if (m_writeDescriptorSets.empty())
			m_writeDescriptorSets.resize(m_concurrentFrames);

		bindingInfo->Binding.descriptorCount = 1;
		for (uint32_t i = 0; i < m_concurrentFrames; ++i)
		{
			std::vector<vk::DescriptorBufferInfo>& bufferInfos = m_descriptorBufferInfos.emplace_back(std::vector<vk::DescriptorBufferInfo>());
			bufferInfos.emplace_back(vk::DescriptorBufferInfo(uniformBuffers[i]->Get(), 0, uniformBuffers[i]->Size()));
			m_writeDescriptorSets[i].emplace_back(vk::WriteDescriptorSet(nullptr, binding, 0, vk::DescriptorType::eUniformBuffer, nullptr, bufferInfos));
		}

		return true;
	}

	bool PipelineLayout::Update(const PhysicalDevice& physicalDevice, const Device& device, const vk::PipelineCache& pipelineCache,
		vk::Format swapchainFormat, vk::Format depthFormat)
	{
		bool rebuild = m_specConstantsDirty;
		if (!m_writeDescriptorSets.empty())
		{
			if (!CreateDescriptorSetLayout(device))
				return false;

			for (size_t i = 0; i < m_writeDescriptorSets.size(); ++i)
			{
				uint32_t descriptorCount = 0;
				std::vector<vk::WriteDescriptorSet> writeDescriptorSet = m_writeDescriptorSets[i];
				for (auto& writeDescriptor : writeDescriptorSet)
				{
					writeDescriptor.dstSet = m_descriptorSets[i];
					descriptorCount += writeDescriptor.descriptorCount;
				}
				device.Get().updateDescriptorSets(writeDescriptorSet, nullptr);
			}
			m_writeDescriptorSets.clear();
			m_descriptorBufferInfos.clear();
			m_descriptorImageInfos.clear();
			rebuild = true;
		}

		if (rebuild && !Rebuild(physicalDevice, device, pipelineCache, swapchainFormat, depthFormat))
			return false;

		return true;
	}

	bool PipelineLayout::SetSpecialisationConstant(std::string name, int32_t value)
	{
		const auto& it = m_specialisationConstants.find(name);
		if (it == m_specialisationConstants.end())
		{
			Logger::Error("Specialisation constant '{}' not found in pipeline layout '{}'.", name, m_material.GetName());
			return false;
		}

		if (it->second.value != value)
		{
			it->second.value = value;
			m_specConstantsDirty = true;
		}

		return true;
	}

	void PipelineLayout::BindPipeline(const vk::CommandBuffer& commandBuffer, uint32_t frameIndex) const
	{
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline.get());
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout.get(), 0, m_descriptorSets[frameIndex], nullptr);
	}

	inline bool IsMatchingStage(vk::ShaderStageFlagBits stage, SpvReflectShaderStageFlagBits spvStage)
	{
		if (spvStage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT && stage == vk::ShaderStageFlagBits::eVertex)
			return true;

		if (spvStage == SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT && stage == vk::ShaderStageFlagBits::eFragment)
			return true;

		return false;
	}

	bool PipelineLayout::CreateDescriptorSetLayout(const Device& device)
	{
		m_descriptorSetLayout.reset();
		m_descriptorSets.clear();
		m_descriptorPool.reset();

		if (m_descriptorSetInfos.empty())
			return true;

		for (const auto& setPair : m_descriptorSetInfos)
		{
			const DescriptorSetInfo& info = setPair.second;
			std::vector<vk::DescriptorSetLayoutBinding> bindings;
			std::vector<vk::DescriptorPoolSize> poolSizes;
			uint32_t descriptorCount = 0;
			for (const auto& bindingPair : info.BindingInfos)
			{
				const vk::DescriptorSetLayoutBinding& binding = bindingPair.second.Binding;
				bindings.emplace_back(binding);
				descriptorCount += binding.descriptorCount;
				if (binding.descriptorCount > 0)
					poolSizes.emplace_back(vk::DescriptorPoolSize(binding.descriptorType, binding.descriptorCount));
			}

			m_descriptorPool = std::make_unique<DescriptorPool>();
			if (!m_descriptorPool->Initialise(device, m_concurrentFrames, poolSizes))
				return false;

			vk::DescriptorSetLayoutCreateInfo createInfo(vk::DescriptorSetLayoutCreateFlags(), bindings);
			m_descriptorSetLayout = device.Get().createDescriptorSetLayoutUnique(createInfo);
			if (!m_descriptorSetLayout.get())
			{
				Logger::Error("Failed to create descriptor set layout.");
				return false;
			}

			std::vector<vk::DescriptorSetLayout> layouts;
			for (uint32_t i = 0; i < m_concurrentFrames; ++i)
			{
				layouts.push_back(m_descriptorSetLayout.get());
			}

			m_descriptorSets = m_descriptorPool->CreateDescriptorSets(device, layouts);

			break; // TODO: Handle multiple sets?
		}

		return true;
	}

	bool PipelineLayout::PerformReflection(vk::ShaderStageFlagBits stage, const std::vector<uint8_t>& data, SpvReflectShaderModule& module, ReflectionData& entry)
	{
		SpvReflectResult result = spvReflectCreateShaderModule(data.size(), data.data(), &module);
		if (result != SPV_REFLECT_RESULT_SUCCESS)
			return false;

		if (!IsMatchingStage(stage, module.shader_stage))
			return false;

		uint32_t inputVariableCount = 0;
		result = spvReflectEnumerateInputVariables(&module, &inputVariableCount, NULL);
		if (result != SPV_REFLECT_RESULT_SUCCESS)
		{
			return false;
		}

		entry.InputVariables.resize(inputVariableCount);
		result = spvReflectEnumerateInputVariables(&module, &inputVariableCount, entry.InputVariables.data());
		if (result != SPV_REFLECT_RESULT_SUCCESS)
		{
			return false;
		}

		for (int32_t i = inputVariableCount - 1; i >= 0; --i)
		{
			if (entry.InputVariables[i]->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
			{
				entry.InputVariables.erase(entry.InputVariables.begin() + i);
				--inputVariableCount;
			}
		}

		uint32_t outputVariableCount = 0;
		result = spvReflectEnumerateOutputVariables(&module, &outputVariableCount, NULL);
		if (result != SPV_REFLECT_RESULT_SUCCESS)
		{
			return false;
		}

		entry.OutputVariables.resize(outputVariableCount);
		result = spvReflectEnumerateOutputVariables(&module, &outputVariableCount, entry.OutputVariables.data());
		if (result != SPV_REFLECT_RESULT_SUCCESS)
		{
			return false;
		}

		for (int32_t i = outputVariableCount - 1; i >= 0; --i)
		{
			if (entry.OutputVariables[i]->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
			{
				entry.OutputVariables.erase(entry.OutputVariables.begin() + i);
				--outputVariableCount;
			}
		}

		if (stage == vk::ShaderStageFlagBits::eVertex)
		{
			for (uint32_t i = 0; i < inputVariableCount; ++i)
			{
				const SpvReflectInterfaceVariable& inputVariable = *(entry.InputVariables[i]);

				vk::Format format = static_cast<vk::Format>(inputVariable.format);
				uint32_t stride = GetSizeForFormat(format);
				m_bindingDescriptions.emplace_back(vk::VertexInputBindingDescription(inputVariable.location, stride));
				m_attributeDescriptions.emplace_back(vk::VertexInputAttributeDescription(inputVariable.location, inputVariable.location, format, 0));
			}
		}

		uint32_t descriptorSetCount = 0;
		result = spvReflectEnumerateDescriptorSets(&module, &descriptorSetCount, NULL);
		if (result != SPV_REFLECT_RESULT_SUCCESS)
		{
			return false;
		}

		if (descriptorSetCount > 0)
		{
			std::vector<SpvReflectDescriptorSet*> descriptorSets(descriptorSetCount);
			result = spvReflectEnumerateDescriptorSets(&module, &descriptorSetCount, descriptorSets.data());
			if (result != SPV_REFLECT_RESULT_SUCCESS)
			{
				return false;
			}

			for (uint32_t i = 0; i < descriptorSetCount; ++i)
			{
				const SpvReflectDescriptorSet& spvDescriptorSet = *(descriptorSets[i]);

				DescriptorSetInfo& descriptorSetInfo = m_descriptorSetInfos[spvDescriptorSet.set];

				for (uint32_t j = 0; j < spvDescriptorSet.binding_count; ++j)
				{
					const SpvReflectDescriptorBinding& spvBinding = *(spvDescriptorSet.bindings[j]);

					auto existingSearch = descriptorSetInfo.BindingInfos.find(spvBinding.binding);
					if (existingSearch != descriptorSetInfo.BindingInfos.end())
					{
						DescriptorBindingInfo& existingBinding = existingSearch->second;
						vk::DescriptorSetLayoutBinding& layoutBinding = existingBinding.Binding;
						layoutBinding.stageFlags |= stage;

						if (layoutBinding.descriptorType != static_cast<vk::DescriptorType>(spvBinding.descriptor_type))
						{
							Logger::Error("Descriptor type mismatch between stages for set {} binding {}.", spvDescriptorSet.set, spvBinding.binding);
							return false;
						}

						uint32_t descriptorCount = 1;
						for (uint32_t k = 0; k < spvBinding.array.dims_count; ++k)
							descriptorCount *= spvBinding.array.dims[k];

						if (layoutBinding.descriptorCount != descriptorCount)
						{
							Logger::Error("Descriptor count mismatch ({} != {}) between stages for set {} binding {}.",
								layoutBinding.descriptorCount, descriptorCount, spvDescriptorSet.set, spvBinding.binding);
							return false;
						}

						if (existingBinding.BlockSize != spvBinding.block.size)
						{
							Logger::Error("Descriptor block size mismatch ({} != {}) between stages for set {} binding {}.",
								existingBinding.BlockSize, spvBinding.block.size, spvDescriptorSet.set, spvBinding.binding);
							return false;
						}

						for (uint32_t k = 0; k < spvBinding.block.member_count; ++k)
						{
							if (existingBinding.BlockMemberOffsets[k] != spvBinding.block.members[k].offset)
							{
								Logger::Error("Descriptor block member offset mismatch ({} != {}) between stages for set {} binding {}.",
									existingBinding.BlockMemberOffsets[k], spvBinding.block.members[k].offset, spvDescriptorSet.set, spvBinding.binding);
								return false;
							}
						}
					}
					else
					{
						DescriptorBindingInfo bindingInfo{};
						vk::DescriptorSetLayoutBinding& layoutBinding = bindingInfo.Binding;
						layoutBinding.binding = spvBinding.binding;
						layoutBinding.descriptorType = static_cast<vk::DescriptorType>(spvBinding.descriptor_type);
						layoutBinding.descriptorCount = 1;
						for (uint32_t k = 0; k < spvBinding.array.dims_count; ++k)
							layoutBinding.descriptorCount *= spvBinding.array.dims[k];
						layoutBinding.stageFlags = stage;

						bindingInfo.BlockSize = spvBinding.block.size;
						if (spvBinding.block.member_count > 0)
						{
							bindingInfo.BlockMemberOffsets.resize(spvBinding.block.member_count);
							for (uint32_t k = 0; k < spvBinding.block.member_count; ++k)
								bindingInfo.BlockMemberOffsets[k] = spvBinding.block.members[k].offset;
						}

						descriptorSetInfo.BindingInfos.emplace(spvBinding.binding, bindingInfo);
					}
				}
			}
		}

		uint32_t pushConstantRangeCount = 0;
		result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantRangeCount, nullptr);
		if (result != SPV_REFLECT_RESULT_SUCCESS)
		{
			return false;
		}

		if (pushConstantRangeCount > 0)
		{
			std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantRangeCount);
			result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantRangeCount, pushConstants.data());
			if (result != SPV_REFLECT_RESULT_SUCCESS)
			{
				return false;
			}

			m_pushConstantRanges.resize(pushConstantRangeCount);
			for (uint32_t i = 0; i < pushConstantRangeCount; ++i)
			{
				const SpvReflectBlockVariable& spvPushConstant = *(pushConstants[i]);

				bool matched = false;
				for (auto& existingRange : m_pushConstantRanges)
				{
					if (existingRange.size != 0 && existingRange.offset == spvPushConstant.offset)
					{
						if (existingRange.size != spvPushConstant.size)
						{
							Logger::Error("Push constant range size mismatch ({} != {}) between stages.", existingRange.size, spvPushConstant.size);
							return false;
						}

						existingRange.stageFlags |= stage;
						matched = true;
						break;
					}
				}

				if (matched)
					continue;

				m_pushConstantRanges[i] = vk::PushConstantRange(stage, spvPushConstant.offset, spvPushConstant.size);
			}
		}

		uint32_t specialisationConstantCount = 0;
		spvReflectEnumerateSpecializationConstants(&module, &specialisationConstantCount, nullptr);
		if (result != SPV_REFLECT_RESULT_SUCCESS)
		{
			return false;
		}

		if (specialisationConstantCount > 0)
		{
			std::vector<SpvReflectSpecializationConstant*> specialisationConstants(specialisationConstantCount);
			result = spvReflectEnumerateSpecializationConstants(&module, &specialisationConstantCount, specialisationConstants.data());
			if (result != SPV_REFLECT_RESULT_SUCCESS)
			{
				return false;
			}

			for (uint32_t i = 0; i < specialisationConstantCount; ++i)
			{
				const SpvReflectSpecializationConstant& spvSpecialisationConstant = *(specialisationConstants[i]);

				if (spvSpecialisationConstant.type->op != SpvOpTypeInt)
				{
					Logger::Error("Currently only integer specialisation constants are supported.");
					return false;
				}

				const auto& existingSearch = m_specialisationConstants.find(spvSpecialisationConstant.name);
				if (existingSearch != m_specialisationConstants.end())
				{
					existingSearch->second.stageFlags |= stage;
				}
				else
				{
					SpecialisationConstantInfo constantInfo;
					constantInfo.stageFlags = stage;
					constantInfo.entry = vk::SpecializationMapEntry(spvSpecialisationConstant.constant_id, 0, sizeof(uint32_t));
					constantInfo.value = spvSpecialisationConstant.default_value.value.sint32_value;
					m_specialisationConstants.emplace(spvSpecialisationConstant.name, constantInfo);
				}
			}
		}

		return true;
	}

	bool PipelineLayout::ValidateInputsOutputs(std::unordered_map<vk::ShaderStageFlagBits, ReflectionData>& reflectionData)
	{
		// Validate inputs match outputs.
		const auto& fragmentEntry = reflectionData.find(vk::ShaderStageFlagBits::eFragment);
		if (fragmentEntry != reflectionData.cend())
		{
			ReflectionData& fragmentData = fragmentEntry->second;

			uint32_t fragmentOutputCount = static_cast<uint32_t>(fragmentData.OutputVariables.size());
			uint32_t attachmentCount = static_cast<uint32_t>(m_attachmentFormats.size());
			if (fragmentOutputCount != attachmentCount)
			{
				Logger::Error("Material '{}' has an output attachment mismatch. Material contains {} outputs but fragment shader contains {}.", m_material.GetName(), attachmentCount, fragmentOutputCount);
				return false;
			}

			const auto& vertexEntry = reflectionData.find(vk::ShaderStageFlagBits::eVertex);
			if (vertexEntry != reflectionData.cend())
			{
				ReflectionData& vertexData = vertexEntry->second;

				uint32_t vertexOutputCount = static_cast<uint32_t>(vertexData.OutputVariables.size());
				uint32_t fragmentInputCount = static_cast<uint32_t>(fragmentData.InputVariables.size());
				if (vertexOutputCount != fragmentInputCount)
				{
					Logger::Error("Shader '{}' contains {} vertex outputs but {} fragment inputs.", m_material.GetName(), vertexOutputCount, fragmentInputCount);
					return false;
				}

				std::sort(vertexData.OutputVariables.begin(), vertexData.OutputVariables.end(), [](const auto* lhs, const auto* rhs) { return lhs->location < rhs->location; });
				std::sort(fragmentData.InputVariables.begin(), fragmentData.InputVariables.end(), [](const auto* lhs, const auto* rhs) { return lhs->location < rhs->location; });

				for (uint32_t i = 0; i < vertexOutputCount; ++i)
				{
					if (vertexData.OutputVariables[i]->format != fragmentData.InputVariables[i]->format)
					{
						Logger::Error("Shader '{}' vertex output {} does not match type of fragment input {}.", m_material.GetName(), i, i);
						return false;
					}
				}
			}
			else
			{
				Logger::Error("Failed to match fragment program with vertex program for shader '{}'.", m_material.GetName());
				return false;
			}
		}

		return true;
	}

	bool PipelineLayout::Initialise(const Device& device, uint32_t concurrentFrames)
	{
		const vk::Device& deviceImp = device.Get();

		m_concurrentFrames = concurrentFrames;
		m_shaderModules.reserve(m_material.GetProgramData().size());
		std::unordered_map<vk::ShaderStageFlagBits, ReflectionData> reflectionData;
		std::vector<SpvReflectShaderModule> modules;
		reflectionData.reserve(m_material.GetProgramData().size());
		for (const auto& program : m_material.GetProgramData())
		{
			vk::ShaderStageFlagBits stage;
			if (!TryGetVulkanProgramType(program.first, stage))
			{
				Logger::Error("Unhandled program type encountered while building material '{}'.", m_material.GetName());
				return false;
			}

			ReflectionData entry = { .Data = program.second };
			SpvReflectShaderModule module;
			if (!PerformReflection(stage, program.second, module, entry))
			{
				spvReflectDestroyShaderModule(&module);

				Logger::Error("Failed to reflect {} program for shader '{}'.", GetProgramTypeName(stage), m_material.GetName());
				return false;
			}

			modules.push_back(std::move(module));
			reflectionData.emplace(stage, entry);
		}

		m_attachmentFormats.reserve(m_material.GetAttachmentFormats().size());
		for (const AttachmentFormat& format : m_material.GetAttachmentFormats())
		{
			vk::Format vulkanFormat;
			if (!TryGetVulkanAttachmentFormat(format, vulkanFormat))
			{
				Logger::Error("Unhandled attachment format encountered while building material '{}'.", m_material.GetName());
				return false;
			}

			if (vulkanFormat == vk::Format::eUndefined)
				m_swapchainFormatIndices.push_back(static_cast<uint32_t>(m_attachmentFormats.size()));

			m_attachmentFormats.push_back(vulkanFormat);
		}

		bool validInputsOutputs = ValidateInputsOutputs(reflectionData);

		for (SpvReflectShaderModule& m : modules)
			spvReflectDestroyShaderModule(&m);

		if (!validInputsOutputs)
		{
			return false;
		}

		for (const auto& pair : reflectionData)
		{
			vk::ShaderStageFlagBits stage = pair.first;
			const ReflectionData& entry = pair.second;
			vk::ShaderModuleCreateInfo createInfo(vk::ShaderModuleCreateFlags(), entry.Data.size(), reinterpret_cast<const uint32_t*>(entry.Data.data()));

			vk::UniqueShaderModule shaderModule = deviceImp.createShaderModuleUnique(createInfo);
			if (!shaderModule.get())
			{
				Logger::Error("Failed to create {} program for shader '{}'.", GetProgramTypeName(stage), m_material.GetName());
				return false;
			}

			m_shaderModules.push_back(std::make_pair(stage, std::move(shaderModule)));
		}

		return true;
	}
}