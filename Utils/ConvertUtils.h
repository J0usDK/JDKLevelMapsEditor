#pragma once
#include <CryCore/BaseTypes.h>
#include <QVariant>

#include "Logger.h"

namespace JDKLevelMaps::Utils::ConvertUtils
{
	[[nodiscard]] inline uint8 QVariantToUint8(const QVariant& value, const uint8 defaultValue = 0)
	{
		if (!value.isValid())
			return defaultValue;

		bool bIsOk = false;
		uint tempValue = value.toUInt(&bIsOk);

		if (bIsOk && tempValue <= UINT8_MAX)
			return static_cast<uint8>(tempValue);

		JDK_WARN("Cannot convert value to uint8");
		return defaultValue;
	}

	[[nodiscard]] inline uint32 QVariantToUint32(const QVariant& value, const uint32 defaultValue = 0)
	{
		if (!value.isValid())
			return defaultValue;

		bool bIsOk = false;
		uint32 result = value.toUInt(&bIsOk);

		if (bIsOk)
			return result;

		JDK_WARN("Cannot convert value to uint32");
		return defaultValue;
	}

	[[nodiscard]] inline float QVariantToFloat(const QVariant& value, const float defaultValue = 0.0f)
	{
		if (!value.isValid())
			return defaultValue;

		bool bIsOk = false;
		const float result = value.toFloat(&bIsOk);

		if (bIsOk && std::isfinite(result))
			return result;

		JDK_WARN("Cannot convert value to float");
		return defaultValue;
	}

	[[nodiscard]] inline std::string QVariantToStdString(const QVariant& value, const std::string& defaultValue = "")
	{
		if (value.isValid())
			return value.toString().toStdString();

		JDK_WARN("Cannot convert value to std::string");
		return defaultValue;
	}

	[[nodiscard]] inline bool QVariantToBool(const QVariant& value, const bool bDefaultValue = false)
	{
		if (value.isValid() && value.canConvert<bool>())
			return value.toBool();

		JDK_WARN("Cannot convert value to bool");
		return bDefaultValue;
	}
}