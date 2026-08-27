#pragma once
#include <QImage>

#include <CryCore/BaseTypes.h>

#include "Utils/ImageView.h"

namespace JDKLevelMaps::Utils::Image
{
	inline static bool ResizeImage(uint32 width, uint32 height, ImageWork::SImageView& imageView, void* context)
	{
		auto& image = *static_cast<QImage*>(context);
		image = QImage(width, height, QImage::Format_RGB888);
		if (image.isNull())
			return false;
		image.fill(Qt::black);

		imageView.pData = image.bits();
		imageView.width = static_cast<uint32>(image.width());
		imageView.height = static_cast<uint32>(image.height());
		imageView.stride = static_cast<uint32>(image.bytesPerLine());
		imageView.channelsCount = 3;

		return true;
	}
}