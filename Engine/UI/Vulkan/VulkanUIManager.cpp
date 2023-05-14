#include "VulkanUIManager.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/Vulkan/VulkanRenderer.hpp"
#include "Rendering/Vulkan/Device.hpp"
#include "Rendering/Vulkan/PhysicalDevice.hpp"
#include "Rendering/Vulkan/RenderPass.hpp"
#include "Rendering/Vulkan/Buffer.hpp"
#include "Core/Logging/Logger.hpp"
#include "OS/Window.hpp"
#include "imgui.h"

#include <vulkan/vulkan.hpp>
#include "backends/imgui_impl_vulkan.h"
#ifdef _WIN32
#include "backends/imgui_impl_win32.h"
#else
#error Not implemented.
#endif

// Currently the CPP file itself is included to expose recreating the pipeline without having to recreate the entire rendering backend when e.g. multi-sampling changes.
#include "backends/imgui_impl_vulkan.cpp"

using namespace Engine::OS;
using namespace Engine::Rendering;
using namespace Engine::Rendering::Vulkan;
using namespace Engine::Logging;

namespace Engine::UI::Vulkan
{
	static void check_vk_result(VkResult err)
	{
		if (err == 0)
			return;
		Logger::Error("ImGui Vulkan Error: VkResult = {}", static_cast<int32_t>(err));
		if (err < 0)
			abort();
	}

	VulkanUIManager::VulkanUIManager(const Window& window, Renderer& renderer)
		: UIManager::UIManager(window, renderer)
		, m_descriptorPool()
	{
	}

	VulkanUIManager::~VulkanUIManager()
	{
		ImGui_ImplVulkan_Shutdown();
#ifdef _WIN32
		ImGui_ImplWin32_Shutdown();
#else
#error Not implemented.
#endif
	}

	bool VulkanUIManager::Initialise(const vk::Instance& instance, VulkanRenderer& renderer)
	{
		uint32_t concurrentFrames = renderer.GetConcurrentFrameCount();
		const Device& device = renderer.GetDevice();
		const PhysicalDevice& physicalDevice = renderer.GetPhysicalDevice();
		const RenderPass& renderPass = renderer.GetRenderPass();

		m_descriptorPool = std::make_unique<DescriptorPool>();
		if (!m_descriptorPool->Initialise(device, concurrentFrames, {}))
		{
			return false;
		}

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		uint32_t queueFamily = physicalDevice.GetQueueFamilyIndices().GraphicsFamily.value();
		const vk::Queue& queue = device.GetGraphicsQueue();

		// Setup Platform/Renderer backends
		ImGui_ImplVulkan_InitInfo initInfo = {};
		initInfo.Instance = instance;
		initInfo.PhysicalDevice = physicalDevice.Get();
		initInfo.Device = device.Get();
		initInfo.QueueFamily = queueFamily;
		initInfo.Queue = queue;
		initInfo.PipelineCache = nullptr;
		initInfo.DescriptorPool = m_descriptorPool->Get();
		initInfo.Subpass = 0;
		initInfo.MinImageCount = concurrentFrames;
		initInfo.ImageCount = concurrentFrames;
		initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		initInfo.Allocator = nullptr;
		initInfo.CheckVkResultFn = check_vk_result;
		if (!ImGui_ImplVulkan_Init(&initInfo, renderPass.Get()))
		{
			return false;
		}

#ifdef _WIN32
		if (!ImGui_ImplWin32_Init(m_window.GetHandle()))
		{
			return false;
		}
#else
#error Not implemented.
#endif

		return renderer.SubmitResourceCommand([](const vk::CommandBuffer& commandBuffer, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers)
			{
				ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
				return true;
			});
	}

	void VulkanUIManager::PostInitialise() const
	{
		ImGui_ImplVulkan_DestroyFontUploadObjects();
	}

	bool VulkanUIManager::Rebuild(const vk::Device& device, const vk::RenderPass& renderPass, vk::SampleCountFlagBits multiSampleCount) const
	{
		// This code block is partially taken from ImGui's Vulkan backend code to perform minimal rebuilds.

		ImGui_ImplVulkan_Data* bd = (ImGui_ImplVulkan_Data*)ImGui::GetIO().BackendRendererUserData;
		ImGui_ImplVulkan_InitInfo* v = &bd->VulkanInitInfo;
		if (bd->ShaderModuleVert) { vkDestroyShaderModule(v->Device, bd->ShaderModuleVert, v->Allocator); bd->ShaderModuleVert = nullptr; }
		if (bd->ShaderModuleFrag) { vkDestroyShaderModule(v->Device, bd->ShaderModuleFrag, v->Allocator); bd->ShaderModuleFrag = nullptr; }
		if (bd->PipelineLayout) { vkDestroyPipelineLayout(v->Device, bd->PipelineLayout, v->Allocator); bd->PipelineLayout = nullptr; }
		if (bd->Pipeline) { vkDestroyPipeline(v->Device, bd->Pipeline, v->Allocator); bd->Pipeline = nullptr; }

		// Constants: we are using 'vec2 offset' and 'vec2 scale' instead of a full 3d projection matrix
		VkPushConstantRange push_constants[1] = {};
		push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_constants[0].offset = sizeof(float) * 0;
		push_constants[0].size = sizeof(float) * 4;
		VkDescriptorSetLayout set_layout[1] = { bd->DescriptorSetLayout };
		VkPipelineLayoutCreateInfo layout_info = {};
		layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_info.setLayoutCount = 1;
		layout_info.pSetLayouts = set_layout;
		layout_info.pushConstantRangeCount = 1;
		layout_info.pPushConstantRanges = push_constants;
		if (vkCreatePipelineLayout(v->Device, &layout_info, v->Allocator, &bd->PipelineLayout) != VK_SUCCESS)
		{
			return false;
		}

		ImGui_ImplVulkan_CreatePipeline(device, nullptr, nullptr, renderPass, static_cast<VkSampleCountFlagBits>(multiSampleCount), &bd->Pipeline, 0);

		return true;
	}

	void VulkanUIManager::Draw(const vk::CommandBuffer& commandBuffer, float width, float height)
	{
		if (!m_window.IsCursorVisible())
			return;

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize.x = width;
		io.DisplaySize.y = height;

		if (width < FLT_MIN || height < FLT_MIN)
			return;

#ifdef _WIN32
		ImGui_ImplWin32_NewFrame();
#else
#error Not implemented.
#endif

		ImGui_ImplVulkan_NewFrame();
		ImGui::NewFrame();

		for (const auto& callback : m_drawCallbacks)
		{
			callback(m_drawer);
		}

		ImGui::Render();

		ImDrawData* draw_data = ImGui::GetDrawData();
		const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
		if (!is_minimized)
		{
			ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
		}
	}
}