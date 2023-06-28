#pragma once

#include <vector>

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
			: m_bufferOutputs()
			, m_bufferInputs()
			, m_imageOutputs()
			, m_imageInputs()
		{
		}

		virtual ~IRenderPass() = default;
		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const = 0;

		inline const std::vector<const char*>& GetBufferInputs() const { return m_bufferInputs; }

		inline const std::vector<const char*>& GetBufferOutputs() const { return m_bufferOutputs; }

		inline const std::vector<const char*>& GetImageInputs()const { return m_imageInputs; }

		inline const std::vector<const char*>& GetImageOutputs() const { return m_imageOutputs; }

		virtual const IRenderImage& GetImageResource(const char* name) const = 0;

		virtual const IBuffer& GetBufferResource(const char* name) const = 0;

	protected:
		std::vector<const char*> m_bufferOutputs;
		std::vector<const char*> m_bufferInputs;

		std::vector<const char*> m_imageOutputs;
		std::vector<const char*> m_imageInputs;
	};
}