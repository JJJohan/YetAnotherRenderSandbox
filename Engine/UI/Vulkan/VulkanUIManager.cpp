#include "VulkanUIManager.hpp"
#include "Rendering/Renderer.hpp"
#include "Rendering/Vulkan/VulkanRenderer.hpp"
#include "Rendering/Vulkan/Device.hpp"
#include "Rendering/Vulkan/SwapChain.hpp"
#include "Rendering/Vulkan/PhysicalDevice.hpp"
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
		, m_initVersion(0)
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

	bool VulkanUIManager::SetupRenderBackend(const vk::Instance& instance, VulkanRenderer& renderer, vk::SampleCountFlagBits multiSampleCount)
	{
		uint32_t concurrentFrames = renderer.GetConcurrentFrameCount();
		const Device& device = renderer.GetDevice();
		const PhysicalDevice& physicalDevice = renderer.GetPhysicalDevice();

		m_descriptorPool = std::make_unique<DescriptorPool>();
		if (!m_descriptorPool->Initialise(device, concurrentFrames, {})) // Why does ImGui need an empty descriptor pool?
		{
			return false;
		}

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
		initInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(multiSampleCount);
		initInfo.Allocator = nullptr;
		initInfo.CheckVkResultFn = check_vk_result;
		initInfo.UseDynamicRendering = true;
		initInfo.ColorAttachmentFormat = static_cast<VkFormat>(renderer.GetSwapChain().GetFormat());
		if (!ImGui_ImplVulkan_Init(&initInfo, nullptr))
		{
			return false;
		}

		return true;
	}

	bool VulkanUIManager::SubmitRenderResourceSetup(VulkanRenderer& renderer)
	{
		uint8_t initVersion = m_initVersion;
		return renderer.SubmitResourceCommand([](const vk::CommandBuffer& commandBuffer, std::vector<std::unique_ptr<Buffer>>& temporaryBuffers)
			{
				ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
				return true;
			},
			[this, initVersion]() {
				if (initVersion == m_initVersion)
					ImGui_ImplVulkan_DestroyFontUploadObjects();
			});
	}

	bool VulkanUIManager::Initialise(const vk::Instance& instance, VulkanRenderer& renderer)
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		if (!SetupRenderBackend(instance, renderer, vk::SampleCountFlagBits::e1))
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

		if (!SubmitRenderResourceSetup(renderer))
		{
			return false;
		}

		return true;
	}

	bool VulkanUIManager::Rebuild(const vk::Instance& instance, VulkanRenderer& renderer, vk::SampleCountFlagBits multiSampleCount)
	{
		++m_initVersion;
		ImGui_ImplVulkan_Shutdown();

		if (!SetupRenderBackend(instance, renderer, multiSampleCount))
		{
			return false;
		}

		if (!SubmitRenderResourceSetup(renderer))
		{
			return false;
		}

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