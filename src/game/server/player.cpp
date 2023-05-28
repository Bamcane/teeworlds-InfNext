/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <new>
#include <engine/shared/config.h>
#include <infnext/infdefine.h>
#include "player.h"

struct Skin
{
	char m_aSkinName[64];
	// body, marking, decoration, hands, feet, eyes
	char m_apSkinPartNames[6][24];
	bool m_aUseCustomColors[6];
	int m_aSkinPartColors[6];
};


static Skin g_DefaultSkin = {"defalut", {"standard", "", "", "standard", "standard", "standard"}, {true, false, false, true, true, false}, {1798004, 0, 0, 1799582, 1869630, 0}};

MACRO_ALLOC_POOL_ID_IMPL(CPlayer, MAX_CLIENTS)

IServer *CPlayer::Server() const { return m_pGameServer->Server(); }

CPlayer::CPlayer(CGameContext *pGameServer, int ClientID, int Team)
{
	m_pGameServer = pGameServer;
	m_RespawnTick = Server()->Tick();
	m_DieTick = Server()->Tick();
	m_ScoreStartTick = Server()->Tick();
	m_LastScore = 0;
	m_pCharacter = 0;
	m_pClass = 0;
	m_ClientID = ClientID;
	m_Team = Team;
	m_SpectatorID = SPEC_FREEVIEW;
	m_LastActionTick = Server()->Tick();
	m_TeamChangeTick = Server()->Tick();

	m_Authed = IServer::AUTHED_NO;

	m_PrevTuningParams = *pGameServer->Tuning();
	m_NextTuningParams = m_PrevTuningParams;

	int* idMap = Server()->GetIdMap(ClientID);
	for (int i = 1;i < VANILLA_MAX_CLIENTS;i++)
	{
	    idMap[i] = -1;
	}
	idMap[0] = ClientID;

	CureToDefault();
}

CPlayer::~CPlayer()
{
	delete m_pCharacter;
	m_pCharacter = 0;

	if(m_pClass)
		delete m_pClass;
	
	m_pClass = 0;
}

void CPlayer::HandleTuningParams()
{
	if(!(m_PrevTuningParams == m_NextTuningParams))
	{
		if(m_IsReady)
		{
			CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
			int *pParams = (int *)&m_NextTuningParams;

			for(unsigned i = 0; i < sizeof(m_NextTuningParams)/sizeof(int); i++)
				Msg.AddInt(pParams[i]);
			
			Server()->SendMsg(&Msg, MSGFLAG_VITAL, GetCID());
		}

		m_PrevTuningParams = m_NextTuningParams;
	}

	m_NextTuningParams = *GameServer()->Tuning();
}

void CPlayer::Tick()
{
	if(!Server()->ClientIngame(m_ClientID))
		return;

	Server()->SetClientScore(m_ClientID, m_Score);

	// do latency stuff
	{
		IServer::CClientInfo Info;

		if(Server()->GetClientInfo(m_ClientID, &Info))
		{
			m_Latency.m_Accum += Info.m_Latency;
			m_Latency.m_AccumMax = max(m_Latency.m_AccumMax, Info.m_Latency);
			m_Latency.m_AccumMin = min(m_Latency.m_AccumMin, Info.m_Latency);
		}
		
		// each second
		if(Server()->Tick()%Server()->TickSpeed() == 0)
		{
			m_Latency.m_Avg = m_Latency.m_Accum/Server()->TickSpeed();
			m_Latency.m_Max = m_Latency.m_AccumMax;
			m_Latency.m_Min = m_Latency.m_AccumMin;
			m_Latency.m_Accum = 0;
			m_Latency.m_AccumMin = 1000;
			m_Latency.m_AccumMax = 0;
		}
	}

	if(!GameServer()->m_World.m_Paused)
	{
		if(!m_pCharacter && m_Team == TEAM_SPECTATORS && m_SpectatorID == SPEC_FREEVIEW)
			m_ViewPos -= vec2(clamp(m_ViewPos.x-m_LatestActivity.m_TargetX, -500.0f, 500.0f), clamp(m_ViewPos.y-m_LatestActivity.m_TargetY, -400.0f, 400.0f));

		if(!m_pCharacter && m_DieTick+Server()->TickSpeed()*3 <= Server()->Tick())
			m_Spawning = true;

		if(m_pCharacter)
		{
			if(m_pCharacter->IsAlive())
			{
				m_ViewPos = m_pCharacter->m_Pos;
			}
			else
			{
				delete m_pCharacter;
				m_pCharacter = 0;
			}
		}
		else if(m_Spawning && !GameServer()->m_pController->IsGameOver() && m_RespawnTick <= Server()->Tick() && m_Team != TEAM_SPECTATORS)
			TryRespawn();
	}
	else
	{
		++m_RespawnTick;
		++m_DieTick;
		++m_ScoreStartTick;
		++m_LastActionTick;
		++m_TeamChangeTick;
 	}

	HandleTuningParams();
}

