#include "StdAfx.h"
#include "PathResolver.h"

#include <IEditor.h>
#include <ILevelEditor.h>
#include <Cry3DEngine/I3DEngine.h>

#include "Utils/Logger.h"

namespace JDKLevelMaps::FileSystem
{
	CPathResolver::CPathResolver()
	{
		RecomputePath();
	}

	void CPathResolver::RecomputePath()
	{
		m_bInitialized = false;
		m_defaultPath.clear();

		ILevelEditor* pLevelEditor = GetIEditor()->GetLevelEditor();
		if (!pLevelEditor || !pLevelEditor->IsLevelLoaded())
			return;

		m_defaultPath = gEnv->p3DEngine->GetLevelFilePath("JDKLevelMaps");

		if (!gEnv->pCryPak->MakeDir(m_defaultPath.c_str()))
		{
			JDK_ERR("Can't create plugin directory: %s", m_defaultPath.c_str());
			return;
		}

		m_defaultPath += "/";
		m_bInitialized = true;
	}

	std::optional<std::string> CPathResolver::GetMapPath(const char* bakerId) const
	{
		if (!m_bInitialized)
			return std::nullopt;

		return m_defaultPath + bakerId + kMapExtension;
	}

	std::optional<std::string> CPathResolver::GetImagePath(const char* bakerId) const
	{
		if (!m_bInitialized)
			return std::nullopt;

		return m_defaultPath + bakerId + kImageExtension;
	}
}