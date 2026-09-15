#pragma once
#include <CrySystem/File/ICryPak.h>

#include "Core/Data/RunResult.h"

namespace JDKLevelMaps::Utils::FileSystem
{
	inline constexpr size_t kMaxChunkSize = 1024 * 1024;

	template<typename TCallback>
	Data::SRunResult WriteDataChunked(FILE* pFile, const void* pData, size_t elementSize, size_t totalElements, TCallback callback)
	{
		if (totalElements == 0)
			return { true, "" };

		const size_t elementsPerChunk = std::max<size_t>(1, kMaxChunkSize / elementSize);
		const uint8* pCurrentData = static_cast<const uint8*>(pData);
		size_t elementsWritten = 0;

		while (elementsWritten < totalElements)
		{
			size_t elementsToWrite = std::min(elementsPerChunk, totalElements - elementsWritten);

			if (gEnv->pCryPak->FWrite(pCurrentData, elementSize, elementsToWrite, pFile) != elementsToWrite)
				return { false, "Disk I/O Error: Failed to write data chunk" };

			pCurrentData += elementsToWrite * elementSize;
			elementsWritten += elementsToWrite;

			if (!callback(elementsWritten))
				return { false, "Operation cancelled by user" };
		}
		return { true, "" };
	}

	template<typename TCallback>
	Data::SRunResult ReadDataChunked(FILE* pFile, void* pData, size_t elementSize, size_t totalElements, TCallback callback)
	{
		if (totalElements == 0)
			return { true, "" };

		const size_t elementsPerChunk = std::max<size_t>(1, kMaxChunkSize / elementSize);
		uint8* pCurrentData = static_cast<uint8*>(pData);
		size_t elementsRead = 0;

		while (elementsRead < totalElements)
		{
			size_t elementsToRead = std::min(elementsPerChunk, totalElements - elementsRead);

			if (gEnv->pCryPak->FReadRaw(pCurrentData, elementSize, elementsToRead, pFile) != elementsToRead)
				return { false, "Disk I/O Error: Failed to read data chunk" };

			pCurrentData += elementsToRead * elementSize;
			elementsRead += elementsToRead;

			if (!callback(elementsRead))
				return { false, "Operation cancelled by user" };
		}

		return { true, "" };
	}
}