void CPlayer::PostTick()
{
	// update latency value
	if(m_PlayerFlags&PLAYERFLAG_SCOREBOARD)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
				m_aActLatency[i] = GameServer()->m_apPlayers[i]->m_Latency.m_Min;
		}
	}

	// update view pos for spectators
	if(m_Team == TEAM_SPECTATORS && m_SpectatorID != SPEC_FREEVIEW && GameServer()->m_apPlayers[m_SpectatorID])
		m_ViewPos = GameServer()->m_apPlayers[m_SpectatorID]->m_ViewPos;
}

void CPlayer::Snap(int SnappingClient)
{
	if(!Server()->ClientIngame(m_ClientID))
		return;

	int id = m_ClientID;
	if(!Server()->Translate(id, SnappingClient))
		return;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, id, sizeof(CNetObj_ClientInfo)));
	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, Server()->ClientName(m_ClientID));
	StrToInts(&pClientInfo->m_Clan0, 3, !m_pClass ? "????" : m_pClass->m_ClassName);
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_aSkinName);
	pClientInfo->m_Country = Server()->ClientCountry(m_ClientID);
	
	pClientInfo->m_UseCustomColor = GetClass() ? GetClass()->m_Skin.m_UseCustomColor : m_TeeInfos.m_UseCustomColor;
	pClientInfo->m_ColorBody = m_TeeInfos.m_ColorBody;
	pClientInfo->m_ColorFeet = m_TeeInfos.m_ColorFeet;

	IServer::CClientInfo Info;
	Server()->GetClientInfo(m_ClientID, &Info);

	int Latency = SnappingClient == -1 ? m_Latency.m_Min : GameServer()->m_apPlayers[SnappingClient]->m_aActLatency[m_ClientID];
	int Score = m_Score;

	if(!Server()->IsSixup(SnappingClient))
	{
		CNetObj_PlayerInfo *pPlayerInfo = static_cast<CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, sizeof(CNetObj_PlayerInfo)));
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_Latency = Latency;
		pPlayerInfo->m_Score = Score;
		pPlayerInfo->m_Local = (int)(m_ClientID == SnappingClient);
		pPlayerInfo->m_ClientID = id;
		pPlayerInfo->m_Team = m_Team;
	}
	else
	{
		protocol7::CNetObj_PlayerInfo *pPlayerInfo = static_cast<protocol7::CNetObj_PlayerInfo *>(Server()->SnapNewItem(NETOBJTYPE_PLAYERINFO, id, sizeof(protocol7::CNetObj_PlayerInfo)));
		if(!pPlayerInfo)
			return;

		pPlayerInfo->m_PlayerFlags = 0;
		if(m_PlayerFlags & PLAYERFLAG_SCOREBOARD)
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_SCOREBOARD;
		if(m_PlayerFlags & PLAYERFLAG_CHATTING)
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_CHATTING;

		if(Info.m_Authed)
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_ADMIN;

		if(SnappingClient != -1 && m_Team == TEAM_SPECTATORS && (SnappingClient == m_SpectatorID))
			pPlayerInfo->m_PlayerFlags |= protocol7::PLAYERFLAG_WATCHING;

		// Times are in milliseconds for 0.7
		pPlayerInfo->m_Score = Score;
		pPlayerInfo->m_Latency = Latency;
		
	}

	if(m_ClientID == SnappingClient && m_Team == TEAM_SPECTATORS)
	{
		if(!Server()->IsSixup(SnappingClient))
		{
			CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(CNetObj_SpectatorInfo)));
			if(!pSpectatorInfo)
				return;

			pSpectatorInfo->m_SpectatorID = m_SpectatorID;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
		else
		{
			protocol7::CNetObj_SpectatorInfo *pSpectatorInfo = static_cast<protocol7::CNetObj_SpectatorInfo *>(Server()->SnapNewItem(NETOBJTYPE_SPECTATORINFO, m_ClientID, sizeof(protocol7::CNetObj_SpectatorInfo)));
			if(!pSpectatorInfo)
				return;

			pSpectatorInfo->m_SpecMode = m_SpectatorID == SPEC_FREEVIEW ? protocol7::SPEC_FREEVIEW : protocol7::SPEC_PLAYER;
			pSpectatorInfo->m_SpectatorID = m_SpectatorID;
			pSpectatorInfo->m_X = m_ViewPos.x;
			pSpectatorInfo->m_Y = m_ViewPos.y;
		}
	}
}

