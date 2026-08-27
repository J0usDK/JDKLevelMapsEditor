#include "StdAfx.h"
#include "LevelContext.h"

#include <Cry3DEngine/I3DEngine.h>

namespace JDKLevelMaps::Data
{
	int GetLevelTerrainSize() noexcept
	{
		CRY_ASSERT(gEnv && gEnv->p3DEngine);
		return gEnv->p3DEngine->GetTerrainSize();
	}
}