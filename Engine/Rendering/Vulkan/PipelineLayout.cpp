#include "PipelineLayout.hpp"
#include "Device.hpp"
#include "RenderPass.hpp"
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
		: Shader()
		, m_pipelineLayout(nullptr)
		, m_graphicsPipeline(nullptr)
		, m_descriptorSetLayout(nullptr)
		, m_shaderModules()
	{
	}

	bool PipelineLayout::IsValid() const
	{
		return m_graphicsPipeline.get() && m_pipelineLayout.get();
	}

	const vk::PipelineLayout& PipelineLayout::Get() const
	{
		return m_pipelineLayout.get();
	}

	const vk::Pipeline& PipelineLayout::GetGraphicsPipeline() const
	{
		return m_graphicsPipeline.get();
	}

	vk::ShaderStageFlagBits GetShaderStage(ShaderProgramType type)
	{
		switch (type)
		{
		case ShaderProgramType::VERTEX:
			return vk::ShaderStageFlagBits::eVertex;
		case ShaderProgramType::FRAGMENT:
			return vk::ShaderStageFlagBits::eFragment;
		default:
			Logger::Warning("Unexpected shader program type, enabling all bits.");
			return vk::ShaderStageFlagBits::eAllGraphics;
		}
	}

	bool PipelineLayout::SetupDescriptorSetLayout(const Device& device)
	{
		std::array<vk::DescriptorSetLayoutBinding, 2> layoutBindings =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo(vk::DescriptorSetLayoutCreateFlags(), layoutBindings);

		const vk::Device& deviceImp = device.Get();
		m_descriptorSetLayout = deviceImp.createDescriptorSetLayoutUnique(layoutInfo);
		if (!m_descriptorSetLayout.get())
		{
			Logger::Error("Failed to create descriptor set layout.");
			return false;
		}

		return true;
	}

	std::vector<vk::DescriptorSetLayout> PipelineLayout::GetDescriptorSetLayouts() const
	{
		return { m_descriptorSetLayout.get() }; // hard-coded to 1 for now.
	}

	bool PipelineLayout::Rebuild(const Device& device, const RenderPass& renderPass)
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

		std::array<vk::VertexInputBindingDescription, 2> bindingDescriptions =
		{ {
			vk::VertexInputBindingDescription(0, sizeof(glm::vec3), vk::VertexInputRate::eVertex),
			vk::VertexInputBindingDescription(1, sizeof(glm::vec2), vk::VertexInputRate::eVertex)
		} };

		std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions =
		{ {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0),
			vk::VertexInputAttributeDescription(1, 1, vk::Format::eR32G32Sfloat, 0)
		} };

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo(vk::PipelineVertexInputStateCreateFlags(), bindingDescriptions, attributeDescriptions);

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

		// Multisampling state
		vk::PipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = renderPass.GetSampleCount();
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		// Depth and stencil state
		vk::PipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.depthCompareOp = vk::CompareOp::eLess;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthTestEnable = VK_TRUE;
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

		// Color blend state
		vk::PipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		// Pipeline layout
		std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = GetDescriptorSetLayouts();
		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		m_pipelineLayout = deviceImp.createPipelineLayoutUnique(pipelineLayoutInfo);
		if (!m_pipelineLayout.get())
		{
			Logger::Error("Failed to create pipeline layout.");
			return false;
		}

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
		pipelineInfo.renderPass = renderPass.Get();
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		vk::ResultValue<vk::UniquePipeline> result = deviceImp.createGraphicsPipelineUnique(nullptr, pipelineInfo);
		if (result.result != vk::Result::eSuccess)
		{
			Logger::Error("Failed to create graphics pipeline.");
			return false;
		}

		m_graphicsPipeline = std::move(result.value);
		return true;
	}

	bool PipelineLayout::Initialise(const Device& device, const std::string& name,
		const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs,
		const RenderPass& renderPass)
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

			vk::ShaderStageFlagBits stage = GetShaderStage(program.first);
			m_shaderModules.push_back(std::make_pair(stage, std::move(shaderModule)));
		}

		if (!SetupDescriptorSetLayout(device))
		{
			return false;
		}

		Rebuild(device, renderPass);
	}
}