#pragma once
#include "IPlugin.h"

#if defined(JDKLevelMapsPlugin_EXPORTS)
	#define JDKLevelMaps_PLUGIN_API DLL_EXPORT
#else
	#define JDKLevelMaps_PLUGIN_API DLL_IMPORT
#endif

class JDKLevelMaps_PLUGIN_API CJDKLevelMapsPlugin : public IPlugin
{
public:
	CJDKLevelMapsPlugin() { /* entry point of the plugin, perform initializations */ }
	~CJDKLevelMapsPlugin() { /* exit point of the plugin, perform cleanup */ }

	int32       GetPluginVersion() noexcept override { return 1; }
	const char* GetPluginName() noexcept override { return "JDKLevelMaps"; }
	const char* GetPluginDescription() noexcept override { return "Editor extension for generating optimized runtime data maps from level information"; }
};