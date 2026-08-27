#pragma once
#include <CrySystem/ISystem.h>

#define JDK_LOG(...) CryLogAlways("[JDKLevelMaps] " __VA_ARGS__)
#define JDK_WARN(...) CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[JDKLevelMaps] " __VA_ARGS__)
#define JDK_ERR(...) CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[JDKLevelMaps] " __VA_ARGS__)