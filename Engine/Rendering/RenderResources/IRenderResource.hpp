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

		virtual bool Build(const Renderer& renderer) = 0;

		inline const char* GetName() const { return m_name; }

		inline const std::vector<const char*>& GetBufferOutputs() const { return m_bufferOutputs; }

		inline const std::vector<const char*>& GetImageOutputs() const { return m_imageOutputs; }

		IRenderImage* GetImageResource(const char* name) const
		{
			return m_imageResources.at(name);
		}

		IBuffer* GetBufferResource(const char* name) const
		{
			return m_bufferResources.at(name);
		}

	protected:
		IRenderResource(const char* name)
			: m_bufferOutputs()
			, m_imageOutputs()
			, m_name(name)
			, m_bufferResources()
			, m_imageResources()
		{
		}

		std::vector<const char*> m_bufferOutputs;
		std::vector<const char*> m_imageOutputs;
		std::unordered_map<const char*, IBuffer*> m_bufferResources;
		std::unordered_map<const char*, IRenderImage*> m_imageResources;

	private:
		const char* m_name;
	};
}