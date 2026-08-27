start_sources()

sources_platform(ALL)

add_sources("JDKLevelMapsPlugin_uber.cpp"
	SOURCE_GROUP "Root"
		"JDKLevelMapsPlugin.cpp"
		"JDKLevelMapsPlugin.h"
		"StdAfx.h"
		"StdAfx.cpp"
)

add_sources("JDKLevelMapsPlugin_Core_uber.cpp"
	SOURCE_GROUP "Core"
		"Core/BakersRegistry.h"
		"Core/BakersRegistry.cpp"
)

add_sources("JDKLevelMapsPlugin_Core_Bakers_uber.cpp"
	SOURCE_GROUP "Core\\\\Bakers"
		"Core/Bakers/IMapBaker.h"
)

add_sources("JDKLevelMapsPlugin_Core_Bakers_Vegetation_uber.cpp"
	SOURCE_GROUP "Core\\\\Bakers\\\\Vegetation"
		"Core/Bakers/Vegetation/EditorVegetationSource.h"
		"Core/Bakers/Vegetation/EditorVegetationSource.cpp"
		"Core/Bakers/Vegetation/VegetationBaker.h"
		"Core/Bakers/Vegetation/VegetationBaker.cpp"
		"Core/Bakers/Vegetation/VegetationClassifier.h"
)

add_sources("JDKLevelMapsPlugin_Core_Data_uber.cpp"
	SOURCE_GROUP "Core\\\\Data"
		"Core/Data/RunResult.h"
		"Core/Data/LevelContext.h"
		"Core/Data/LevelContext.cpp"
)

add_sources("JDKLevelMapsPlugin_Core_FileSystem_uber.cpp"
	SOURCE_GROUP "Core\\\\FileSystem"
		"Core/FileSystem/PathResolver.h"
		"Core/FileSystem/PathResolver.cpp"
		"Core/FileSystem/LFSFacade.h"
)

add_sources("JDKLevelMapsPlugin_Core_ImageWork_uber.cpp"
	SOURCE_GROUP "Core\\\\ImageWork"
		"Core/ImageWork/MapImageConverter.h"
)

add_sources("JDKLevelMapsPlugin_Core_ImageImport_uber.cpp"
	SOURCE_GROUP "Core\\\\ImageWork\\\\Import"
		"Core/ImageWork/Import/ImageImporter.h"
		"Core/ImageWork/Import/ImageImporter.cpp"
		"Core/ImageWork/Import/PNGStreamReader.h"
		"Core/ImageWork/Import/PNGStreamReader.cpp"
)

add_sources("JDKLevelMapsPlugin_Core_ImageExport_uber.cpp"
	SOURCE_GROUP "Core\\\\ImageWork\\\\Export"
		"Core/ImageWork/Export/ImageExporter.h"
		"Core/ImageWork/Export/ImageExporter.cpp"
		"Core/ImageWork/Export/PNGStreamWriter.h"
		"Core/ImageWork/Export/PNGStreamWriter.cpp"
)

add_sources("JDKLevelMapsPlugin_Core_MapWork_uber.cpp"
	SOURCE_GROUP "Core\\\\MapWork"
		"Core/MapWork/MapFileWriter.h"
		"Core/MapWork/MapFileWriter.cpp"
		"Core/MapWork/MapFileReader.h"
		"Core/MapWork/MapFileReader.cpp"
)

add_sources("JDKLevelMapsPlugin_Core_Orchestration_uber.cpp"
	SOURCE_GROUP "Core\\\\Orchestration"
		"Core/Orchestration/MapsBaker.h"
		"Core/Orchestration/MapsBaker.cpp"
		"Core/Orchestration/ImageLoader.h"
		"Core/Orchestration/ImageLoader.cpp"
)

add_sources("JDKLevelMapsPlugin_Editor_uber.cpp"
	SOURCE_GROUP "Editor"
		"Editor/JDKLevelMapsEditor.h"
		"Editor/JDKLevelMapsEditor.cpp"
)

add_sources("JDKLevelMapsPlugin_Editor_ViewModels_uber.cpp"
	SOURCE_GROUP "Editor\\\\ViewModels"
		"Editor/ViewModels/LevelMapsViewModel.h"
		"Editor/ViewModels/LevelMapsViewModel.cpp"
)

add_sources("JDKLevelMapsPlugin_Editor_Components_uber.cpp"
	SOURCE_GROUP "Editor\\\\Components"
		"Editor/Components/MapPreview.h"
		"Editor/Components/MapPreview.cpp"
		"Editor/Components/GenerateButton.h"
		"Editor/Components/GenerateButton.cpp"
)

add_sources("JDKLevelMapsPlugin_Settings_uber.cpp"
	SOURCE_GROUP "Settings"
		"Settings/BakerSettings.h"
		"Settings/VegetationBakerSettings.h"
)

add_sources("JDKLevelMapsPlugin_Shared_uber.cpp"
	SOURCE_GROUP "Shared"
		"Shared/MapHeader.h"
		"Shared/MapLayers.h"
)

add_sources("JDKLevelMapsPlugin_Utils_uber.cpp"
	SOURCE_GROUP "Utils"
		"Utils/ConvertUtils.h"
		"Utils/Progress.h"
		"Utils/Progress.cpp"
		"Utils/ScopedCryFile.h"
		"Utils/Logger.h"
		"Utils/ImageResizer.h"
		"Utils/ImageView.h"
		"Utils/Defines.h"
)

end_sources()

CryEditorPlugin(JDKLevelMapsPlugin PCH "StdAfx.cpp" SOLUTION_FOLDER "Sandbox/Plugins")
target_include_directories(${THIS_PROJECT} PRIVATE ".")

target_link_libraries(${THIS_PROJECT} PRIVATE Sandbox)
target_include_directories(${THIS_PROJECT} PRIVATE "${CRYENGINE_DIR}/Code/Sandbox/EditorQt")
use_xt()