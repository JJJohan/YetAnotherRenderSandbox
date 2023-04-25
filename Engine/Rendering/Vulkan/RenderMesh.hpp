#pragma once

#include <vulkan/vulkan.hpp>
#include "Buffer.hpp"

namespace Engine::Rendering
{
	class Mesh;
}

namespace Engine::Rendering::Vulkan
{
	class Device;
	class PhysicalDevice;
	class PipelineLayout;
	class CommandPool;
	class CommandBuffer;

	class RenderMesh
	{
	public:
		RenderMesh(const PipelineLayout* pipelineLayout);
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device,
			const CommandPool& resourceCommandPool, const Mesh& mesh);

		void Draw(const vk::CommandBuffer& commandBuffer) const;

	private:
		uint32_t m_vertexCount;
		uint32_t m_indexCount;
		std::unique_ptr<Buffer> m_positionBuffer;
		std::unique_ptr<Buffer> m_colourBuffer;
		std::unique_ptr<Buffer> m_indexBuffer;
		const PipelineLayout* m_pipelineLayout;
	};
}