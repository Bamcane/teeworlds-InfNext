#ifndef ENGINE_SERVER_MAPCONVERTER_H
#define ENGINE_SERVER_MAPCONVERTER_H

#include <base/tl/array.h>
#include <engine/storage.h>
#include <engine/map.h>
#include <engine/console.h>
#include <engine/shared/datafile.h>
#include <game/mapitems.h>
#include <game/gamecore.h>
#include <infnext/classes.h>


enum
{
	TIMESHIFT_MENUCLASS = 60,
	MENU_RADIUS = 192,
	MENU_ANGLE_START = -90
};

class CMapConverter
{
protected:
	IStorage *m_pStorage;
	IEngineMap *m_pMap;
	IConsole *m_pConsole;
	CClasses *m_pClasses;

	CDataFileWriter m_DataFile;
	
	CTile* m_pTiles;
	CTile* m_pPhysicsLayerTiles;
	int m_Width;
	int m_Height;
	
	int m_NumGroups;
	int m_NumLayers;
	int m_NumImages;
	int m_NumSounds;
	int m_NumEnvs;
	array<CEnvPoint> m_lEnvPoints;
	
	vec2 m_MenuPosition;
	int m_AnimationCycle;
	int m_TimeShiftUnit;

protected:	
	IEngineMap* Map() { return m_pMap; };
	IStorage* Storage() { return m_pStorage; };
	IConsole* Console() { return m_pConsole; };
	CClasses* Classes() { return m_pClasses; }
	
	void InitQuad(CQuad* pQuad);
	void InitQuad(CQuad* pQuad, vec2 Pos, vec2 Size);

	void InitState();
	void QuantizeAnimation(int Quant);
	
	void CopyVersion();
	void CopyMapInfo();
	void CopyImages();
	void CopySounds();
	void CopyGameLayer();
	void CopyTeleLayer();
	void CopySpeedupLayer();
	void CopyLayers();
	void CopyAnimations();
	
	void AddImageQuad(const char* pName, int ImageID, int GridX, int GridY, int X, int Y, int Width, int Height, vec2 Pos, vec2 Size, vec4 Color, int Env);
	void AddTeeLayer(const char* pName, int ImageID, vec2 Pos, float Size, int Env=-1, bool Black=false);
	int AddExternalImage(const char* pImageName, int Width, int Height);
	
	void Finalize();
	void CreateCircle(array<CQuad>* pQuads, vec2 Pos, float Size, vec4 Color, int Env=-1, int EnvTO=0);
	
	// 0.7
	bool CheckImageDimensions(void *pLayerItem, int LayerType, const char *pFilename);
	void *ReplaceImageItem(void *pItem, int Type, CMapItemImage *pNewImgItem);
	int ConvertMapTo7(const char* pFilename);

public:
	CMapConverter(IStorage *pStorage, IEngineMap *pMap, IConsole* pConsole, CClasses *pClasses);
	~CMapConverter();
	
	bool Load();
	bool CreateMap(const char* pFilename, const char *pMapName);
	
	inline int GetTimeShiftUnit() const { return m_TimeShiftUnit; }
};

#endif
