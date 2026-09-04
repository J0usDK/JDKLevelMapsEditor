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
		"Core/Data/MapContext.h"
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

	SOURCE_GROUP "Core\\\\MapWork\\\\Write"
		"Core/MapWork/Write/MapFileWriter.h"
		"Core/MapWork/Write/MapFileWriter.cpp"
		"Core/MapWork/Write/MapFileWriter_Prepare.cpp"
		"Core/MapWork/Write/MapFileWriter_Bake.cpp"

	SOURCE_GROUP "Core\\\\MapWork\\\\Read"
		"Core/MapWork/Read/MapFileReader.h"
		"Core/MapWork/Read/MapFileReader.cpp"
		"Core/MapWork/Read/MapFileReader_Prepare.cpp"
		"Core/MapWork/Read/MapFileReader_Read.cpp"

	SOURCE_GROUP "Core\\\\MapWork\\\\Strategies"
		"Core/MapWork/Strategies/DirectoryBitmaskStrategy.h"
		"Core/MapWork/Strategies/DirectoryBitmaskStrategy.cpp"
		"Core/MapWork/Strategies/DirectoryHybridStrategy.h"
		"Core/MapWork/Strategies/DirectoryHybridStrategy.cpp"
		"Core/MapWork/Strategies/IDirectoryFormatStrategy.h"
		"Core/MapWork/Strategies/IDirectoryFormatStrategy.cpp"
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
		"Utils/VectorUtils.h"
		"Utils/IOUtils.h"
)

add_sources("JDKLevelMapsPlugin_Includes_uber.cpp"
	SOURCE_GROUP "Includes"

	SOURCE_GROUP "Includes\\\\JDKMath"
		"Includes/JDKMath/MathConfig.h"
		"Includes/JDKMath/MathAssert.h"
		"Includes/JDKMath/CpuFeatures.h"
		"Includes/JDKMath/MathGeneral.h"
		"Includes/JDKMath/BitMath.h"
		"Includes/JDKMath/BitmaskRank.h"
		"Includes/JDKMath/FlatRankTable.h"
		"Includes/JDKMath/TwoLevelRankTable.h"
		"Includes/JDKMath/UniversalRankTable.h"
)

end_sources()

CryEditorPlugin(JDKLevelMapsPlugin PCH "StdAfx.cpp" SOLUTION_FOLDER "Sandbox/Plugins")
target_include_directories(${THIS_PROJECT} PRIVATE ".")

target_link_libraries(${THIS_PROJECT} PRIVATE Sandbox)
target_include_directories(${THIS_PROJECT} PRIVATE "${CRYENGINE_DIR}/Code/Sandbox/EditorQt")
use_xt()