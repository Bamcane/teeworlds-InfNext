#ifndef INFNEXT_CLASSES_H
#define INFNEXT_CLASSES_H

#include "class.h"

#include <base/tl/array.h>

class CGameContext;

class CClasses
{
	CGameContext *m_pGameServer;

private:
	CGameContext *GameServer() {return m_pGameServer;}

public:
	CClasses(CGameContext *pGameServer);
	~CClasses();

	class CClassStatus
	{
	public:
		CClassStatus() {};
		CClass *m_pClass;
		int m_Value;
	};

	array<CClassStatus> m_HumanClasses;
	array<CClassStatus> m_InfectClasses;

	void InitClasses();
	void InitHumanClass(CClass *pClass, bool Enable);
	void InitInfectClass(CClass *pClass, int Proba);
};

#endif