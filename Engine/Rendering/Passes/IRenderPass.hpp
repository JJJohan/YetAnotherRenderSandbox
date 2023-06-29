#pragma once

#include <vector>
#include "Core/Macros.hpp"

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
			, m_name("")
			, m_frameTime(0.0f)
		{
		}

		virtual ~IRenderPass() = default;
		virtual void Draw(const IDevice& device, const ICommandBuffer& commandBuffer) const = 0;
		EXPORT inline const char* GetName() const { return m_name; }

		EXPORT inline float GetFrameTime() const { return m_frameTime; }
		inline void SetFrameTime(float time) { m_frameTime = time; }

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
		const char* m_name;
		float m_frameTime;
	};
}