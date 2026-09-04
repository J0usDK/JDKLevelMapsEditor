#pragma once
#include <CrySystem/File/ICryPak.h>

namespace JDKLevelMaps::Utils::FileSystem
{
	inline constexpr size_t kMaxChunkSize = 1024 * 1024;

	template<typename TCallback>
	bool WriteDataChunked(FILE* pFile, const void* pData, size_t elementSize, size_t totalElements, TCallback callback)
	{
		if (totalElements == 0)
			return true;

		const size_t elementsPerChunk = std::max<size_t>(1, kMaxChunkSize / elementSize);
		const uint8* pCurrentData = static_cast<const uint8*>(pData);
		size_t elementsWritten = 0;

		while (elementsWritten < totalElements)
		{
			size_t elementsToWrite = std::min(elementsPerChunk, totalElements - elementsWritten);

			if (gEnv->pCryPak->FWrite(pCurrentData, elementSize, elementsToWrite, pFile) != elementsToWrite)
				return false;

			pCurrentData += elementsToWrite * elementSize;
			elementsWritten += elementsToWrite;

			if (!callback(elementsWritten))
				return false;
		}
		return true;
	}
}