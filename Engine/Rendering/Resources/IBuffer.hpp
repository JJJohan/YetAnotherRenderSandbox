#pragma once

#include <stdint.h>
#include "Core/Logging/Logger.hpp"
#include "../Types.hpp"

namespace Engine::Rendering
{
	class ICommandBuffer;
	class IRenderImage;
	class IDevice;

	class IBuffer
	{
	public:
		IBuffer()
			: m_size(0)
			, m_mappedDataPtr(nullptr)
		{
		}

		virtual ~IBuffer() = default;

		virtual bool Initialise(uint64_t size, BufferUsageFlags bufferUsage,
			MemoryUsage memoryUsage, AllocationCreateFlags createFlags, SharingMode sharingMode) = 0;
		virtual bool UpdateContents(const void* data, size_t size) = 0;
		virtual uint64_t GetDeviceAddress(const IDevice& device) = 0;
		virtual void Copy(const ICommandBuffer& commandBuffer, const IBuffer& destination, size_t size) const = 0;
		virtual void CopyToImage(uint32_t mipLevel, const ICommandBuffer& commandBuffer, const IRenderImage& destination) const = 0;

		inline uint64_t Size() const { return m_size; }

		template <typename T>
		bool GetMappedMemory(T** mappedMemory) const
		{
			if (m_mappedDataPtr == nullptr)
			{
				Engine::Logging::Logger::Error("Memory is not mapped.");
				return false;
			}

			*mappedMemory = static_cast<T*>(m_mappedDataPtr);
			return true;
		}

	protected:
		uint64_t m_size;
		void* m_mappedDataPtr;
	};
}