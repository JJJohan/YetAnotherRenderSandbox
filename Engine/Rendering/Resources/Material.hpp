#pragma once

#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>
#include "../Types.hpp"
#include "IRenderImage.hpp"
#include "AttachmentInfo.hpp"
#include "Core/Logging/Logger.hpp"

namespace Engine::Rendering
{
	class IImageView;
	class IImageSampler;
	class IBuffer;
	class ICommandBuffer;

	enum class BindPoint
	{
		Graphics,
		Compute
	};

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

		inline AttachmentInfo GetColourAttachmentInfo(uint32_t attachmentIndex, IRenderImage* image, 
			AttachmentLoadOp loadOp = AttachmentLoadOp::DontCare, AttachmentStoreOp storeOp = AttachmentStoreOp::Store,
			ClearValue clearValue = { Colour(0,0,0,0) })
		{
			if (m_attachmentFormats.size() <= attachmentIndex)
			{
				Engine::Logging::Logger::Error("Attachment index {} exceeds attachment count of {}.", attachmentIndex, m_attachmentFormats.size());
			}
			
			if (m_attachmentFormats[attachmentIndex] != Format::PlaceholderSwapchain && m_attachmentFormats[attachmentIndex] != image->GetFormat())
			{
				Engine::Logging::Logger::Error("Attachment format mismatch for provided image.");
			}

			return AttachmentInfo(image, ImageLayout::ColorAttachment, loadOp, storeOp, clearValue);
		}

		inline bool BindImageView(uint32_t binding, const std::unique_ptr<IImageView>& imageView)
		{
			return BindImageViewsImp(binding, { imageView.get() });
		}

		inline bool BindImageView(uint32_t binding, const IImageView* imageView)
		{
			return BindImageViewsImp(binding, { imageView });
		}

		inline bool BindImageView(uint32_t binding, const IImageView& imageView)
		{
			return BindImageViewsImp(binding, { &imageView });
		}

		inline bool BindSampler(uint32_t binding, const std::unique_ptr<IImageSampler>& sampler)
		{
			return BindSamplersImp(binding, { sampler.get() });
		}

		inline bool BindSampler(uint32_t binding, const IImageSampler& sampler)
		{
			return BindSamplersImp(binding, { &sampler });
		}

		inline bool BindSampler(uint32_t binding, const IImageSampler* sampler)
		{
			return BindSamplersImp(binding, { sampler });
		}

		inline bool BindCombinedImageSampler(uint32_t binding, const std::unique_ptr<IImageSampler>& sampler, const std::unique_ptr<IImageView>& imageView, ImageLayout imageLayout)
		{
			return BindCombinedImageSamplersImp(binding, { sampler.get() }, { imageView.get() }, { imageLayout });
		}

		inline bool BindCombinedImageSampler(uint32_t binding, const IImageSampler& sampler, const IImageView& imageView, ImageLayout imageLayout)
		{
			return BindCombinedImageSamplersImp(binding, { &sampler }, { &imageView }, { imageLayout });
		}

		inline bool BindCombinedImageSampler(uint32_t binding, const IImageSampler* sampler, const IImageView* imageView, ImageLayout imageLayout)
		{
			return BindCombinedImageSamplersImp(binding, { sampler }, { imageView }, { imageLayout });
		}

		inline bool BindStorageBuffer(uint32_t binding, const std::unique_ptr<IBuffer>& storageBuffer)
		{
			return BindStorageBuffersImp(binding, { storageBuffer.get() });
		}

		inline bool BindStorageBuffer(uint32_t binding, const IBuffer* storageBuffer)
		{
			return BindStorageBuffersImp(binding, { storageBuffer });
		}

		inline bool BindStorageBuffer(uint32_t binding, const IBuffer& storageBuffer)
		{
			return BindStorageBuffersImp(binding, { &storageBuffer });
		}

		inline bool BindStorageImage(uint32_t binding, const std::unique_ptr<IImageView>& imageView)
		{
			return BindStorageImagesImp(binding, { imageView.get() });
		}

		inline bool BindStorageImage(uint32_t binding, const IImageView* imageView)
		{
			return BindStorageImagesImp(binding, { imageView });
		}

