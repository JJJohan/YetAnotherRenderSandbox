#pragma once

#include <vector>
#include <memory>

namespace Engine::Rendering
{
	class ICommandBuffer;
	class IDevice;
	class IPhysicalDevice;

	enum class CommandPoolFlags
	{
		None = 0,
		Transient = 1,
		Reset = 2,
		Protected = 4
	};

	inline CommandPoolFlags& operator&=(CommandPoolFlags& a, CommandPoolFlags b)
	{
		a = static_cast<CommandPoolFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
		return a;
	}

	inline CommandPoolFlags& operator|=(CommandPoolFlags& a, CommandPoolFlags b)
	{
		a = static_cast<CommandPoolFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
		return a;
	}

	inline CommandPoolFlags operator|(CommandPoolFlags a, CommandPoolFlags b)
	{
		return a |= b;
	}

	inline CommandPoolFlags operator&(CommandPoolFlags a, CommandPoolFlags b)
	{
		return a &= b;
	}

	class ICommandPool
	{
	public:
		ICommandPool() = default;
		virtual ~ICommandPool() = default;

		virtual bool Initialise(std::string_view name, const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t queueFamilyIndex, CommandPoolFlags flags) = 0;

		virtual std::vector<std::unique_ptr<ICommandBuffer>> CreateCommandBuffers(std::string_view name, const IDevice& device, uint32_t count) const = 0;
		virtual std::unique_ptr<ICommandBuffer> BeginResourceCommandBuffer(const IDevice& device) const = 0;
		virtual void Reset(const IDevice& device) const = 0;
		inline uint32_t GetQueueFamilyIndex() const { return m_queueFamilyIndex; }

	protected:
		uint32_t m_queueFamilyIndex;
	};
}