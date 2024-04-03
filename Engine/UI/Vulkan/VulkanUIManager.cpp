#include "VulkanUIManager.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/Vulkan/VulkanRenderer.hpp"
#include "Rendering/Vulkan/Device.hpp"
#include "Rendering/Vulkan/SwapChain.hpp"
#include "Rendering/Vulkan/PhysicalDevice.hpp"
#include "Rendering/Vulkan/CommandBuffer.hpp"
#include "Rendering/Vulkan/VulkanTypesInterop.hpp"
#include "Core/Logging/Logger.hpp"
#include "OS/Window.hpp"
#include "imgui.h"
#include "implot.h"

#include <vulkan/vulkan.hpp>
#include "backends/imgui_impl_vulkan.h"

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
		ImPlot::CreateContext();
	}

	VulkanUIManager::~VulkanUIManager()
	{
		ImPlot::DestroyContext();
		ImGui_ImplVulkan_Shutdown();
	}

	bool VulkanUIManager::SetupRenderBackend(const vk::Instance& instance, VulkanRenderer& renderer)
	{
		uint32_t concurrentFrames = renderer.GetConcurrentFrameCount();
		const Device& device = static_cast<const Device&>(renderer.GetDevice());
		const PhysicalDevice& physicalDevice = static_cast<const PhysicalDevice&>(renderer.GetPhysicalDevice());

		m_descriptorPool = std::make_unique<DescriptorPool>();
		if (!m_descriptorPool->Initialise(device, concurrentFrames, {}, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet))
		{
			return false;
		}

		uint32_t queueFamily = physicalDevice.GetQueueFamilyIndices().GraphicsFamily.value();
		const vk::Queue& queue = device.GetGraphicsQueue();
		vk::Format format = GetVulkanFormat(renderer.GetSwapChain().GetFormat());

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
		initInfo.MinImageCount = std::max(2U, concurrentFrames);
		initInfo.ImageCount = std::max(2U, concurrentFrames);
		initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		initInfo.Allocator = nullptr;
		initInfo.CheckVkResultFn = check_vk_result;
		initInfo.UseDynamicRendering = true;
		initInfo.PipelineRenderingCreateInfo = vk::PipelineRenderingCreateInfo(0U, 1U, &format);

		if (!ImGui_ImplVulkan_Init(&initInfo))
		{
			return false;
		}

		return true;
	}

	bool VulkanUIManager::Initialise(const vk::Instance& instance, VulkanRenderer& renderer)
	{
		if (!UIManager::Initialise())
		{
			return false;
		}

		if (!SetupRenderBackend(instance, renderer))
		{
			return false;
		}

		return true;
	}

	bool VulkanUIManager::Rebuild(const vk::Instance& instance, VulkanRenderer& renderer)
	{
		ImGui_ImplVulkan_Shutdown();

		if (!SetupRenderBackend(instance, renderer))
		{
			return false;
		}

		return true;
	}

	void VulkanUIManager::Draw(const ICommandBuffer& commandBuffer, float width, float height)
	{
		if (!UIManager::Draw(width, height))
			return;

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
			ImGui_ImplVulkan_RenderDrawData(draw_data, static_cast<const CommandBuffer&>(commandBuffer).Get());
		}
	}
}