#pragma once
#include <string>

namespace JDKLevelMaps::Data
{
	struct [[nodiscard]] SRunResult
	{
		bool bSuccess = false;
		std::string message;

		SRunResult() = default;
		SRunResult(bool bSuccess, std::string msg) : bSuccess(bSuccess), message(std::move(msg)) {}
	};
}