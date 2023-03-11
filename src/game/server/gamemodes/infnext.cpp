#include <string>

#include <game/server/gamecontext.h>
#include <engine/shared/config.h>

#include <engine/server/mapconverter.h>
#include <infnext/classes.h>

#include "infnext.h"

CGameControllerNext::CGameControllerNext(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_pGameType = "InfNext";

	m_LastPlayersNum = 0;
}

CGameControllerNext::~CGameControllerNext()
{
}

CClasses *CGameControllerNext::Classes()
{
	return Server()->Classes();
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

	if(m_LastPlayersNum < 2) 
	{
		GameServer()->SendBroadcast_Localization(-1, _("Wait game start!"), 0.1f, BROADCAST_INFO);
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

CClass* CGameControllerNext::OnPlayerInfect(CPlayer *pPlayer)
{
	int ClassesNum = Classes()->m_InfectClasses.size();
	double Probability[ClassesNum];

	for(int i = 0;i < ClassesNum; i ++)
	{
		Probability[i] = (double)Classes()->m_InfectClasses[i].m_Value;
	}

	int Seconds = (Server()->Tick()-m_RoundStartTick)/((float)Server()->TickSpeed());

	int ClassID = random_distribution(Probability, Probability + ClassesNum);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "infected victim='%s' duration='%d' newclass='%s'", 
		Server()->ClientName(pPlayer->GetCID()), Seconds, Classes()->m_InfectClasses[ClassID].m_pClass->m_ClassName);

	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	return Classes()->m_InfectClasses[ClassID].m_pClass;
}

void CGameControllerNext::SendClassChooser()
{
}

// from infclass
void CGameControllerNext::Snap(int SnappingClient)
{
	CNetObj_GameInfo *pGameInfoObj = (CNetObj_GameInfo *)Server()->SnapNewItem(NETOBJTYPE_GAMEINFO, 0, sizeof(CNetObj_GameInfo));
	if(!pGameInfoObj)
		return;

	pGameInfoObj->m_GameFlags = m_GameFlags;
	pGameInfoObj->m_GameStateFlags = 0;
	if(m_GameOverTick != -1)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_GAMEOVER;
	if(m_SuddenDeath)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_SUDDENDEATH;
	if(GameServer()->m_World.m_Paused)
		pGameInfoObj->m_GameStateFlags |= GAMESTATEFLAG_PAUSED;
	pGameInfoObj->m_RoundStartTick = m_RoundStartTick;
	pGameInfoObj->m_WarmupTimer = GameServer()->m_World.m_Paused ? m_UnpauseTimer : m_Warmup;

	pGameInfoObj->m_ScoreLimit = g_Config.m_SvScorelimit;
	pGameInfoObj->m_TimeLimit = g_Config.m_SvTimelimit;

	pGameInfoObj->m_RoundNum = (str_length(g_Config.m_SvMaprotation) && g_Config.m_SvRoundsPerMap) ? g_Config.m_SvRoundsPerMap : 0;
	pGameInfoObj->m_RoundCurrent = m_RoundCount+1;
	
	int ClassMask = 0;
	for(int i=0; i<Classes()->m_HumanClasses.size(); i++) 
	{
		if(Classes()->m_HumanClasses[i].m_Value)
			ClassMask |= 1<<i;
	}

	if(SnappingClient != -1)
	{
		if(GameServer()->m_apPlayers[SnappingClient])
		{
			int Page = -1;
			
			if(!GameServer()->m_apPlayers[SnappingClient]->GetClass())
			{
				int Item = GameServer()->m_apPlayers[SnappingClient]->m_MapMenuItem;
				Page = TIMESHIFT_MENUCLASS + 3*((Item+1) + ClassMask*(Classes()->m_HumanClasses.size()+1)) + 1;
			}
			
			if(Page >= 0)
			{
				double PageShift = static_cast<double>(Page * Server()->GetTimeShiftUnit())/1000.0f;
				double CycleShift = fmod(static_cast<double>(Server()->Tick() - pGameInfoObj->m_RoundStartTick)/Server()->TickSpeed(), Server()->GetTimeShiftUnit()/1000.0f);
				int TimeShift = (PageShift + CycleShift)*Server()->TickSpeed();
				
				pGameInfoObj->m_RoundStartTick = Server()->Tick() - TimeShift;
				pGameInfoObj->m_TimeLimit += (TimeShift/Server()->TickSpeed())/60;
			}
		}
	}

	CNetObj_GameInfoEx* pGameInfoEx = (CNetObj_GameInfoEx*)Server()->SnapNewItem(NETOBJTYPE_GAMEINFOEX, 0, sizeof(CNetObj_GameInfoEx));
	if(!pGameInfoEx)
		return;

	pGameInfoEx->m_Flags = GAMEINFOFLAG_ALLOW_EYE_WHEEL | GAMEINFOFLAG_ALLOW_HOOK_COLL | GAMEINFOFLAG_DONT_MASK_ENTITIES | GAMEINFOFLAG_ENTITIES_DDNET | GAMEINFOFLAG_PREDICT_VANILLA;
	pGameInfoEx->m_Flags2 = GAMEINFOFLAG2_NO_WEAK_HOOK_AND_BOUNCE | GAMEINFOFLAG2_HUD_DDRACE | GAMEINFOFLAG2_HUD_AMMO | GAMEINFOFLAG2_HUD_HEALTH_ARMOR;
	pGameInfoEx->m_Version = GAMEINFO_CURVERSION;

	if(Server()->IsSixup(SnappingClient))
	{
		protocol7::CNetObj_GameData *pGameData = static_cast<protocol7::CNetObj_GameData *>(Server()->SnapNewItem(-protocol7::NETOBJTYPE_GAMEDATA, 0, sizeof(protocol7::CNetObj_GameData)));
		if(!pGameData)
			return;

		pGameData->m_GameStartTick = m_RoundStartTick;
		pGameData->m_GameStateFlags = 0;
		if(m_GameOverTick != -1)
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_GAMEOVER;
		if(m_SuddenDeath)
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_SUDDENDEATH;
		if(GameServer()->m_World.m_Paused)
			pGameData->m_GameStateFlags |= protocol7::GAMESTATEFLAG_PAUSED;

		pGameData->m_GameStateEndTick = 0;

		if(GameServer()->m_apPlayers[SnappingClient])
		{
			if(!GameServer()->m_apPlayers[SnappingClient]->GetClass())
			{
				pGameData->m_GameStartTick = Server()->Tick();
			}
		}
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
			Class = random_int(0, Classes()->m_HumanClasses.size()-1);
		}while(!Classes()->m_HumanClasses[Class].m_Value);

		pPlayer->SetClass(Classes()->m_HumanClasses[Class].m_pClass);
		
		GameServer()->SendMotd(i, "");
	}
}

void CGameControllerNext::CreateInfects()
{
	int InfectNum;
	if(InfectNum > 24)
		InfectNum = 4;
	else if(InfectNum > 8)
		InfectNum = 3;
	else if(InfectNum > 4)
		InfectNum = 2;
	else InfectNum = 1;

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

void CGameControllerNext::OnPlayerSelectClass(CPlayer* pPlayer)
{
	if(!pPlayer)
		return;
	if(!pPlayer->GetCharacter())
		return;

	int Class = pPlayer->m_MapMenuItem;
	int ClientID = pPlayer->GetCID();

	CCharacter *pChr = pPlayer->GetCharacter();

	if(pChr->GetLatestInput()->m_Fire&1)
	{
		pPlayer->SetClass(Classes()->m_HumanClasses[Class].m_pClass);
	}
	else 
	{
		GameServer()->SendBroadcast_Localization(ClientID, Classes()->m_HumanClasses[Class].m_pClass->m_ClassName, 0.5f, BROADCAST_CLASS);
	}
}