#pragma once

#include "../NvidiaReflex.hpp"
#include "PhysicalDevice.hpp"
#include "SwapChain.hpp"
#include "Device.hpp"
#include "Semaphore.hpp"
#include <memory>

namespace Engine::Rendering::Vulkan
{
	class VulkanNvidiaReflex : public NvidiaReflex
	{
	public:
		VulkanNvidiaReflex(const IDevice& device, const ISwapChain& swapChain);
		virtual ~VulkanNvidiaReflex() override;
		bool Initialise(const IPhysicalDevice& physicalDevice);
		virtual void SetMarker(NvidiaReflexMarker marker) const override;
		virtual bool SetMode(NvidiaReflexMode mode) override;
		virtual bool Sleep() const override;

	private:
		const IDevice& m_device;
		const ISwapChain& m_swapChain;
		std::unique_ptr<Semaphore> m_semaphore;
	};
}