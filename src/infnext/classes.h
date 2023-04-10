#ifndef INFNEXT_CLASSES_H
#define INFNEXT_CLASSES_H

#include "class.h"

#include <vector>

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
		int m_Limit;
	};

	std::vector<CClassStatus> m_HumanClasses;
	std::vector<CClassStatus> m_InfectClasses;

	void InitClasses();
	void InitHumanClass(CClass *pClass, bool Enable, int Limit);
	void InitInfectClass(CClass *pClass, int Proba, int Limit);
};

#endif