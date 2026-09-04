#pragma once
#include <string>
#include <optional>
#include <CryString/CryString.h>

namespace JDKLevelMaps::FileSystem
{
	class CPathResolver
	{
	public:
		CPathResolver();

		void RecomputePath();

		[[nodiscard]] std::optional<std::string> GetImagePath(const char* bakerId) const;
		[[nodiscard]] std::optional<std::string> GetMapPath(const char* bakerId) const;

	private:
		std::string m_defaultPath;
		bool m_bInitialized = false;

		static constexpr const char* kMapExtension = ".jdkm";
		static constexpr const char* kImageExtension = ".png";
	};
}