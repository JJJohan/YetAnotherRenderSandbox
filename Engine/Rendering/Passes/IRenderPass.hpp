#pragma once

#include <vector>
#include <unordered_map>

namespace Engine::Rendering
{
	class IBuffer;
	class IRenderImage;
	class IDevice;
	class ICommandBuffer;

	class IRenderPass
	{
	public:
		IRenderPass()
			: m_bufferResourceMap()
			, m_bufferInputs()
			, m_imageResourceMap()
			, m_imageInputs()
		{
		}

		virtual ~IRenderPass() = default;
		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const = 0;

		inline const std::vector<const char*>& GetBufferInputs() const { return m_bufferInputs; }

		inline const std::vector<const char*> GetBufferOutputs() const
		{
			std::vector<const char*> bufferResourceNames;
			bufferResourceNames.reserve(m_bufferResourceMap.size());
			for (const auto& [key, value] : m_bufferResourceMap)
			{
				bufferResourceNames.emplace_back(key);
			}

			return bufferResourceNames;
		}

		inline const std::vector<const char*>& GetImageInputs()const { return m_imageInputs; }

		inline const std::vector<const char*> GetImageOutputs() const
		{
			std::vector<const char*> imageResourceNames;
			imageResourceNames.reserve(m_imageResourceMap.size());
			for (const auto& [key, value] : m_imageResourceMap)
			{
				imageResourceNames.emplace_back(key);
			}

			return imageResourceNames;
		}

		inline const IRenderImage& GetImageResource(const char* name) const
		{
			return *m_imageResourceMap.at(name);
		}

		inline const IBuffer& GetBufferResource(const char* name) const
		{
			return *m_bufferResourceMap.at(name);
		}

	protected:
		std::unordered_map<const char*, IBuffer*> m_bufferResourceMap;
		std::vector<const char*> m_bufferInputs;

		std::unordered_map<const char*, IRenderImage*> m_imageResourceMap;
		std::vector<const char*> m_imageInputs;
	};
}