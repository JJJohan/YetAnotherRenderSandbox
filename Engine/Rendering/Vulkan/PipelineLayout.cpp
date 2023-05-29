#include "PipelineLayout.hpp"
#include "Device.hpp"
#include "Core/Logging/Logger.hpp"
#include "OS/Files.hpp"
#include <filesystem>
#include <glm/glm.hpp>

using namespace Engine::Logging;
using namespace Engine::OS;
using namespace Engine::Rendering;

namespace Engine::Rendering::Vulkan
{
	PipelineLayout::PipelineLayout()
		: m_name()
		, m_pipelineLayout(nullptr)
		, m_graphicsPipeline(nullptr)
		, m_shaderModules()
		, m_bindingDescriptions()
		, m_attributeDescriptions()
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

	bool PipelineLayout::Rebuild(const Device& device, const std::vector<vk::Format>& attachmentFormats, vk::Format depthFormat,
		const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts, const std::vector<vk::PushConstantRange>& pushConstantRanges)
	{
		m_graphicsPipeline.reset();
		m_pipelineLayout.reset();

		const vk::Device& deviceImp = device.Get();

		std::vector<vk::PipelineShaderStageCreateInfo> shaderStageInfos;
		shaderStageInfos.reserve(m_shaderModules.size());
		for (const auto& module : m_shaderModules)
		{
			vk::PipelineShaderStageCreateInfo& vertShaderStageInfo = shaderStageInfos.emplace_back(vk::PipelineShaderStageCreateInfo());
			vertShaderStageInfo.stage = module.first;
			vertShaderStageInfo.module = module.second.get();
			vertShaderStageInfo.pName = "main";
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
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.polygonMode = vk::PolygonMode::eFill;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = vk::CullModeFlagBits::eBack;
		rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
		rasterizer.depthClampEnable = true;

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
		depthStencil.depthWriteEnable = depthFormat != vk::Format::eUndefined;
		depthStencil.depthTestEnable = depthFormat != vk::Format::eUndefined;
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

		std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(attachmentFormats.size());
		for (size_t i = 0; i < attachmentFormats.size(); ++i)
			blendAttachments[i] = colorBlendAttachment;

		// Color blend state
		vk::PipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
		colorBlending.attachmentCount = static_cast<uint32_t>(attachmentFormats.size());
		colorBlending.pAttachments = blendAttachments.data();

		// Pipeline layout
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
		pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());

		m_pipelineLayout = deviceImp.createPipelineLayoutUnique(pipelineLayoutInfo);
		if (!m_pipelineLayout.get())
		{
			Logger::Error("Failed to create pipeline layout.");
			return false;
		}

		vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
		pipelineRenderingCreateInfo.colorAttachmentCount = static_cast<uint32_t>(attachmentFormats.size());
		pipelineRenderingCreateInfo.pColorAttachmentFormats = attachmentFormats.data();
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

		vk::ResultValue<vk::UniquePipeline> result = deviceImp.createGraphicsPipelineUnique(nullptr, pipelineInfo);
		if (result.result != vk::Result::eSuccess)
		{
			Logger::Error("Failed to create graphics pipeline.");
			return false;
		}

		m_graphicsPipeline = std::move(result.value);
		return true;
	}

	inline std::string GetProgramTypeName(vk::ShaderStageFlagBits type)
	{
		switch (type)
		{
		case vk::ShaderStageFlagBits::eVertex:
			return "Vertex";
		case vk::ShaderStageFlagBits::eFragment:
			return "Fragment";
		default:
			return "Unknown";
		}
	}

	bool PipelineLayout::Initialise(const Device& device, const std::string& name,
		const std::unordered_map<vk::ShaderStageFlagBits, std::vector<uint8_t>>& programs,
		const std::vector<vk::VertexInputBindingDescription>& bindingDescriptions,
		const std::vector<vk::VertexInputAttributeDescription>& attributeDescriptions,
		const std::vector<vk::Format>& attachmentFormats, vk::Format depthFormat,
		const std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
		const std::vector<vk::PushConstantRange>& pushConstantRanges)
	{
		m_name = name;

		const vk::Device& deviceImp = device.Get();

		m_shaderModules.reserve(programs.size());
		for (const auto& program : programs)
		{
			vk::ShaderModuleCreateInfo createInfo(vk::ShaderModuleCreateFlags(), program.second.size(), reinterpret_cast<const uint32_t*>(program.second.data()));

			vk::UniqueShaderModule shaderModule = deviceImp.createShaderModuleUnique(createInfo);
			if (!shaderModule.get())
			{
				Logger::Error("Failed to create {} program for shader '{}'.", GetProgramTypeName(program.first), name);
				return false;
			}

			vk::ShaderStageFlagBits stage = program.first;
			m_shaderModules.push_back(std::make_pair(stage, std::move(shaderModule)));
		}

		m_bindingDescriptions = bindingDescriptions;
		m_attributeDescriptions = attributeDescriptions;

		if (!Rebuild(device, attachmentFormats, depthFormat, descriptorSetLayouts, pushConstantRanges))
		{
			return false;
		}

		return true;
	}
}