void CPlayer::FakeSnap(int SnappingClient)
{
	IServer::CClientInfo info;
	Server()->GetClientInfo(SnappingClient, &info);

	if (Server()->IsSixup(SnappingClient))
		return;
	
	if (info.m_DDNetVersion > VERSION_DDNET_OLD)
		return;

	int id = VANILLA_MAX_CLIENTS - 1;

	CNetObj_ClientInfo *pClientInfo = static_cast<CNetObj_ClientInfo *>(Server()->SnapNewItem(NETOBJTYPE_CLIENTINFO, id, sizeof(CNetObj_ClientInfo)));

	if(!pClientInfo)
		return;

	StrToInts(&pClientInfo->m_Name0, 4, " ");
	StrToInts(&pClientInfo->m_Clan0, 3, Server()->ClientClan(m_ClientID));
	StrToInts(&pClientInfo->m_Skin0, 6, m_TeeInfos.m_aSkinName);
}

void CPlayer::OnDisconnect(const char *pReason)
{
	KillCharacter();

	if(Server()->ClientIngame(m_ClientID))
	{
		if(pReason && pReason[0])
			GameServer()->SendChatTarget_Localization(-1, _("'{str:Player}' has left the game ({str:Reason})"), 
				"Player", Server()->ClientName(m_ClientID),
				"Reason", pReason,
				NULL);
		else
			GameServer()->SendChatTarget_Localization(-1, _("'{str:Player}' has left the game"), 
				"Player", Server()->ClientName(m_ClientID),
				NULL);
		char aBuf[512];
		str_format(aBuf, sizeof(aBuf), "leave player='%d:%s'", m_ClientID, Server()->ClientName(m_ClientID));
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "game", aBuf);
	}
}

void CPlayer::OnPredictedInput(CNetObj_PlayerInput *NewInput)
{
	// skip the input if chat is active
	if((m_PlayerFlags&PLAYERFLAG_CHATTING) && (NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING))
		return;

	if(m_pCharacter)
		m_pCharacter->OnPredictedInput(NewInput);
}

void CPlayer::OnDirectInput(CNetObj_PlayerInput *NewInput)
{
	if(NewInput->m_PlayerFlags&PLAYERFLAG_CHATTING)
	{
		// skip the input if chat is active
		if(m_PlayerFlags&PLAYERFLAG_CHATTING)
			return;

		// reset input
		if(m_pCharacter)
			m_pCharacter->ResetInput();

		m_PlayerFlags = NewInput->m_PlayerFlags;
 		return;
	}

	m_PlayerFlags = NewInput->m_PlayerFlags;

	if(m_pCharacter)
		m_pCharacter->OnDirectInput(NewInput);

	if(!m_pCharacter && m_Team != TEAM_SPECTATORS && (NewInput->m_Fire&1))
		m_Spawning = true;

	// check for activity
	if(NewInput->m_Direction || m_LatestActivity.m_TargetX != NewInput->m_TargetX ||
		m_LatestActivity.m_TargetY != NewInput->m_TargetY || NewInput->m_Jump ||
		NewInput->m_Fire&1 || NewInput->m_Hook)
	{
		m_LatestActivity.m_TargetX = NewInput->m_TargetX;
		m_LatestActivity.m_TargetY = NewInput->m_TargetY;
		m_LastActionTick = Server()->Tick();
	}
}

