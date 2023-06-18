#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>
#include "Types.hpp"

namespace Engine::Rendering
{
	class IImageView;
	class IImageSampler;
	class IBuffer;
	class ICommandBuffer;

	class Material
	{
	public:
		Material();
		virtual ~Material() = default;

		bool Parse(const std::filesystem::path& path);

		inline std::string_view GetName() const { return m_name; };
		inline const std::unordered_map<ShaderStageFlags, std::vector<uint8_t>>& GetProgramData() const { return m_programData; }
		inline const std::vector<Format>& GetAttachmentFormats() const { return m_attachmentFormats; }
		inline bool DepthWrite() const { return m_depthWrite; }
		inline bool DepthTest() const { return m_depthTest; }

		inline bool BindImageView(uint32_t binding, const std::unique_ptr<IImageView>& imageView)
		{
			return BindImageViewsImp(binding, { imageView.get() });
		}

		inline bool BindSampler(uint32_t binding, const std::unique_ptr<IImageSampler>& sampler)
		{
			return BindSamplersImp(binding, { sampler.get() });
		}

		inline bool BindStorageBuffer(uint32_t binding, const std::unique_ptr<IBuffer>& storageBuffer)
		{
			return BindStorageBuffersImp(binding, { storageBuffer.get() });
		}

		inline bool BindImageView(uint32_t binding, const IImageView& imageView)
		{
			return BindImageViewsImp(binding, { &imageView });
		}

		inline bool BindSampler(uint32_t binding, const IImageSampler& sampler)
		{
			return BindSamplersImp(binding, { &sampler });
		}

		inline bool BindStorageBuffer(uint32_t binding, const IBuffer& storageBuffer)
		{
			return BindStorageBuffersImp(binding, { &storageBuffer });
		}

		inline bool BindImageViews(uint32_t binding, const std::vector<std::unique_ptr<IImageView>>& imageViews)
		{
			std::vector<const IImageView*> imageViewPtrs(imageViews.size());
			for (size_t i = 0; i < imageViews.size(); ++i)
				imageViewPtrs[i] = imageViews[i].get();
			return BindImageViewsImp(binding, imageViewPtrs);
		}

		inline bool BindSamplers(uint32_t binding, const std::vector<std::unique_ptr<IImageSampler>>& samplers)
		{
			std::vector<const IImageSampler*> samplerPtrs(samplers.size());
			for (size_t i = 0; i < samplers.size(); ++i)
				samplerPtrs[i] = samplers[i].get();
			return BindSamplersImp(binding, samplerPtrs);
		}

		inline bool BindStorageBuffers(uint32_t binding, const std::vector<std::unique_ptr<IBuffer>>& storageBuffers)
		{
			std::vector<const IBuffer*> storageBufferPtrs(storageBuffers.size());
			for (size_t i = 0; i < storageBuffers.size(); ++i)
				storageBufferPtrs[i] = storageBuffers[i].get();
			return BindStorageBuffersImp(binding, storageBufferPtrs);
		}

		inline bool BindUniformBuffers(uint32_t binding, const std::vector<std::unique_ptr<IBuffer>>& uniformBuffers)
		{
			std::vector<const IBuffer*> uniformBufferPtrs(uniformBuffers.size());
			for (size_t i = 0; i < uniformBuffers.size(); ++i)
				uniformBufferPtrs[i] = uniformBuffers[i].get();
			return BindUniformBuffersImp(binding, uniformBufferPtrs);
		}

		virtual bool SetSpecialisationConstant(std::string name, int32_t value) = 0;
		virtual	void BindMaterial(const ICommandBuffer& commandBuffer, uint32_t frameIndex) const = 0;

	protected:
		virtual bool BindImageViewsImp(uint32_t binding, const std::vector<const IImageView*>& imageViews) = 0;
		virtual bool BindSamplersImp(uint32_t binding, const std::vector<const IImageSampler*>& samplers) = 0;
		virtual bool BindStorageBuffersImp(uint32_t binding, const std::vector<const IBuffer*>& storageBuffers) = 0;
		virtual bool BindUniformBuffersImp(uint32_t binding, const std::vector<const IBuffer*>& uniformBuffers) = 0;

	private:
		std::string m_name;
		std::unordered_map<ShaderStageFlags, std::vector<uint8_t>> m_programData;
		std::vector<Format> m_attachmentFormats;
		bool m_depthWrite;
		bool m_depthTest;
	};
}