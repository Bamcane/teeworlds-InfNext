#include <string>

#include <game/server/gamecontext.h>
#include <engine/shared/config.h>

#include "class-define.h"
#include "infnext.h"

CGameControllerNext::CGameControllerNext(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_pGameType = "InfNext";

	m_LastPlayersNum = 0;

	InitClasses();
}

CGameControllerNext::~CGameControllerNext()
{
}

void CGameControllerNext::Tick()
{
	int Infects=0, Humans=0;
	for(int i = 0;i < MAX_CLIENTS;i++)
	{
		if(!Server()->ClientIngame(i) || !GameServer()->m_apPlayers[i]) 
			continue;
		if(GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS) 
			continue;
		
		if(GameServer()->m_apPlayers[i]->IsInfect()) 
			Infects++;
		else if(GameServer()->m_apPlayers[i]->IsHuman()) 
			Humans++;
	}

	if(m_LastPlayersNum < 2 && (Infects + Humans) >= 2)
	{
		GameServer()->SendBroadcast("", -1);
		EndRound();
	}
	else if(m_LastPlayersNum < 2 && (Server()->Tick()%25) == 0) 
	{
		GameServer()->SendBroadcast_VL(_("Wait game start!"), -1);
	}

	if(RoundSecond() < 10) // send class chooser
	{
		SendClassChooser();
	}

	// start infection
	if((Server()->Tick()-m_RoundStartTick) == 500)
	{
		CreateInfects();
		CheckNoClass();
	}

	// do win check
	if(m_LastPlayersNum >= 2 && m_GameOverTick == -1 && !m_Warmup && !GameServer()->m_World.m_ResetRequested)
	{
		if(!Humans && Infects)
		{
			GameServer()->SendChatTarget_Localization(-1, _("INF| Infects win!"));
			GameServer()->CreateSoundGlobal(SOUND_CTF_DROP, -1);
			EndRound();
		}else if((Server()->Tick()-m_RoundStartTick) >= g_Config.m_SvTimelimit*Server()->TickSpeed()*60)
		{
			GameServer()->SendChatTarget_Localization(-1, _("HUM| Humans win!"));
			GameServer()->CreateSoundGlobal(SOUND_CTF_DROP, -1);
			EndRound();
		}
	}

	m_LastPlayersNum = Humans + Infects;

	IGameController::Tick();
}

int CGameControllerNext::RoundTick() const
{
	return Server()->Tick() - m_RoundStartTick;
}

int CGameControllerNext::RoundSecond() const
{
	return RoundTick()/Server()->TickSpeed();
}

// Init Class
void CGameControllerNext::InitClasses()
{
	InitHumanClass(new CClassLooper(GameServer()), true);
	InitHumanClass(new CClassSniper(GameServer()), true);

	InitInfectClass(new CClassHunter(GameServer()), 50);
	InitInfectClass(new CClassBoomer(GameServer()), 50);
}

// Humans
void CGameControllerNext::InitHumanClass(CClass *pClass, bool Enable)
{
	CClassStatus NewStatus;
	NewStatus.m_pClass = pClass;
	NewStatus.m_Value = Enable;
	m_HumanClasses.add(NewStatus);
}

// Infects
void CGameControllerNext::InitInfectClass(CClass *pClass, int Proba)
{
	CClassStatus NewStatus;
	NewStatus.m_pClass = pClass;
	NewStatus.m_Value = Proba;
	m_InfectClasses.add(NewStatus);
}

CClass* CGameControllerNext::OnPlayerInfect(CPlayer *pPlayer)
{
	int ClassesNum = m_InfectClasses.size();
	double Probability[ClassesNum];

	for(int i = 0;i < ClassesNum; i ++)
	{
		Probability[i] = (double)m_InfectClasses[i].m_Value;
	}

	int Seconds = (Server()->Tick()-m_RoundStartTick)/((float)Server()->TickSpeed());

	int ClassID = random_distribution(Probability, Probability + ClassesNum);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "infected victim='%s' duration='%d' newclass='%s'", 
		Server()->ClientName(pPlayer->GetCID()), Seconds, m_InfectClasses[ClassID].m_pClass->m_ClassName);

	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	return m_InfectClasses[ClassID].m_pClass;
}

void CGameControllerNext::SendClassChooser()
{
	for(int i = 0;i < MAX_CLIENTS; i ++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];

		if(!pPlayer) 
			continue;
		
		if(pPlayer->GetClass())
			continue;

		const char *pLanguage = GameServer()->m_apPlayers[i]->GetLanguage();
		
		std::string Buffer;

		Buffer.append("=====");
		Buffer.append(GameServer()->Localize(pLanguage, _("Class Chooser")));
		Buffer.append("=====");

		pPlayer->m_ClassChooserLine = abs(pPlayer->m_ClassChooserLine%m_HumanClasses.size());

		for(int j = 0; j < m_HumanClasses.size(); j ++)
		{
			Buffer.append("\n");

			if(!m_HumanClasses[j].m_Value)
			{
				Buffer.append("----");
			}

			if(j == pPlayer->m_ClassChooserLine)
				Buffer.append("[");

			Buffer.append(GameServer()->Localize(pLanguage, m_HumanClasses[j].m_pClass->m_ClassName));
			
			if(j == pPlayer->m_ClassChooserLine)
				Buffer.append("]");
		}

		if(m_HumanClasses[pPlayer->m_ClassChooserLine].m_Value && pPlayer->GetCharacter() && pPlayer->GetCharacter()->GetLatestInput()->m_Fire&1)
		{
			pPlayer->SetClass(m_HumanClasses[pPlayer->m_ClassChooserLine].m_pClass);
			GameServer()->SendMotd(i, "");
		}
		else GameServer()->SendMotd(i, Buffer.c_str());
	}

}

void CGameControllerNext::CheckNoClass()
{
	for(int i = 0;i < MAX_CLIENTS; i ++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];

		if(!pPlayer) 
			continue;
		
		if(pPlayer->GetClass())
			continue;

		int Class = 0;
		do
		{
			Class = random_int(0, m_HumanClasses.size()-1);
		}while(!m_HumanClasses[Class].m_Value);

		pPlayer->SetClass(m_HumanClasses[Class].m_pClass);
		
		GameServer()->SendMotd(i, "");
	}
}

void CGameControllerNext::CreateInfects()
{
	int InfectNum;
	if(m_LastPlayersNum > 32)
		InfectNum = 5;
	else if(m_LastPlayersNum > 16)
		InfectNum = 4;
	else if(m_LastPlayersNum > 8)
		InfectNum = 3;
	else if(m_LastPlayersNum > 4)
		InfectNum = 2;
	else
		InfectNum = 2;

	array<int> tmpList;

	for(int i = 0;i < InfectNum;i ++)
	{
		int InfectID=-1;
		do
		{
			InfectID = random_int(0, MAX_CLIENTS-1);
		}while(PlayerInfected(InfectID));

		GameServer()->m_apPlayers[InfectID]->Infect();
		tmpList.add(InfectID);
	}
	m_LastInfect.clear();
	m_LastInfect = tmpList;
}

bool CGameControllerNext::PlayerInfected(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return true;

	if(!GameServer()->m_apPlayers[ClientID])
		return true;

	for(int i = 0;i < m_LastInfect.size();i ++)
	{
		if(m_LastInfect[i] == ClientID)
			return true;
	}
	return false;
}