#include "PipelineLayout.hpp"
#include "Device.hpp"
#include "RenderPass.hpp"
#include "Core/Logging/Logger.hpp"
#include "OS/Files.hpp"
#include <filesystem>

using namespace Engine::Logging;
using namespace Engine::OS;
using namespace Engine::Rendering;

namespace Engine::Rendering::Vulkan
{
	PipelineLayout::PipelineLayout()
		: Shader()
		, m_pipelineLayout(nullptr)
		, m_graphicsPipeline(nullptr)
	{
	}

	void PipelineLayout::Destroy(const Device& device)
	{
		if (m_graphicsPipeline)
		{
			vkDestroyPipeline(device.Get(), m_graphicsPipeline, nullptr);
			m_graphicsPipeline = nullptr;
		}

		if (m_pipelineLayout)
		{
			vkDestroyPipelineLayout(device.Get(), m_pipelineLayout, nullptr);
			m_pipelineLayout = nullptr;
		}
	}

	bool PipelineLayout::IsValid() const
	{
		return m_graphicsPipeline != nullptr && m_pipelineLayout != nullptr;
	}

	VkPipeline PipelineLayout::GetGraphicsPipeline() const
	{
		return m_graphicsPipeline;
	}

	VkShaderStageFlagBits GetShaderStage(ShaderProgramType type)
	{
		switch (type)
		{
		case ShaderProgramType::VERTEX:
			return VK_SHADER_STAGE_VERTEX_BIT;
		case ShaderProgramType::FRAGMENT:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		default:
			Logger::Warning("Unexpected shader program type, returning VK_SHADER_STAGE_ALL.");
			return VK_SHADER_STAGE_ALL;
		}
	}

	void DestroyShaderModules(const VkDevice& device, const std::vector<std::pair<VkShaderStageFlagBits, VkShaderModule>>& shaderModules)
	{
		for (auto& module : shaderModules)
		{
			vkDestroyShaderModule(device, module.second, nullptr);
		}
	}

	bool PipelineLayout::Create(const Device& device, const std::string& name, const std::unordered_map<ShaderProgramType, std::vector<uint8_t>>& programs, const RenderPass& renderPass)
	{
		m_name = name;

		VkDevice deviceImp = device.Get();

		std::vector<std::pair<VkShaderStageFlagBits, VkShaderModule>> shaderModules{};
		shaderModules.reserve(programs.size());
		for (const auto& program : programs)
		{
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = program.second.size();
			createInfo.pCode = reinterpret_cast<const uint32_t*>(program.second.data());

			VkShaderModule shaderModule{};
			if (vkCreateShaderModule(deviceImp, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
			{
				Logger::Error("Failed to create {} program for shader '{}'.", GetProgramTypeName(program.first), name);
				DestroyShaderModules(deviceImp, shaderModules);
				return false;
			}

			VkShaderStageFlagBits stage = GetShaderStage(program.first);
			shaderModules.push_back(std::make_pair(stage, shaderModule));
		}

		std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos{};
		shaderStageInfos.reserve(shaderModules.size());
		for (const auto& module : shaderModules)
		{
			VkPipelineShaderStageCreateInfo& vertShaderStageInfo = shaderStageInfos.emplace_back(VkPipelineShaderStageCreateInfo{});
			vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertShaderStageInfo.stage = module.first;
			vertShaderStageInfo.module = module.second;
			vertShaderStageInfo.pName = "main";
		}

		std::vector<VkDynamicState> dynamicStates =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		// Vertex input state
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

		// Input assembly state
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// Viewport state
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		// Rasterizer state
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

		// Multisampling state
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		// Depth and stencil state
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencil.depthWriteEnable = VK_FALSE;
		depthStencil.depthTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

		// Color blend attachment state
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		// Color blend state
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		// Pipeline layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0; // Optional
		pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
		pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

		if (vkCreatePipelineLayout(deviceImp, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
		{
			Logger::Error("Failed to create pipeline layout.");
			DestroyShaderModules(deviceImp, shaderModules);
			return false;
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageInfos.size());
		pipelineInfo.pStages = shaderStageInfos.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr; // Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_pipelineLayout;
		pipelineInfo.renderPass = renderPass.Get();
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		if (vkCreateGraphicsPipelines(deviceImp, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS)
		{
			Logger::Error("Failed to create graphics pipeline.");
			vkDestroyPipelineLayout(deviceImp, m_pipelineLayout, nullptr);
			DestroyShaderModules(deviceImp, shaderModules);
			m_pipelineLayout = nullptr;
			return false;
		}

		DestroyShaderModules(deviceImp, shaderModules);
		return true;
	}
}