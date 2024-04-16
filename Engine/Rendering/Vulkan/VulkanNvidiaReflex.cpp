#include "VulkanNvidiaReflex.hpp"
#include "Core/Logger.hpp"

static PFN_vkSetLatencySleepModeNV _vkSetLatencySleepModeNVAddr = 0;
static PFN_vkLatencySleepNV _vkLatencySleepNV = 0;
static PFN_vkSetLatencyMarkerNV _vkSetLatencyMarkerNV = 0;

namespace Engine::Rendering::Vulkan
{
	VulkanNvidiaReflex::VulkanNvidiaReflex(const IDevice& device, const ISwapChain& swapChain)
		: m_device(device)
		, m_swapChain(swapChain)
		, m_semaphore(nullptr)
	{
	}

	VulkanNvidiaReflex::~VulkanNvidiaReflex()
	{
	}

	bool VulkanNvidiaReflex::Initialise(const IPhysicalDevice& physicalDevice)
	{
		const PhysicalDevice& physicalDeviceImp = static_cast<const PhysicalDevice&>(physicalDevice);
		if (!physicalDeviceImp.SupportsOptionalExtension(VK_NV_LOW_LATENCY_2_EXTENSION_NAME))
			return false;

		const vk::Device& deviceImp = static_cast<const Device&>(m_device).Get();
		_vkSetLatencySleepModeNVAddr = (PFN_vkSetLatencySleepModeNV)deviceImp.getProcAddr("vkSetLatencySleepModeNV");
		_vkLatencySleepNV = (PFN_vkLatencySleepNV)deviceImp.getProcAddr("vkLatencySleepNV");
		_vkSetLatencyMarkerNV = (PFN_vkSetLatencyMarkerNV)deviceImp.getProcAddr("vkSetLatencyMarkerNV");
		if (!_vkSetLatencySleepModeNVAddr || !_vkLatencySleepNV || !_vkSetLatencyMarkerNV)
			return false;

		m_semaphore = std::make_unique<Semaphore>();
		if (!m_semaphore->Initialise("ReflexSemaphore", m_device, false))
			return false;

		if (!SetMode(NvidiaReflexMode::On))
			return false;

		m_supported = true;
		return true;
	}

	bool VulkanNvidiaReflex::Sleep() const
	{
		if (!m_supported)
			return true;

		const vk::Device& deviceImp = static_cast<const Device&>(m_device).Get();
		const vk::SwapchainKHR& swapChainImp = static_cast<const SwapChain&>(m_swapChain).Get();

		uint64_t value = deviceImp.getSemaphoreCounterValue(m_semaphore->Get()) + 1;

		VkLatencySleepInfoNV sleepInfo
		{
			.sType = VK_STRUCTURE_TYPE_LATENCY_SLEEP_INFO_NV,
			.signalSemaphore = m_semaphore->Get(),
			.value = value
		};

		VkResult sleepResult = _vkLatencySleepNV(deviceImp, swapChainImp, &sleepInfo);
		if (sleepResult != VK_SUCCESS)
		{
			Logger::Error("Nvidia Reflex - Failed to perform sleep: {}", int(sleepResult));
			return false;
		}

		vk::SemaphoreWaitInfo waitInfo{};
		waitInfo.semaphoreCount = 1;
		waitInfo.pSemaphores = &m_semaphore->Get();
		waitInfo.pValues = &value;
		vk::Result result = deviceImp.waitSemaphores(waitInfo, UINT64_MAX);
		if (result != vk::Result::eSuccess)
		{
			Logger::Error("Nvidia Reflex - Failed to wait for semaphore: {}", int(result));
			return true;
		}

		return true;
	}

	void VulkanNvidiaReflex::SetMarker(NvidiaReflexMarker marker) const
	{
		if (!m_supported)
			return;

		const vk::Device& deviceImp = static_cast<const Device&>(m_device).Get();
		const vk::SwapchainKHR& swapChainImp = static_cast<const SwapChain&>(m_swapChain).Get();

		VkSetLatencyMarkerInfoNV markerInfo =
		{
			.sType = VK_STRUCTURE_TYPE_SET_LATENCY_MARKER_INFO_NV,
			.marker = static_cast<VkLatencyMarkerNV>(marker)
		};

		_vkSetLatencyMarkerNV(deviceImp, swapChainImp, &markerInfo);
	}

	bool VulkanNvidiaReflex::SetMode(NvidiaReflexMode mode)
	{
		if (m_mode == mode)
			return true;

		const vk::Device& deviceImp = static_cast<const Device&>(m_device).Get();
		const vk::SwapchainKHR& swapChainImp = static_cast<const SwapChain&>(m_swapChain).Get();

		VkLatencySleepModeInfoNV sleepModeInfo
		{
			.sType = VK_STRUCTURE_TYPE_LATENCY_SLEEP_MODE_INFO_NV,
			.lowLatencyMode = mode >= NvidiaReflexMode::On,
			.lowLatencyBoost = mode == NvidiaReflexMode::OnPlusBoost
		};

		VkResult result = _vkSetLatencySleepModeNVAddr(deviceImp, swapChainImp, &sleepModeInfo);

		if (result != VK_SUCCESS)
		{
			Logger::Error("Nvidia Reflex - Failed to set latency sleep mode: {}", int(result));
		}

		return result == VK_SUCCESS;
	}
}