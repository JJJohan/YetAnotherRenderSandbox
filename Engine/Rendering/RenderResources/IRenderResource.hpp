#pragma once

#include <vector>
#include <unordered_map>

namespace Engine::Rendering
{
	class IBuffer;
	class IRenderImage;
	class Renderer;

	class IRenderResource
	{
	public:
		virtual ~IRenderResource() = default;

		virtual bool Build(const Renderer& renderer, const std::unordered_map<const char*, IRenderImage*>& imageInputs,
			const std::unordered_map<const char*, IBuffer*>& bufferInputs) = 0;

		inline const char* GetName() const { return m_name; }

		inline const std::unordered_map<const char*, IBuffer*>& GetBufferOutputs() const { return m_bufferOutputs; }

		inline const std::unordered_map<const char*, IRenderImage*>& GetImageOutputs() const { return m_imageOutputs; }

		inline IBuffer* GetOutputBuffer(const char* name) const { return m_bufferOutputs.at(name); }

		inline IRenderImage* GetOutputImage(const char* name) const { return m_imageOutputs.at(name); }

		inline virtual size_t GetMemoryUsage() const { return 0; }

	protected:
		IRenderResource(const char* name)
			: m_bufferOutputs()
			, m_imageOutputs()
			, m_name(name)
		{
		}

		std::unordered_map<const char*, IBuffer*> m_bufferOutputs;
		std::unordered_map<const char*, IRenderImage*> m_imageOutputs;

	private:
		const char* m_name;
	};
}