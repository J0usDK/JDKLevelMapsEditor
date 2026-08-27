#pragma once
#include <CryCore/BaseTypes.h>

namespace JDKLevelMaps::ImageWork
{
	struct SImageView;
	using ImageResizePtr = bool(*)(uint32 width, uint32 height, SImageView& outImage, void* context);

	struct SImageView
	{
	public:
		SImageView() = delete;
		SImageView(uint32 width, uint32 height, uint32 stride, uint32 channelsCount, uint8* pData, ImageResizePtr resizePtr, void* context)
			: width(width), height(height), stride(stride), channelsCount(channelsCount), pData(pData), imageResizer(resizePtr), context(context) {}

		[[nodiscard]] uint8* ScanLine(uint32 y) const noexcept
		{
			return pData + y * stride;
		}

		[[nodiscard]] bool IsValid() const noexcept
		{
			return pData != nullptr && width != 0 && height != 0;
		}

		[[nodiscard]] bool Resize(uint32 width, uint32 height)
		{
			return imageResizer ? imageResizer(width, height, *this, context) : false;
		}

	public:
		uint8* pData = nullptr;
		uint32 width = 0;
		uint32 height = 0;
		uint32 stride = 0;
		uint32 channelsCount = 3;

	private:
		ImageResizePtr imageResizer = nullptr;
		void* context = nullptr;
	};
}