CCharacter *CPlayer::GetCharacter()
{
	if(m_pCharacter && m_pCharacter->IsAlive())
		return m_pCharacter;
	return 0;
}

void CPlayer::KillCharacter(int Weapon)
{
	if(m_pCharacter)
	{
		m_pCharacter->Die(m_ClientID, Weapon);
		
		delete m_pCharacter;
		m_pCharacter = 0;
	}
}

void CPlayer::Respawn()
{
	if(m_Team != TEAM_SPECTATORS)
		m_Spawning = true;
}

void CPlayer::SetTeam(int Team, bool DoChatMsg)
{
	// clamp the team
	Team = GameServer()->m_pController->ClampTeam(Team);
	if(m_Team == Team)
		return;

	char aBuf[512];
	if(DoChatMsg)
	{
		GameServer()->SendChatTarget_Localization(-1, _("'{str:Player}' joined the {lstr:Team}"), 
			"Player", Server()->ClientName(m_ClientID), 
			"Team", GameServer()->m_pController->GetTeamName(Team),
			NULL);
	}

	if(Team == TEAM_SPECTATORS)
		Server()->SetClientSpec(m_ClientID, true);
	else 
		Server()->SetClientSpec(m_ClientID, false);

	if(Team != m_Team)
		KillCharacter();

	m_Team = Team;
	m_LastActionTick = Server()->Tick();
	m_SpectatorID = SPEC_FREEVIEW;
	// we got to wait 0.5 secs before respawning
	m_RespawnTick = Server()->Tick()+Server()->TickSpeed()/2;
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' m_Team=%d", m_ClientID, Server()->ClientName(m_ClientID), m_Team);
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	GameServer()->m_pController->OnPlayerInfoChange(GameServer()->m_apPlayers[m_ClientID]);

	if(Team == TEAM_SPECTATORS)
	{
		// update spectator modes
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->m_SpectatorID == m_ClientID)
				GameServer()->m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
		}
	}

	protocol7::CNetMsg_Sv_Team Msg;
	Msg.m_ClientID = m_ClientID;
	Msg.m_Team = m_Team;
	Msg.m_Silent = 1;
	Msg.m_CooldownTick = m_LastSetTeam + Server()->TickSpeed() * 3;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);
}

void CPlayer::TryRespawn()
{
	vec2 SpawnPos;

	if(m_ChangeClass)
		Infect();

	if(!GameServer()->m_pController->PreSpawn(this, &SpawnPos))
		return;
		
	m_Spawning = false;
	m_pCharacter = new(m_ClientID) CCharacter(&GameServer()->m_World);
	m_pCharacter->Spawn(this, SpawnPos);
	GameServer()->CreatePlayerSpawn(SpawnPos);
}

const char* CPlayer::GetLanguage()
{
	return Server()->GetClientLanguage(m_ClientID);
}

void CPlayer::SetLanguage(const char* pLanguage)
{
	Server()->SetClientLanguage(m_ClientID, pLanguage);
}

bool CPlayer::IsHuman() const
{
	if(!m_pClass)
		return 1;
	
	return !m_pClass->m_Infect;
}

bool CPlayer::IsInfect() const
{
	if(!m_pClass)
		return 0;
	
	return m_pClass->m_Infect;
}

