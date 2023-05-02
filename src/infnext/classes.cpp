#include <game/server/gamecontext.h>

#include "class-define.h"
#include "classes.h"

CClasses::CClasses(CGameContext *pGameServer)
{
    m_pGameServer = pGameServer;
}

CClasses::~CClasses()
{
	for(int i = 0;i < (int) m_HumanClasses.size(); i++)
		delete m_HumanClasses[i].m_pClass;
	for(int i = 0;i < (int) m_InfectClasses.size(); i++)
		delete m_InfectClasses[i].m_pClass;
	m_HumanClasses.clear();
	m_InfectClasses.clear();
}

// Init Class
void CClasses::InitClasses()
{
	InitHumanClass(new CClassLooper(0, 0), true, 32);
	InitHumanClass(new CClassSniper(0, 0), true, 32);
	InitHumanClass(new CClassMedic(0, 0), true, 32);
	InitHumanClass(new CClassCaptain(0, 0), true, 32);

	InitInfectClass(new CClassHunter(0, 0), 33, 32);
	InitInfectClass(new CClassBoomer(0, 0), 33, 32);
	InitInfectClass(new CClassSmoker(0, 0), 33, 32);
}

// Humans
void CClasses::InitHumanClass(CClass *pClass, bool Enable, int Limit)
{
	CClassStatus NewStatus;
	NewStatus.m_pClass = pClass;
	NewStatus.m_Value = Enable;
	NewStatus.m_Limit = Limit;
	m_HumanClasses.push_back(NewStatus);
}

// Infects
void CClasses::InitInfectClass(CClass *pClass, int Proba, int Limit)
{
	CClassStatus NewStatus;
	NewStatus.m_pClass = pClass;
	NewStatus.m_Value = Proba;
	NewStatus.m_Limit = Limit;
	m_InfectClasses.push_back(NewStatus);
}