#pragma once
#include <CrySystem/File/ICryPak.h>

namespace JDKLevelMaps::Utils::FileSystem
{
	struct ScopedCryFile
	{
	public:
		explicit ScopedCryFile(FILE* pFile) noexcept : pFile(pFile) { }
		ScopedCryFile(FILE* pFile, const CryPathString& path) noexcept : pFile(pFile), bSuccessFlag(false), filePath(path) { }
		~ScopedCryFile() noexcept { close(); }

		ScopedCryFile(const ScopedCryFile&) = delete;
		ScopedCryFile& operator=(const ScopedCryFile&) = delete;
		ScopedCryFile(ScopedCryFile&&) = delete;
		ScopedCryFile& operator=(ScopedCryFile&&) = delete;

		operator FILE* () const noexcept { return pFile; }

		void close(bool bSuccess) noexcept
		{
			bSuccessFlag = bSuccess;
			close();
		}

		void close() noexcept
		{
			if (!pFile)
				return;

			gEnv->pCryPak->FClose(pFile);
			pFile = nullptr;

			if (!bSuccessFlag && !filePath.empty())
				gEnv->pCryPak->RemoveFile(filePath);
		}

	private:
		FILE* pFile = nullptr;
		CryPathString filePath;
		bool bSuccessFlag = true;
	};
}