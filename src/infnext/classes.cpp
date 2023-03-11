#include <game/server/gamecontext.h>

#include "class-define.h"
#include "classes.h"

CClasses::CClasses(CGameContext *pGameServer)
{
    m_pGameServer = pGameServer;
}

CClasses::~CClasses()
{
	for(int i = 0;i < m_HumanClasses.size(); i++)
		delete m_HumanClasses[i].m_pClass;
	for(int i = 0;i < m_InfectClasses.size(); i++)
		delete m_InfectClasses[i].m_pClass;
}

// Init Class
void CClasses::InitClasses()
{
	InitHumanClass(new CClassLooper(GameServer()), true);
	InitHumanClass(new CClassSniper(GameServer()), true);
	InitHumanClass(new CClassMedic(GameServer()), true);
	InitHumanClass(new CClassCaptain(GameServer()), true);

	InitInfectClass(new CClassHunter(GameServer()), 33);
	InitInfectClass(new CClassBoomer(GameServer()), 33);
	InitInfectClass(new CClassSmoker(GameServer()), 33);
}

// Humans
void CClasses::InitHumanClass(CClass *pClass, bool Enable)
{
	CClassStatus NewStatus;
	NewStatus.m_pClass = pClass;
	NewStatus.m_Value = Enable;
	m_HumanClasses.add(NewStatus);
}

// Infects
void CClasses::InitInfectClass(CClass *pClass, int Proba)
{
	CClassStatus NewStatus;
	NewStatus.m_pClass = pClass;
	NewStatus.m_Value = Proba;
	m_InfectClasses.add(NewStatus);
}