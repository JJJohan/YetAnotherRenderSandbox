#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string_view>

namespace Engine::Rendering
{
	enum class ProgramType
	{
		Vertex,
		Fragment
	};

	enum class AttachmentFormat
	{
		R8G8Unorm,
		R8G8B8A8Unorm,
		R16G16Sfloat,
		R16G16B16A16Sfloat,
		R32G32B32A32Sfloat,
		Swapchain
	};

	class Material
	{
	public:
		Material();
		bool Parse(const std::filesystem::path& path);

		inline std::string_view GetName() const { return m_name; };
		inline const std::unordered_map<ProgramType, std::vector<uint8_t>>& GetProgramData() const { return m_programData; }
		inline const std::vector<AttachmentFormat>& GetAttachmentFormats() const { return m_attachmentFormats; }
		inline bool DepthWrite() const { return m_depthWrite; }
		inline bool DepthTest() const { return m_depthTest; }

	private:
		std::string m_name;
		std::unordered_map<ProgramType, std::vector<uint8_t>> m_programData;
		std::vector<AttachmentFormat> m_attachmentFormats;
		bool m_depthWrite;
		bool m_depthTest;
	};
}