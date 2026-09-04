#include "StdAfx.h"
#include "ImageExporter.h"

#include <utility>

#include "Core/Data/LevelContext.h"
#include "Core/Data/RunResult.h"
#include "Core/Bakers/IMapBaker.h"
#include "Core/ImageWork/Export/PNGStreamWriter.h" 
#include "Core/FileSystem/PathResolver.h"
#include "Core/FileSystem/LFSFacade.h"
#include "Utils/Progress.h"
#include "Utils/ScopedCryFile.h"
#include "Utils/Defines.h"

namespace JDKLevelMaps::ImageWork
{
	class CImageDataSource final : public IPNGDataSource
	{
	public:
		CImageDataSource() = delete;
		explicit CImageDataSource(const std::vector<uint8>& data, uint32 width, uint32 channelsCount, uint8 channelsMask, Bakers::DebugColorMapperPtr mapper, Utils::Common::SProgressTask* pTask) noexcept
			: m_data(data), m_width(width), m_channelsCount(channelsCount), m_channelsMask(channelsMask), m_colorMapper(mapper), m_pTask(pTask) { }

		bool FetchRowRGB(uint32 y, uint8* JDK_RESTRICT pOutRowRgb) noexcept override
		{
			const size_t rowOffset = static_cast<size_t>(y) * m_width * m_channelsCount;
			const uint8* pRowData = m_data.data() + rowOffset;

			const uint8* pPixel = pRowData;
			uint8* pOut = pOutRowRgb;

			for (uint32 x = 0; x < m_width; ++x)
			{
				const Bakers::SDebugColor color = m_colorMapper(m_channelsMask, pPixel);

				pOut[0] = color.r;
				pOut[1] = color.g;
				pOut[2] = color.b;

				pPixel += m_channelsCount;
				pOut += 3;
			}
			return true;
		}

		bool OnProgress(uint32 processedLine) noexcept override
		{
			if (m_pTask)
				return m_pTask->Update(processedLine);
			return true;
		}

	private:
		const std::vector<uint8>& m_data;

		const uint32 m_width;
		const uint32 m_channelsCount;
		const uint8 m_channelsMask;

		const Bakers::DebugColorMapperPtr m_colorMapper;
		Utils::Common::SProgressTask* m_pTask;
	};

	Data::SRunResult CImageExporter::Prepare(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, FileSystem::CPathResolver& pathResolver, Utils::Common::CProgressor* pProgressor)
	{
		if (auto imagePath = pathResolver.GetImagePath(baker.GetID()))
			m_imagePath = std::move(*imagePath);
		else
			return { false, "Disk I/O Error: Cannot get image's path" };

		if (pProgressor)
			m_pImageTask = pProgressor->RegisterProgressTask(context.gridHeight, 1);

		m_bReady = true;
		return { true, "" };
	}

	Data::SRunResult CImageExporter::ExportImage(const Bakers::IMapBaker& baker, const Data::SLevelContext& context, const std::vector<uint8>& data)
	{
		if (!std::exchange(m_bReady, false))
			return { false, "Exporter is not ready" };

		Utils::FileSystem::ScopedCryFile file(FileSystem::LFSFacade::FOpen(m_imagePath.c_str(), "wb"), m_imagePath.c_str());
		if (!file)
			return { false, "Disk I/O Error: Cannot open debug image file for writing" };

		CImageDataSource dataSource(data, context.gridWidth, baker.GetChannelCount(), baker.GetActiveLayersMask(), baker.GetDebugColorMapper(), m_pImageTask);

		if (auto result = ImageWork::WritePNG(file, context.gridWidth, context.gridHeight, dataSource); !result.bSuccess)
			return result;

		file.close(true);
		return { true, "" };
	}
}