void CPlayer::SetClass(CClass *pClass)
{
	m_ChangeClass = 0;

	if(m_pClass)
		delete m_pClass;
	m_pClass = 0;

	m_pClass = pClass;
	m_TeeInfos = m_pClass->m_Skin;
	
	for (auto each : m_pGameServer->m_apPlayers) 
	{
		if (each) 
		{
			m_pGameServer->SendSkinChange(GetCID(), each->GetCID());
			m_pGameServer->SendClanChange(GetCID(), each->GetCID(), m_pClass->m_ClassName);
		}
	}
	
	GameServer()->SendBroadcast_Localization(m_ClientID, _("You are '{lstr:Class}'"), 150, BROADCAST_CLASS,
		"Class", pClass->m_ClassName,
		NULL);

	if(m_pCharacter)
	{
		GameServer()->CreatePlayerSpawn(m_pCharacter->m_Pos);
		m_pCharacter->InitClassWeapon();
		m_pCharacter->GetCore()->m_Infect = pClass->m_Infect;
		m_pCharacter->DestroyChildEntites();
	}

	Server()->ExpireServerInfo();
			
	protocol7::CNetMsg_Sv_GameInfo GameInfoMsg;
	GameInfoMsg.m_GameFlags = 0;
	GameInfoMsg.m_ScoreLimit = g_Config.m_SvScorelimit;
	GameInfoMsg.m_TimeLimit = g_Config.m_SvTimelimit;
	GameInfoMsg.m_MatchNum = (str_length(g_Config.m_SvMaprotation) && g_Config.m_SvRoundsPerMap) ? g_Config.m_SvRoundsPerMap : 0;
	GameInfoMsg.m_MatchCurrent = GameServer()->m_pController->m_RoundCount + 1;

	protocol7::CNetMsg_Sv_GameInfo *pInfoMsg = &GameInfoMsg;
	Server()->SendPackMsg(pInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, m_ClientID);
}

void CPlayer::CureToDefault()
{
	m_ChangeClass = 0;

	{
		str_copy(m_TeeInfos.m_aSkinName, "default");
		m_TeeInfos.m_UseCustomColor = 0;

		for(int p = 0; p < 6; p++)
		{
			str_copy(m_TeeInfos.m_apSkinPartNames[p], g_DefaultSkin.m_apSkinPartNames[p], 24);
			m_TeeInfos.m_aUseCustomColors[p] = g_DefaultSkin.m_aUseCustomColors[p];
			m_TeeInfos.m_aSkinPartColors[p] = g_DefaultSkin.m_aSkinPartColors[p];
		}
	}
	if(m_pClass)
		delete m_pClass;
	m_pClass = 0;
	
	for (auto each : m_pGameServer->m_apPlayers) 
	{
		if (each) 
		{
			m_pGameServer->SendSkinChange(GetCID(), each->GetCID());
			m_pGameServer->SendClanChange(GetCID(), each->GetCID(), "????");
		}
	}
	
	if(m_pCharacter)
	{
		GameServer()->CreatePlayerSpawn(m_pCharacter->m_Pos);
		m_pCharacter->InitClassWeapon();
		m_pCharacter->GetCore()->m_Infect = false;
		m_pCharacter->DestroyChildEntites();
	}

	Server()->ExpireServerInfo();
			
	protocol7::CNetMsg_Sv_GameInfo GameInfoMsg;
	GameInfoMsg.m_GameFlags = 0;
	GameInfoMsg.m_ScoreLimit = g_Config.m_SvScorelimit;
	GameInfoMsg.m_TimeLimit = g_Config.m_SvTimelimit;
	GameInfoMsg.m_MatchNum = (str_length(g_Config.m_SvMaprotation) && g_Config.m_SvRoundsPerMap) ? g_Config.m_SvRoundsPerMap : 0;
	GameInfoMsg.m_MatchCurrent = GameServer()->m_pController->m_RoundCount + 1;

	protocol7::CNetMsg_Sv_GameInfo *pInfoMsg = &GameInfoMsg;
	Server()->SendPackMsg(pInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, m_ClientID);
}

void CPlayer::Infect()
{
	if(IsInfect() && !m_ChangeClass)
		return;
	
	if(m_pCharacter)
	{
		GameServer()->CreateSound(m_pCharacter->m_Pos, SOUND_PLAYER_PAIN_SHORT);
	}

	SetClass(GameServer()->m_pController->OnPlayerInfect(this));
}