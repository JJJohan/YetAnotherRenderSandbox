#pragma once

#include <vulkan/vulkan.hpp>
#include "Buffer.hpp"
#include "DescriptorPool.hpp"

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
		RenderMesh(const PipelineLayout* pipelineLayout, const uint32_t maxConcurrentFrames);
		bool Initialise(const PhysicalDevice& physicalDevice, const Device& device,
			const CommandPool& resourceCommandPool, const Mesh& mesh);

		void Draw(const vk::CommandBuffer& commandBuffer, const vk::Extent2D& viewSize, uint32_t currentFrameIndex) const;

	private:
		std::unique_ptr<Buffer> SetupPositionBuffer(const PhysicalDevice& physicalDevice, const Device& device, const Mesh& mesh);
		std::unique_ptr<Buffer> SetupColourBuffer(const PhysicalDevice& physicalDevice, const Device& device, const Mesh& mesh);
		std::unique_ptr<Buffer> SetupIndexBuffer(const PhysicalDevice& physicalDevice, const Device& device, const Mesh& mesh);
		bool SetupUniformBuffers(const PhysicalDevice& physicalDevice, const Device& device, const Mesh& mesh);

		uint32_t m_vertexCount;
		uint32_t m_indexCount;
		uint32_t m_maxConcurrentFrames;

		std::unique_ptr<Buffer> m_positionBuffer;
		std::unique_ptr<Buffer> m_colourBuffer;
		std::unique_ptr<Buffer> m_indexBuffer;

		std::vector<std::unique_ptr<Buffer>> m_uniformBuffers;
		std::vector<void*> m_uniformBuffersMapped;

		std::unique_ptr<DescriptorPool> m_descriptorPool;
		std::vector<vk::DescriptorSet> m_descriptorSets;

		const PipelineLayout* m_pipelineLayout;
	};
}