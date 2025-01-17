#pragma once

#include "Core/Macros.hpp"
#include <vector>
#include <unordered_map>
#include <string>

namespace Engine::Rendering
{
	struct FrameStats
	{
		uint64_t InputAssemblyVertexCount;
		uint64_t InputAssemblyPrimitivesCount;
		uint64_t VertexShaderInvocations;
		uint64_t FragmentShaderInvocations;
		uint64_t RenderBegin;
		uint64_t RenderEnd;
		float RenderTime;
	};

	struct MemoryStats
	{
		std::unordered_map<std::string, size_t> ResourceMemoryUsage;
		uint64_t DedicatedUsage;
		uint64_t DedicatedBudget;
		uint64_t SharedUsage;
		uint64_t SharedBudget;
	};

	class IPhysicalDevice;
	class IDevice;
	class ICommandBuffer;
	class IRenderResource;

	class RenderStats
	{
	public:
		RenderStats();
		virtual ~RenderStats() = default;

		virtual bool Initialise(const IPhysicalDevice& physicalDevice, const IDevice& device, uint32_t renderPassCount) = 0;
		virtual void Begin(const ICommandBuffer& commandBuffer, std::string_view passName, bool isCompute) = 0;
		virtual void End(const ICommandBuffer& commandBuffer, bool isCompute) = 0;
		virtual void FinaliseResults(const IPhysicalDevice& physicalDevice, const IDevice& device,
			const std::vector<IRenderResource*>& renderResources) = 0;

		EXPORT const std::unordered_map<std::string, FrameStats>& GetFrameStats() const;
		EXPORT const MemoryStats& GetMemoryStats() const;

	protected:
		std::unordered_map<std::string, FrameStats> m_statsData;
		MemoryStats m_memoryStats;
	};
}