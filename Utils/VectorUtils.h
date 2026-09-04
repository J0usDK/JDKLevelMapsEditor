#pragma once
#include <vector>

namespace JDKLevelMaps::Utils::Common
{
	template<typename T>
	inline bool TryResize(std::vector<T>& vector, size_t size) noexcept
	{
		try { vector.resize(size); }
		catch (...) { return false; }
		return true;
	}

	template<typename Container>
	inline bool TryReserve(Container& container, size_t size) noexcept
	{
		try { container.reserve(size); }
		catch (...) { return false; }
		return true;
	}

	template<typename T, typename U>
	inline bool TryAssign(std::vector<T>& vector, size_t size, const U& value) noexcept
	{
		try { vector.assign(size, value); }
		catch (...) { return false; }
		return true;
	}
}