		inline bool BindStorageImage(uint32_t binding, const IImageView& imageView)
		{
			return BindStorageImagesImp(binding, { &imageView });
		}

		inline bool BindImageViews(uint32_t binding, const std::vector<std::unique_ptr<IImageView>>& imageViews)
		{
			std::vector<const IImageView*> imageViewPtrs(imageViews.size());
			for (size_t i = 0; i < imageViews.size(); ++i)
				imageViewPtrs[i] = imageViews[i].get();
			return BindImageViewsImp(binding, imageViewPtrs);
		}

		inline bool BindImageViews(uint32_t binding, const std::vector<const IImageView*>& imageViews)
		{
			return BindImageViewsImp(binding, imageViews);
		}

		inline bool BindSamplers(uint32_t binding, const std::vector<std::unique_ptr<IImageSampler>>& samplers)
		{
			std::vector<const IImageSampler*> samplerPtrs(samplers.size());
			for (size_t i = 0; i < samplers.size(); ++i)
				samplerPtrs[i] = samplers[i].get();
			return BindSamplersImp(binding, samplerPtrs);
		}

		inline bool BindCombinedImageSamplers(uint32_t binding, const std::vector<const IImageSampler*>& samplers,
			const std::vector<const IImageView*>& imageViews, const std::vector<ImageLayout>& imageLayouts)
		{
			if (samplers.size() != imageViews.size() || samplers.size() != imageLayouts.size())
			{
				Engine::Logging::Logger::Error("Sampler, ImageView and imageLayouts arrays should have identical counts.");
				return false;
			}

			return BindCombinedImageSamplersImp(binding, samplers, imageViews, imageLayouts);
		}

		inline bool BindStorageBuffers(uint32_t binding, const std::vector<std::unique_ptr<IBuffer>>& storageBuffers)
		{
			std::vector<const IBuffer*> storageBufferPtrs(storageBuffers.size());
			for (size_t i = 0; i < storageBuffers.size(); ++i)
				storageBufferPtrs[i] = storageBuffers[i].get();
			return BindStorageBuffersImp(binding, storageBufferPtrs);
		}	
		
		inline bool BindStorageImages(uint32_t binding, const std::vector<const IImageView*>& imageViews)
		{
			return BindStorageImagesImp(binding, imageViews);
		}

		inline bool BindUniformBuffers(uint32_t binding, const std::vector<std::unique_ptr<IBuffer>>& uniformBuffers)
		{
			std::vector<const IBuffer*> uniformBufferPtrs(uniformBuffers.size());
			for (size_t i = 0; i < uniformBuffers.size(); ++i)
				uniformBufferPtrs[i] = uniformBuffers[i].get();
			return BindUniformBuffersImp(binding, uniformBufferPtrs);
		}

		virtual bool SetSpecialisationConstant(std::string name, int32_t value) = 0;
		virtual	bool BindMaterial(const ICommandBuffer& commandBuffer, BindPoint bindPoint, uint32_t frameIndex) const = 0;

	protected:
		virtual bool BindImageViewsImp(uint32_t binding, const std::vector<const IImageView*>& imageViews) = 0;
		virtual bool BindSamplersImp(uint32_t binding, const std::vector<const IImageSampler*>& samplers) = 0;
		virtual bool BindCombinedImageSamplersImp(uint32_t binding, const std::vector<const IImageSampler*>& samplers, 
			const std::vector<const IImageView*>& imageViews, const std::vector<ImageLayout>& imageLayouts) = 0;
		virtual bool BindStorageBuffersImp(uint32_t binding, const std::vector<const IBuffer*>& storageBuffers) = 0;
		virtual bool BindStorageImagesImp(uint32_t binding, const std::vector<const IImageView*>& imageViews) = 0;
		virtual bool BindUniformBuffersImp(uint32_t binding, const std::vector<const IBuffer*>& uniformBuffers) = 0;

	private:
		std::string m_name;
		std::unordered_map<ShaderStageFlags, std::vector<uint8_t>> m_programData;
		std::vector<Format> m_attachmentFormats;
		bool m_depthWrite;
		bool m_depthTest;
	};
}