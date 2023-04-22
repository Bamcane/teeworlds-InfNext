/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_COLLISION_H
#define GAME_COLLISION_H

#include <base/vmath.h>
#include <base/tl/array.h>

struct ZoneData
{
	int Index = -1;
	int ExtraData = -1;
};

class CCollision
{
	class CTile *m_pTiles;
	int m_Width;
	int m_Height;
	class CLayers *m_pLayers;
	
	double m_Time;

	array< array<int> > m_Zones;

	bool IsTileSolid(int x, int y);
	int GetTile(int x, int y);

public:

	CCollision();
	~CCollision();
	void Init(class CLayers *pLayers);
	bool CheckPoint(float x, float y) { return IsTileSolid(round_to_int(x), round_to_int(y)); }
	bool CheckPoint(vec2 Pos) { return CheckPoint(Pos.x, Pos.y); }
	int GetCollisionAt(float x, float y) { return GetTile(round_to_int(x), round_to_int(y)); }
	bool IsCollisionTile(float x, float y, int Flag) { return GetCollisionAt(x, y) == Flag; } 
	int GetWidth() { return m_Width; };
	int GetHeight() { return m_Height; };
	int IntersectLine(vec2 Pos0, vec2 Pos1, vec2 *pOutCollision, vec2 *pOutBeforeCollision);
	void MovePoint(vec2 *pInoutPos, vec2 *pInoutVel, float Elasticity, int *pBounces);
	void MoveBox(vec2 *pInoutPos, vec2 *pInoutVel, vec2 Size, float Elasticity);
	bool TestBox(vec2 Pos, vec2 Size);
	
	void Dest();

	void SetTime(double Time) { m_Time = Time; }
	
	//This function return an Handle to access all zone layers with the name "pName"
	int GetZoneHandle(const char* pName);
	int GetZoneValueAt(int ZoneHandle, float x, float y, ZoneData *pData = nullptr);
	int GetZoneValueAt(int ZoneHandle, vec2 Pos, ZoneData *pData = nullptr) { return GetZoneValueAt(ZoneHandle, Pos.x, Pos.y, pData); }
	
};

#endif
