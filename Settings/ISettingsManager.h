#pragma once
#include <QVariant>

namespace JDKLevelMaps::Settings
{
	class ISettingsManager
	{
	public:
		virtual ~ISettingsManager() = default;

		virtual void SetPluginProperty(const char* key, const QVariant& value) = 0;
		virtual QVariant GetPluginProperty(const char* key) = 0;
	};
}