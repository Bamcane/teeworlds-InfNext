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
	for(int i = 0;i < m_HumanClasses.size(); i++)
		delete m_HumanClasses[i].m_pClass;
	for(int i = 0;i < m_InfectClasses.size(); i++)
		delete m_InfectClasses[i].m_pClass;
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

	if(!IsInfectionStarted() && (Infects + Humans) >= 2) // send class chooser
	{
		SendClassChooser();
	}

	// start infection
	if((Server()->Tick()-m_RoundStartTick) == 500 && (Infects + Humans) >= 2)
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

int CGameControllerNext::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if(!pKiller || Weapon == WEAPON_GAME)
		return 0;

	if(pVictim->GetPlayer() != pKiller)
	{
		if(pKiller->IsInfect())
		{
			GameServer()->SendChatTarget_Localization(pKiller->GetCID(), _("You infected '%s'"), Server()->ClientName(pVictim->GetCID()));
			GameServer()->SendChatTarget_Localization(pVictim->GetCID(), _("You're infected by '%s'"), Server()->ClientName(pKiller->GetCID()));
			pKiller->m_Score += 3;
		}else
		{
			pKiller->m_Score += 1;
		}
		GameServer()->CreateSound(pKiller->m_ViewPos, SOUND_CTF_GRAB_PL, CmaskOne(pKiller->GetCID()));
	}
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
	InitHumanClass(new CClassMedic(GameServer()), true);

	InitInfectClass(new CClassHunter(GameServer()), 33);
	InitInfectClass(new CClassBoomer(GameServer()), 33);
	InitInfectClass(new CClassSmoker(GameServer()), 33);
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

		if(pPlayer->GetTeam() == TEAM_SPECTATORS)
			continue;
		
		if(pPlayer->GetClass())
			continue;

		if(pPlayer->m_PlayerFlags&PLAYERFLAG_IN_MENU || pPlayer->m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
			continue;

		pPlayer->m_ClassChooserLine = abs(pPlayer->m_ClassChooserLine%m_HumanClasses.size());

		if(m_HumanClasses[pPlayer->m_ClassChooserLine].m_Value && pPlayer->GetCharacter() && pPlayer->GetCharacter()->GetLatestInput()->m_Fire&1)
		{
			pPlayer->SetClass(m_HumanClasses[pPlayer->m_ClassChooserLine].m_pClass);
			GameServer()->SendMotd(i, "");
			continue;
		}

		if(Server()->Tick() % 50)
			continue;

		const char *pLanguage = GameServer()->m_apPlayers[i]->GetLanguage();
		
		std::string Buffer;

		Buffer.append("=====");
		Buffer.append(GameServer()->Localize(pLanguage, _("Class Chooser")));
		Buffer.append("=====");

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
		
		GameServer()->SendMotd(i, Buffer.c_str());
	}

}

void CGameControllerNext::CheckNoClass()
{
	for(int i = 0;i < MAX_CLIENTS; i ++)
	{
		CPlayer *pPlayer = GameServer()->m_apPlayers[i];

		if(!pPlayer) 
			continue;

		if(pPlayer->GetTeam() == TEAM_SPECTATORS)
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
	InfectNum = m_LastPlayersNum/5+1;

	array<int> tmpList;

	for(int i = 0;i < InfectNum;i ++)
	{
		int BestScore=0;

		int InfectID=-1;

		for(int j = 0; j < MAX_CLIENTS; j ++)
		{
			if(!GameServer()->m_apPlayers[j])
				continue;

			for(int k = 0;k < tmpList.size();k ++)
			{
				if(tmpList[k] == j)
					continue;
			}

			float Scoremod = 1.0f;
			int Score = 10;
			
			if(PlayerInfected(j))
			{
				Scoremod = 0.4f;
				if(GameServer()->m_apPlayers[j]->m_LastScore)
					Scoremod -= max(0.f, GameServer()->m_apPlayers[j]->m_LastScore/120.0f);
			}
			
			Score *= Scoremod;
			if(Score > BestScore)
			{
				InfectID = j;
				BestScore = Score;
			}
		};
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

bool CGameControllerNext::IsSpawnable(vec2 Pos)
{
	//First check if there is a tee too close
	CCharacter *aEnts[MAX_CLIENTS];
	int Num = GameServer()->m_World.FindEntities(Pos, 64, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
	
	for(int c = 0; c < Num; ++c)
	{
		if(distance(aEnts[c]->m_Pos, Pos) <= 60)
			return false;
	}
	
	//Check the center
	if(GameServer()->Collision()->CheckPoint(Pos))
		return false;

	//Check the border of the tee. Kind of extrem, but more precise
	for(int i=0; i<16; i++)
	{
		float Angle = i * (2.0f * pi / 16.0f);
		vec2 CheckPos = Pos + vec2(cos(Angle), sin(Angle)) * 30.0f;
		
		if(GameServer()->Collision()->CheckPoint(CheckPos))
			return false;
	}
	
	return true;
}

bool CGameControllerNext::PreSpawn(CPlayer* pPlayer, vec2 *pOutPos)
{
	// spectators can't spawn
	if(pPlayer->GetTeam() == TEAM_SPECTATORS)
		return false;
	
	if(IsInfectionStarted())
		pPlayer->Infect();
			
	int Type = (pPlayer->IsInfect() ? TEAM_RED : TEAM_BLUE);

	// get spawn point
	int RandomShift = random_int(0, m_aaSpawnPoints[Type].size()-1);
	for(int i = 0; i < m_aaSpawnPoints[Type].size(); i++)
	{
		int I = (i + RandomShift)%m_aaSpawnPoints[Type].size();
		if(IsSpawnable(m_aaSpawnPoints[Type][I]))
		{
			*pOutPos = m_aaSpawnPoints[Type][I];
			return true;
		}
	}
	
	return false;
}