/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_LAYERS_H
#define GAME_LAYERS_H

#include "mapitems.h"

class IKernel;
class IMap;

struct CMapItemGroupEx;

class CLayers
{
	int m_GroupsNum;
	int m_GroupsStart;
	int m_GroupsExNum;
	int m_GroupsExStart;
	int m_LayersNum;
	int m_LayersStart;
	CMapItemGroup *m_pGameGroup;
	CMapItemGroupEx *m_pGameGroupEx;
	CMapItemLayerTilemap *m_pGameLayer;
	class IMap *m_pMap;

public:
	CLayers();
	void Init(class IKernel *pKernel);
	void Init(IMap *pMap);
	int NumGroups() const { return m_GroupsNum; };
	class IMap *Map() const { return m_pMap; };
	CMapItemGroup *GameGroup() const { return m_pGameGroup; };
	CMapItemGroupEx *GameGroupEx() const { return m_pGameGroupEx; }
	CMapItemLayerTilemap *GameLayer() const { return m_pGameLayer; };
	CMapItemGroup *GetGroup(int Index) const;
	CMapItemGroupEx *GetGroupEx(int Index) const;
	CMapItemLayer *GetLayer(int Index) const;

	// DDRace

	CMapItemLayerTilemap *TeleLayer() const { return m_pTeleLayer; }
	CMapItemLayerTilemap *SpeedupLayer() const { return m_pSpeedupLayer; }

private:
	CMapItemLayerTilemap *m_pTeleLayer;
	CMapItemLayerTilemap *m_pSpeedupLayer;
};

#endif
