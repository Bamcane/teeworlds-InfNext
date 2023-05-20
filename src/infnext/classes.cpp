#include <game/server/gamecontext.h>

#include <engine/shared/config.h>

#include <engine/console.h>

#include "class-define.h"
#include "classes.h"

IConsole *CClasses::Console()
{
	return GameServer()->Console();
}

CClasses::CClasses(CGameContext *pGameServer)
{
    m_pGameServer = pGameServer;

	Console()->Register("inf_set_class", "is", CFGFLAG_SERVER, ConSetClass, this, "");
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

	InitInfectClass(new CClassHunter(0, 0), 30, 32);
	InitInfectClass(new CClassBoomer(0, 0), 30, 32);
	InitInfectClass(new CClassSmoker(0, 0), 30, 32);
	InitInfectClass(new CClassQueen(0, 0), 10, 1);
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

void CClasses::ConSetClass(IConsole::IResult *pResult, void *pUserData)
{
	CClasses *pSelf = (CClasses *) pUserData;
	
	int ClientID = pResult->GetInteger(0);
	CPlayer *pPlayer = pSelf->GameServer()->m_apPlayers[ClientID];

	if(!pPlayer)
		return;

	const char* ClassName = pResult->GetString(1);

	for(unsigned int i = 0;i < (unsigned int) pSelf->m_HumanClasses.size(); i ++)
	{
		if(str_comp_nocase(pSelf->m_HumanClasses[i].m_pClass->m_ClassName, ClassName) == 0)
		{
			CClass *pNewClass = pSelf->m_HumanClasses[i].m_pClass->CreateNewOne(pSelf->GameServer(), pPlayer);
			pPlayer->SetClass(pNewClass);
			return;
		}
	}

	for(unsigned int i = 0;i < (unsigned int) pSelf->m_InfectClasses.size(); i ++)
	{
		if(str_comp_nocase(pSelf->m_InfectClasses[i].m_pClass->m_ClassName, ClassName) == 0)
		{
			CClass *pNewClass = pSelf->m_InfectClasses[i].m_pClass->CreateNewOne(pSelf->GameServer(), pPlayer);
			pPlayer->SetClass(pNewClass);
			return;
		}
	}
}