#pragma once
#include <CryCore/BaseTypes.h>
#include <CrySystem/File/ICryPak.h>

#if defined(__has_include)
	#if __has_include(<CrySystem/File/JDKPakLFS.h>)
		#include <CrySystem/File/JDKPakLFS.h>
	#endif
#endif

namespace JDKLevelMaps::FileSystem::LFSFacade
{
	[[nodiscard]] inline constexpr uint64 GetMaxFileSize() noexcept
	{
#if defined(JDK_CRYPAK_LFS_PATCH)
		return JDK_MAX_PAK_FILE_SIZE;
#else
		return 2147483648ull;
#endif
	}

	[[nodiscard]] inline constexpr bool IsPatched() noexcept
	{
#if defined(JDK_CRYPAK_LFS_PATCH)
		return true;
#else
		return false;
#endif
	}

	[[nodiscard]] inline FILE* FOpen(const char* pName, const char* szMode, bool bSparse = false, unsigned int nFlags = 0U) noexcept
	{
		if (!bSparse)
			return gEnv->pCryPak->FOpen(pName, szMode, nFlags);

#if defined(JDK_CRYPAK_LFS_PATCH)
		char smode[17];

		size_t i = 0;
		for (; szMode[i] != '\0'; ++i)
		{
			CRY_ASSERT(i + 2 < sizeof(smode));
			smode[i] = szMode[i];
		}
		smode[i++] = 's';
		smode[i] = '\0';

		return gEnv->pCryPak->FOpen(pName, smode, nFlags);
#else
		CRY_ASSERT(false, "Sparse file requested, but sparse-file support is unavailable");
		return nullptr;
#endif
	}

	inline uint64 FTell(FILE* pFile) noexcept
	{
		CRY_ASSERT(pFile, "[JDKLevelMaps] Null file pointer passed to LFSFacade::FTell");

#if defined(JDK_CRYPAK_LFS_PATCH)
		return static_cast<uint64>(gEnv->pCryPak->FTell64(pFile));
#else
		return static_cast<uint64>(gEnv->pCryPak->FTell(pFile));
#endif
	}

	inline size_t FSeek(FILE* pFile, uint64 offset, int mode) noexcept
	{
		CRY_ASSERT(pFile, "[JDKLevelMaps] Null file pointer passed to LFSFacade::FSeek");

#if defined(JDK_CRYPAK_LFS_PATCH)
		return gEnv->pCryPak->FSeek64(pFile, static_cast<int64>(offset), mode);
#else
		CRY_ASSERT(offset <= GetMaxFileSize(), "[JDKLevelMaps] FSeek offset exceeds 2GB limit of unpatched CryPak");
		return gEnv->pCryPak->FSeek(pFile, static_cast<long>(offset), mode);
#endif
	}
}