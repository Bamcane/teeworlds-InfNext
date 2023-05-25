/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <vector>

#include <new>
#include <base/math.h>
#include <engine/shared/config.h>
#include <engine/map.h>
#include <engine/console.h>
#include "gamecontext.h"
#include <game/version.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <engine/shared/json.h>
#include <engine/shared/linereader.h>

#include <game/generated/protocol7.h>
#include <game/generated/protocolglue.h>

#include <teeuniverses/components/localization.h>

#include "gamemodes/infnext.h"

enum
{
	RESET,
	NO_RESET
};

void CGameContext::Construct(int Resetting)
{
	m_Resetting = 0;
	m_pServer = 0;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_apPlayers[i] = 0;
		m_aBroadcast[i].m_aBroadcast.clear();
	}
	m_pController = 0;
	m_VoteCloseTime = 0;
	m_pVoteOptionFirst = 0;
	m_pVoteOptionLast = 0;
	m_NumVoteOptions = 0;
	m_LockTeams = 0;
	m_ChatResponseTargetID = -1;

	if(Resetting==NO_RESET)
		m_pVoteOptionHeap = new CHeap();
}

CGameContext::CGameContext(int Resetting)
{
	Construct(Resetting);
}

CGameContext::CGameContext()
{
	Construct(NO_RESET);
}

CGameContext::~CGameContext()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		delete m_apPlayers[i];
	if(!m_Resetting)
		delete m_pVoteOptionHeap;
}

void CGameContext::OnSetAuthed(int ClientID, int Level)
{
	if(m_apPlayers[ClientID])
		m_apPlayers[ClientID]->m_Authed = Level;
}

void CGameContext::Clear()
{
	CHeap *pVoteOptionHeap = m_pVoteOptionHeap;
	CVoteOptionServer *pVoteOptionFirst = m_pVoteOptionFirst;
	CVoteOptionServer *pVoteOptionLast = m_pVoteOptionLast;
	int NumVoteOptions = m_NumVoteOptions;
	CTuningParams Tuning = m_Tuning;

	m_Resetting = true;
	this->~CGameContext();
	mem_zero(this, sizeof(*this));
	new (this) CGameContext(RESET);

	m_pVoteOptionHeap = pVoteOptionHeap;
	m_pVoteOptionFirst = pVoteOptionFirst;
	m_pVoteOptionLast = pVoteOptionLast;
	m_NumVoteOptions = NumVoteOptions;
	m_Tuning = Tuning;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aBroadcast[i].m_aBroadcast.clear();
	}
}


class CCharacter *CGameContext::GetPlayerChar(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || !m_apPlayers[ClientID])
		return 0;
	return m_apPlayers[ClientID]->GetCharacter();
}

void CGameContext::CreateDamageInd(vec2 Pos, float Angle, int Amount, CClientMask Mask)
{
	float a = 3 * 3.14159f / 2 + Angle;
	//float a = get_angle(dir);
	float s = a-pi/3;
	float e = a+pi/3;
	for(int i = 0; i < Amount; i++)
	{
		float f = mix(s, e, float(i+1)/float(Amount+2));
		CNetEvent_DamageInd *pEvent = (CNetEvent_DamageInd *)m_Events.Create(NETEVENTTYPE_DAMAGEIND, sizeof(CNetEvent_DamageInd), Mask);
		if(pEvent)
		{
			pEvent->m_X = (int)Pos.x;
			pEvent->m_Y = (int)Pos.y;
			pEvent->m_Angle = (int)(f*256.0f);
		}
	}
}

void CGameContext::CreateHammerHit(vec2 Pos, CClientMask Mask)
{
	// create the event
	CNetEvent_HammerHit *pEvent = (CNetEvent_HammerHit *)m_Events.Create(NETEVENTTYPE_HAMMERHIT, sizeof(CNetEvent_HammerHit), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}
}


void CGameContext::CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, CClientMask Mask, int DamageMode)
{
	// create the event
	CNetEvent_Explosion *pEvent = (CNetEvent_Explosion *)m_Events.Create(NETEVENTTYPE_EXPLOSION, sizeof(CNetEvent_Explosion), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
	}

	if (!NoDamage)
	{
		// deal damage
		CCharacter *apEnts[MAX_CLIENTS];
		float Radius = 135.0f;
		float InnerRadius = 48.0f;
		int Num = m_World.FindEntities(Pos, Radius, (CEntity**)apEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++)
		{
			vec2 Diff = apEnts[i]->m_Pos - Pos;
			vec2 ForceDir(0,1);
			float l = length(Diff);
			if(l)
				ForceDir = normalize(Diff);
			l = 1-clamp((l-InnerRadius)/(Radius-InnerRadius), 0.0f, 1.0f);
			float Dmg = 6 * l;
			if((int)Dmg)
				apEnts[i]->TakeDamage(ForceDir*Dmg*2, (int)Dmg, Owner, Weapon, DamageMode);
		}
	}
}

void CGameContext::CreatePlayerSpawn(vec2 Pos, CClientMask Mask)
{
	// create the event
	CNetEvent_Spawn *ev = (CNetEvent_Spawn *)m_Events.Create(NETEVENTTYPE_SPAWN, sizeof(CNetEvent_Spawn), Mask);
	if(ev)
	{
		ev->m_X = (int)Pos.x;
		ev->m_Y = (int)Pos.y;
	}
}

void CGameContext::CreateDeath(vec2 Pos, int ClientID, CClientMask Mask)
{
	// create the event
	CNetEvent_Death *pEvent = (CNetEvent_Death *)m_Events.Create(NETEVENTTYPE_DEATH, sizeof(CNetEvent_Death), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_ClientID = ClientID;
	}
}

void CGameContext::CreateSound(vec2 Pos, int Sound, CClientMask Mask)
{
	if (Sound < 0)
		return;

	// create a sound
	CNetEvent_SoundWorld *pEvent = (CNetEvent_SoundWorld *)m_Events.Create(NETEVENTTYPE_SOUNDWORLD, sizeof(CNetEvent_SoundWorld), Mask);
	if(pEvent)
	{
		pEvent->m_X = (int)Pos.x;
		pEvent->m_Y = (int)Pos.y;
		pEvent->m_SoundID = Sound;
	}
}

void CGameContext::CreateSoundGlobal(int Sound, int Target)
{
	if (Sound < 0)
		return;

	CNetMsg_Sv_SoundGlobal Msg;
	Msg.m_SoundID = Sound;
	if(Target == -2)
		Server()->SendPackMsg(&Msg, MSGFLAG_NOSEND, -1);
	else
	{
		int Flag = MSGFLAG_VITAL;
		if(Target != -1)
			Flag |= MSGFLAG_NORECORD;
		Server()->SendPackMsg(&Msg, Flag, Target);
	}
}

void CGameContext::SendMotd(int To, const char* pText)
{
	if(m_apPlayers[To])
	{
		CNetMsg_Sv_Motd Msg;
		
		Msg.m_pMessage = pText;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
	}
}

void CGameContext::SendSettings(int ClientID)
{
	if(Server()->IsSixup(ClientID))
	{
		protocol7::CNetMsg_Sv_ServerSettings Msg;
		Msg.m_KickVote = g_Config.m_SvVoteKick;
		Msg.m_KickMin = g_Config.m_SvVoteKickMin;
		Msg.m_SpecVote = g_Config.m_SvVoteSpectate;
		Msg.m_TeamLock = 0;
		Msg.m_TeamBalance = 0;
		Msg.m_PlayerSlots = g_Config.m_SvMaxClients - g_Config.m_SvSpectatorSlots;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
}

void CGameContext::SendChatTarget(int To, const char* pText)
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Team = 0;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;

	if(To == -1)
	{
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
		}
	}
	else
	{
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, To);
	}
}

void CGameContext::SendChatTarget_Localization(int To, const char *pText, ...)
{
	int Start = (To < 0 ? 0 : To);
	int End = (To < 0 ? MAX_CLIENTS : To+1);
	
	dynamic_string Buffer;
	
	va_list VarArgs;
	va_start(VarArgs, pText);
	
	for(int i = Start; i < End; i++)
	{
		if(m_apPlayers[i])
		{
			Buffer.clear();
			Server()->Localization()->Format_VL(Buffer, m_apPlayers[i]->GetLanguage(), pText, VarArgs);
			
			SendChatTarget(i, Buffer.buffer());
		}
	}
	
	va_end(VarArgs);
}

void CGameContext::SendChatTarget_Localization_P(int To, const char* pText, int Number, ...)
{
	int Start = (To < 0 ? 0 : To);
	int End = (To < 0 ? MAX_CLIENTS : To+1);
	
	dynamic_string Buffer;
	
	va_list VarArgs;
	va_start(VarArgs, Number);
	
	for(int i = Start; i < End; i++)
	{
		if(m_apPlayers[i])
		{
			Buffer.clear();
			Server()->Localization()->Format_VLP(Buffer, m_apPlayers[i]->GetLanguage(), Number, pText, VarArgs);
			
			SendChatTarget(To, Buffer.buffer());
		}
	}
	
	va_end(VarArgs);
}

void CGameContext::SendChat(int ChatterClientID, int Team, const char *pText)
{
	char aBuf[256];
	if(ChatterClientID >= 0 && ChatterClientID < MAX_CLIENTS)
		str_format(aBuf, sizeof(aBuf), "%d:%d:%s: %s", ChatterClientID, Team, Server()->ClientName(ChatterClientID), pText);
	else
		str_format(aBuf, sizeof(aBuf), "*** %s", pText);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, Team!=CHAT_ALL?"teamchat":"chat", aBuf);

	if(Team == CHAT_ALL)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 0;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;
		// send to the clients
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(!m_apPlayers[i])
				continue;
				
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
		}
	}
	else
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = 1;
		Msg.m_ClientID = ChatterClientID;
		Msg.m_pMessage = pText;

		// pack one for the recording only
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NOSEND, -1);

		// send to the clients
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() == Team)
				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, i);
		}
	}
}

void CGameContext::SendEmoticon(int ClientID, int Emoticon)
{
	CNetMsg_Sv_Emoticon Msg;
	Msg.m_ClientID = ClientID;
	Msg.m_Emoticon = Emoticon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, -1);
}

void CGameContext::SendWeaponPickup(int ClientID, int Weapon)
{
	CNetMsg_Sv_WeaponPickup Msg;
	Msg.m_Weapon = Weapon;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::SendBroadcast(int ClientID, const char *pText, int Time, int Type)
{
	int Start = (ClientID < 0 ? 0 : ClientID);
	int End = (ClientID < 0 ? MAX_CLIENTS : ClientID+1);

	// only for server demo record
	if(ClientID < 0)
	{
		CNetMsg_Sv_Broadcast Msg;
		Msg.m_pMessage = pText;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NOSEND, -1);
	}

	for(int i = Start; i < End;i ++)
	{
		if(m_apPlayers[i])
		{
			CBroadcast Broadcast;

			Broadcast.m_Text = pText;
			Broadcast.m_Time = Time;
			Broadcast.m_Type = Type;
			Broadcast.m_LastBroadcast = 0;

			AddBroadcast(i, Broadcast);
		}
	}
}

void CGameContext::SendClanChange(int ClientID, int TargetID, const char *pClan)
{
	if (!m_apPlayers[ClientID])
	  	return;
	if (ClientID == TargetID)
	  	return;

	if((Server()->IsSixup(TargetID)))
	{
		protocol7::CNetMsg_Sv_ClientDrop Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_pReason = "a";
		Msg.m_Silent = true;
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, TargetID);
		
		protocol7::CNetMsg_Sv_ClientInfo NewClientInfoMsg;
		NewClientInfoMsg.m_ClientID = ClientID;
		NewClientInfoMsg.m_Local = 0; 
		NewClientInfoMsg.m_Team = m_apPlayers[ClientID]->GetTeam();
		NewClientInfoMsg.m_pName = Server()->ClientName(ClientID);
		NewClientInfoMsg.m_pClan = pClan;
		NewClientInfoMsg.m_Country = Server()->ClientCountry(ClientID);
		NewClientInfoMsg.m_Silent = true;

		//TBD
		int NUM_SKINPARTS = 6;
		for(int p = 0; p < NUM_SKINPARTS; p++)
		{
			NewClientInfoMsg.m_apSkinPartNames[p] = m_apPlayers[ClientID]->m_TeeInfos.m_apSkinPartNames[p];
			NewClientInfoMsg.m_aUseCustomColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aUseCustomColors[p];
			NewClientInfoMsg.m_aSkinPartColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aSkinPartColors[p];
		}

		Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL|MSGFLAG_NORECORD, TargetID);

	}
	
}

void CGameContext::SendSkinChange(int ClientID, int TargetID)
{
	if(!(Server()->IsSixup(TargetID)))
	  	return;
    if(!m_apPlayers[ClientID])
	  	return;
	
	protocol7::CNetMsg_Sv_SkinChange Msg;
	Msg.m_ClientID = ClientID;
	const int NUM_SKINPARTS = 6; // TBD
	for(int p = 0; p < NUM_SKINPARTS; p++)
	{
		if (p == (NUM_SKINPARTS - 1)) {
			Msg.m_apSkinPartNames[p] = m_apPlayers[ClientID]->m_TeeInfos.m_apSkinPartNames[p];
		} else {
			Msg.m_apSkinPartNames[p] = m_apPlayers[ClientID]->m_TeeInfos.m_apSkinPartNames[p];
		}
		Msg.m_aUseCustomColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aUseCustomColors[p];
		Msg.m_aSkinPartColors[p] = m_apPlayers[ClientID]->m_TeeInfos.m_aSkinPartColors[p];
	}
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, TargetID);
}

void CGameContext::AddBroadcast(int ClientID, CBroadcast Broadcast)
{
	if(!m_apPlayers[ClientID])
		return;
	
	// only for server demo record
	if(ClientID < 0)
	{
		CNetMsg_Sv_Broadcast Msg;
		Msg.m_pMessage = Broadcast.m_Text.c_str();
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NOSEND, -1);
	}
	
	for(int i = 0; i < (int) m_aBroadcast[ClientID].m_aBroadcast.size();i ++)
	{
		if(	m_aBroadcast[ClientID].m_aBroadcast[i].m_Type == Broadcast.m_Type )
		{
			if( str_comp(m_aBroadcast[ClientID].m_aBroadcast[i].m_Text.c_str(), Broadcast.m_Text.c_str()) == 0)
				Broadcast.m_LastBroadcast = m_aBroadcast[ClientID].m_aBroadcast[i].m_LastBroadcast;
			m_aBroadcast[ClientID].m_aBroadcast[i] = Broadcast;
			return;
		}
	}
			
	m_aBroadcast[ClientID].m_aBroadcast.push_back(Broadcast);
}

void CGameContext::SendBroadcast_Localization(int ClientID, const char *pText, int Time, int Type, ...)
{
	int Start = (ClientID < 0 ? 0 : ClientID);
	int End = (ClientID < 0 ? MAX_CLIENTS : ClientID+1);
	
	dynamic_string Buffer;
	
	va_list VarArgs;
	va_start(VarArgs, Type);

	for(int i = Start; i < End; i++)
	{
		if(m_apPlayers[i])
		{
			Buffer.clear();
			Server()->Localization()->Format_VL(Buffer, m_apPlayers[i]->GetLanguage(), _(pText), VarArgs);
			
			CBroadcast Broadcast;

			Broadcast.m_Text = Buffer.buffer();
			Broadcast.m_Time = Time;
			Broadcast.m_Type = Type;
			Broadcast.m_LastBroadcast = 0;

			AddBroadcast(i, Broadcast);
		}
	}
	
	va_end(VarArgs);
}

void CGameContext::SendBroadcast_Localization_P(int ClientID, const char* pText, int Time, int Type, int Number, ...)
{
	int Start = (ClientID < 0 ? 0 : ClientID);
	int End = (ClientID < 0 ? MAX_CLIENTS : ClientID+1);
	
	dynamic_string Buffer;
	
	va_list VarArgs;
	va_start(VarArgs, Number);
	
	for(int i = Start; i < End; i++)
	{
		if(m_apPlayers[i])
		{
			Buffer.clear();
			Server()->Localization()->Format_VLP(Buffer, m_apPlayers[i]->GetLanguage(), Number, pText, VarArgs);
			
			CBroadcast Broadcast;

			Broadcast.m_Text = Buffer.buffer();
			Broadcast.m_Time = Time;
			Broadcast.m_Type = Type;
			Broadcast.m_LastBroadcast = 0;
			AddBroadcast(i, Broadcast);
		}
	}
	
	va_end(VarArgs);
}

//
void CGameContext::StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSixupDesc)
{
	// check if a vote is already running
	if(m_VoteCloseTime)
		return;

	// reset votes
	m_VoteEnforce = VOTE_ENFORCE_UNKNOWN;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->m_Vote = 0;
			m_apPlayers[i]->m_VotePos = 0;
		}
	}

	// start vote
	m_VoteCloseTime = time_get() + time_freq()*25;
	str_copy(m_aVoteDescription, pDesc);
	str_copy(m_aSixupVoteDescription, pSixupDesc, sizeof(m_aSixupVoteDescription));
	str_copy(m_aVoteCommand, pCommand);
	str_copy(m_aVoteReason, pReason);
	SendVoteSet(-1);
	m_VoteUpdate = true;
}


void CGameContext::EndVote()
{
	m_VoteCloseTime = 0;
	SendVoteSet(-1);
}

void CGameContext::SendVoteSet(int ClientID)
{
	::CNetMsg_Sv_VoteSet Msg6;
	protocol7::CNetMsg_Sv_VoteSet Msg7;

	Msg7.m_ClientID = m_VoteCreator;
	if(m_VoteCloseTime)
	{
		Msg6.m_Timeout = Msg7.m_Timeout = (m_VoteCloseTime - time_get()) / time_freq();
		Msg6.m_pDescription = m_aVoteDescription;
		Msg7.m_pDescription = m_aSixupVoteDescription;
		Msg6.m_pReason = Msg7.m_pReason = m_aVoteReason;

		int &Type = (Msg7.m_Type = protocol7::VOTE_UNKNOWN);
		if(IsKickVote())
			Type = protocol7::VOTE_START_KICK;
		else if(IsSpecVote())
			Type = protocol7::VOTE_START_SPEC;
		else if(IsOptionVote())
			Type = protocol7::VOTE_START_OP;
	}
	else
	{
		Msg6.m_Timeout = Msg7.m_Timeout = 0;
		Msg6.m_pDescription = Msg7.m_pDescription = "";
		Msg6.m_pReason = Msg7.m_pReason = "";

		int &Type = (Msg7.m_Type = protocol7::VOTE_UNKNOWN);
		if(m_VoteEnforce == VOTE_ENFORCE_NO || m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN)
			Type = protocol7::VOTE_END_FAIL;
		else if(m_VoteEnforce == VOTE_ENFORCE_YES || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Type = protocol7::VOTE_END_PASS;
		else if(m_VoteEnforce == VOTE_ENFORCE_ABORT)
			Type = protocol7::VOTE_END_ABORT;

		if(m_VoteEnforce == VOTE_ENFORCE_NO_ADMIN || m_VoteEnforce == VOTE_ENFORCE_YES_ADMIN)
			Msg7.m_ClientID = -1;
	}

	if(ClientID == -1)
	{
		for(int i = 0; i < Server()->MaxClients(); i++)
		{
			if(!m_apPlayers[i])
				continue;
			if(!Server()->IsSixup(i))
				Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, i);
			else
				Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, i);
		}
	}
	else
	{
		if(!Server()->IsSixup(ClientID))
			Server()->SendPackMsg(&Msg6, MSGFLAG_VITAL, ClientID);
		else
			Server()->SendPackMsg(&Msg7, MSGFLAG_VITAL, ClientID);
	}
}

void CGameContext::SendVoteStatus(int ClientID, int Total, int Yes, int No)
{
	CNetMsg_Sv_VoteStatus Msg = {0};
	Msg.m_Total = Total;
	Msg.m_Yes = Yes;
	Msg.m_No = No;
	Msg.m_Pass = Total - (Yes+No);

	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);

}

void CGameContext::AbortVoteKickOnDisconnect(int ClientID)
{
	if(m_VoteCloseTime && ((!str_comp_num(m_aVoteCommand, "kick ", 5) && str_toint(&m_aVoteCommand[5]) == ClientID) ||
		(!str_comp_num(m_aVoteCommand, "set_team ", 9) && str_toint(&m_aVoteCommand[9]) == ClientID)))
		m_VoteCloseTime = -1;
}


void CGameContext::CheckPureTuning()
{
	// might not be created yet during start up
	if(!m_pController)
		return;
}

void CGameContext::SendTuningParams(int ClientID)
{
	CheckPureTuning();

	CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
	int *pParams = (int *)&m_Tuning;
	for(unsigned i = 0; i < sizeof(m_Tuning)/sizeof(int); i++)
	{
		if(i == 30 && Server()->IsSixup(ClientID)) // laser damage
			continue;
		Msg.AddInt(pParams[i]);
	}
	Server()->SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameContext::OnTick()
{
	// check tuning
	CheckPureTuning();

	// copy tuning
	m_World.m_Core.m_Tuning = m_Tuning;
	m_World.Tick();

	//if(world.paused) // make sure that the game object always updates
	m_pController->Tick();

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
		{
			m_apPlayers[i]->Tick();
			m_apPlayers[i]->PostTick();
		}
	}

	{
		for(int i = 0; i < MAX_CLIENTS;i ++)
		{
			if(!m_apPlayers[i] && m_aBroadcast[i].m_aBroadcast.size())
			{
				m_aBroadcast[i].m_aBroadcast.clear();
				continue;
			}

			bool Broadcast = false;

			for(int j = 0; j < (int) m_aBroadcast[i].m_aBroadcast.size(); j ++)
			{
				// don't break!!!!Must remove all need remove broadcast!
				if(m_aBroadcast[i].m_aBroadcast[j].m_Time-- <= 0)
				{
					m_aBroadcast[i].m_aBroadcast.erase(m_aBroadcast[i].m_aBroadcast.begin() + j);
					Broadcast = true;
					// continue;
					continue;
				}

				if(m_aBroadcast[i].m_aBroadcast[j].m_LastBroadcast == 0 || (Server()->Tick()-m_aBroadcast[i].m_aBroadcast[j].m_LastBroadcast)%50 == 0)
				{
					Broadcast = true;
				}
			}

			if(Broadcast)
			{
				UpdateBroadcast(i);
			}
		}
	}

	// update voting
	if(m_VoteCloseTime)
	{
		// abort the kick-vote on player-leave
		if(m_VoteCloseTime == -1)
		{
			SendChatTarget_Localization(-1, _("Vote aborted"));
			EndVote();
		}
		else
		{
			int Total = 0, Yes = 0, No = 0;
			if(m_VoteUpdate)
			{
				// count votes
				char aaBuf[MAX_CLIENTS][NETADDR_MAXSTRSIZE] = {{0}};
				for(int i = 0; i < MAX_CLIENTS; i++)
					if(m_apPlayers[i])
						Server()->GetClientAddr(i, aaBuf[i], NETADDR_MAXSTRSIZE);
				bool aVoteChecked[MAX_CLIENTS] = {0};
				for(int i = 0; i < MAX_CLIENTS; i++)
				{
					if(!m_apPlayers[i] || m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS || aVoteChecked[i])	// don't count in votes by spectators
						continue;

					int ActVote = m_apPlayers[i]->m_Vote;
					int ActVotePos = m_apPlayers[i]->m_VotePos;

					// check for more players with the same ip (only use the vote of the one who voted first)
					for(int j = i+1; j < MAX_CLIENTS; ++j)
					{
						if(!m_apPlayers[j] || aVoteChecked[j] || str_comp(aaBuf[j], aaBuf[i]))
							continue;

						aVoteChecked[j] = true;
						if(m_apPlayers[j]->m_Vote && (!ActVote || ActVotePos > m_apPlayers[j]->m_VotePos))
						{
							ActVote = m_apPlayers[j]->m_Vote;
							ActVotePos = m_apPlayers[j]->m_VotePos;
						}
					}

					Total++;
					if(ActVote > 0)
						Yes++;
					else if(ActVote < 0)
						No++;
				}

				if(Yes >= Total/2+1)
					m_VoteEnforce = VOTE_ENFORCE_YES;
				else if(No >= (Total+1)/2)
					m_VoteEnforce = VOTE_ENFORCE_NO;
			}

			if(m_VoteEnforce == VOTE_ENFORCE_YES)
			{
				Server()->SetRconCID(IServer::RCON_CID_VOTE);
				Console()->ExecuteLine(m_aVoteCommand, -1);
				Server()->SetRconCID(IServer::RCON_CID_SERV);
				EndVote();
				SendChatTarget_Localization(-1, _("Vote passed"));

				if(m_apPlayers[m_VoteCreator])
					m_apPlayers[m_VoteCreator]->m_LastVoteCall = 0;
			}
			else if(m_VoteEnforce == VOTE_ENFORCE_NO || time_get() > m_VoteCloseTime)
			{
				EndVote();
				SendChatTarget_Localization(-1, _("Vote failed"));
			}
			else if(m_VoteUpdate)
			{
				m_VoteUpdate = false;
				SendVoteStatus(-1, Total, Yes, No);
			}
		}
	}


#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		for(int i = 0; i < g_Config.m_DbgDummies ; i++)
		{
			CNetObj_PlayerInput Input = {0};
			Input.m_Direction = (i&1)?-1:1;
			m_apPlayers[MAX_CLIENTS-i-1]->OnPredictedInput(&Input);
		}
	}
#endif
}

// Server hooks
static int PlayerFlags_SevenToSix(int Flags)
{
	int Six = 0;
	if(Flags & protocol7::PLAYERFLAG_CHATTING)
		Six |= PLAYERFLAG_CHATTING;
	if(Flags & protocol7::PLAYERFLAG_SCOREBOARD)
		Six |= PLAYERFLAG_SCOREBOARD;
	if(Flags & protocol7::PLAYERFLAG_AIM)
		Six |= PLAYERFLAG_AIM;
	return Six;
}

void CGameContext::OnClientPrepareInput(int ClientID, void *pInput)
{
	auto *pPlayerInput = (CNetObj_PlayerInput *)pInput;
	if(Server()->IsSixup(ClientID))
		pPlayerInput->m_PlayerFlags = PlayerFlags_SevenToSix(pPlayerInput->m_PlayerFlags);
}

void CGameContext::OnClientDirectInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnDirectInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientPredictedInput(int ClientID, void *pInput)
{
	if(!m_World.m_Paused)
		m_apPlayers[ClientID]->OnPredictedInput((CNetObj_PlayerInput *)pInput);
}

void CGameContext::OnClientEnter(int ClientID)
{
	//world.insert_entity(&players[client_id]);
	if (m_pController->IsInfectionStarted())
		m_apPlayers[ClientID]->Infect();
	else m_apPlayers[ClientID]->CureToDefault();
	
	m_apPlayers[ClientID]->Respawn();
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "team_join player='%d:%s' team=%d", ClientID, Server()->ClientName(ClientID), m_apPlayers[ClientID]->GetTeam());
	Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

	
	if(Server()->IsSixup(ClientID))
	{
		{
			protocol7::CNetMsg_Sv_GameInfo Msg;
			Msg.m_GameFlags = protocol7::GAMEFLAG_RACE;
			Msg.m_MatchCurrent = 1;
			Msg.m_MatchNum = 0;
			Msg.m_ScoreLimit = 0;
			Msg.m_TimeLimit = 0;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}

		// /team is essential
		{
			protocol7::CNetMsg_Sv_CommandInfoRemove Msg;
			Msg.m_pName = "team";
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}

		for(const IConsole::CCommandInfo *pCmd = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT);
			pCmd; pCmd = pCmd->NextCommandInfo(IConsole::ACCESS_LEVEL_USER, CFGFLAG_CHAT))
		{
			if(!str_comp_nocase(pCmd->m_pName, "w") || !str_comp_nocase(pCmd->m_pName, "whisper"))
				continue;
				
			const char *pName = pCmd->m_pName;

			protocol7::CNetMsg_Sv_CommandInfo Msg;
			Msg.m_pName = pName;
			Msg.m_pArgsFormat = pCmd->m_pParams;
			Msg.m_pHelpText = pCmd->m_pHelp;
			Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}
	}
	
	SendChatTarget_Localization(-1, _("'{str:Player}' entered and joined the {lstr:Team}"), 
		"Player", Server()->ClientName(ClientID), 
		"Team", m_pController->GetTeamName(m_apPlayers[ClientID]->GetTeam()),
		NULL);

	SendChatTarget_Localization(ClientID, _("Help us to translation mod on Crowdin!"));
	SendChatTarget(ClientID, "Link: https://crowdin.com/project/teeworlds-infnext");

	Server()->ExpireServerInfo();
	CPlayer *pNewPlayer = m_apPlayers[ClientID];

	// new info for others
	protocol7::CNetMsg_Sv_ClientInfo NewClientInfoMsg;
	NewClientInfoMsg.m_ClientID = ClientID;
	NewClientInfoMsg.m_Local = 0;
	NewClientInfoMsg.m_Team = pNewPlayer->GetTeam();
	NewClientInfoMsg.m_pName = Server()->ClientName(ClientID);
	NewClientInfoMsg.m_pClan = Server()->ClientClan(ClientID);
	NewClientInfoMsg.m_Country = Server()->ClientCountry(ClientID);
	NewClientInfoMsg.m_Silent = 1;

	for(int p = 0; p < 6; p++)
	{
		NewClientInfoMsg.m_apSkinPartNames[p] = pNewPlayer->m_TeeInfos.m_apSkinPartNames[p];
		NewClientInfoMsg.m_aUseCustomColors[p] = pNewPlayer->m_TeeInfos.m_aUseCustomColors[p];
		NewClientInfoMsg.m_aSkinPartColors[p] = pNewPlayer->m_TeeInfos.m_aSkinPartColors[p];
	}

	// update client infos (others before local)
	for(int i = 0; i < Server()->MaxClients(); ++i)
	{
		if(i == ClientID || !m_apPlayers[i] || !Server()->ClientIngame(i))
			continue;

		CPlayer *pPlayer = m_apPlayers[i];

		if(Server()->IsSixup(i))
			Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);

		if(Server()->IsSixup(ClientID))
		{
			// existing infos for new player
			protocol7::CNetMsg_Sv_ClientInfo ClientInfoMsg;
			ClientInfoMsg.m_ClientID = i;
			ClientInfoMsg.m_Local = 0;
			ClientInfoMsg.m_Team = pPlayer->GetTeam();
			ClientInfoMsg.m_pName = Server()->ClientName(i);
			ClientInfoMsg.m_pClan = Server()->ClientClan(i);
			ClientInfoMsg.m_Country = Server()->ClientCountry(i);
			ClientInfoMsg.m_Silent = 1;

			for(int p = 0; p < 6; p++)
			{
				ClientInfoMsg.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
				ClientInfoMsg.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
				ClientInfoMsg.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
			}

			Server()->SendPackMsg(&ClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
		}
	}

	// local info
	if(Server()->IsSixup(ClientID))
	{
		NewClientInfoMsg.m_Local = 1;
		Server()->SendPackMsg(&NewClientInfoMsg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}

	m_VoteUpdate = true;
}

void CGameContext::OnClientConnected(int ClientID)
{
	// Check which team the player should be on
	const int StartTeam = g_Config.m_SvTournamentMode ? TEAM_SPECTATORS : m_pController->GetAutoTeam(ClientID);

	m_apPlayers[ClientID] = new(ClientID) CPlayer(this, ClientID, Server()->GetClientSpec(ClientID) ? TEAM_SPECTATORS : StartTeam);

	(void)m_pController->CheckTeamBalance();
	//players[client_id].init(client_id);
	//players[client_id].client_id = client_id;

#ifdef CONF_DEBUG
	if(g_Config.m_DbgDummies)
	{
		if(ClientID >= MAX_CLIENTS-g_Config.m_DbgDummies)
			return;
	}
#endif

	// send active vote
	if(m_VoteCloseTime)
		SendVoteSet(ClientID);

	// send motd
	CNetMsg_Sv_Motd Msg;
	Msg.m_pMessage = g_Config.m_SvMotd;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
	
	SendSettings(ClientID);

	m_pController->UpdateGameInfo(ClientID);
}

int CGameContext::GetClientVersion(int ClientID) const
{
	return Server()->GetClientVersion(ClientID);
}

void CGameContext::OnClientDrop(int ClientID, const char *pReason)
{
	AbortVoteKickOnDisconnect(ClientID);
	m_apPlayers[ClientID]->OnDisconnect(pReason);
	delete m_apPlayers[ClientID];
	m_apPlayers[ClientID] = 0;

	m_VoteUpdate = true;

	// update spectator modes
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_apPlayers[i] && m_apPlayers[i]->m_SpectatorID == ClientID)
			m_apPlayers[i]->m_SpectatorID = SPEC_FREEVIEW;
	}
}

bool CheckClientID2(int ClientID)
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS;
}

void CGameContext::Whisper(int ClientID, char *pStr)
{
	if(g_Config.m_SvSpamprotection && m_apPlayers[ClientID]->m_LastChat && m_apPlayers[ClientID]->m_LastChat + Server()->TickSpeed() * ((15+str_length(pStr))/16) > Server()->Tick())
		return;
	else
		m_apPlayers[ClientID]->m_LastChat = Server()->Tick();
	pStr = str_skip_whitespaces(pStr);

	char *pName;
	int Victim;
	bool Error = false;

	// add token
	if(*pStr == '"')
	{
		pStr++;

		pName = pStr;
		char *pDst = pStr; // we might have to process escape data
		while(true)
		{
			if(pStr[0] == '"')
			{
				break;
			}
			else if(pStr[0] == '\\')
			{
				if(pStr[1] == '\\')
					pStr++; // skip due to escape
				else if(pStr[1] == '"')
					pStr++; // skip due to escape
			}
			else if(pStr[0] == 0)
			{
				Error = true;
				break;
			}

			*pDst = *pStr;
			pDst++;
			pStr++;
		}

		if(!Error)
		{
			// write null termination
			*pDst = 0;

			pStr++;

			for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
				if(str_comp(pName, Server()->ClientName(Victim)) == 0)
					break;
		}
	}
	else
	{
		pName = pStr;
		while(true)
		{
			if(pStr[0] == 0)
			{
				Error = true;
				break;
			}
			if(pStr[0] == ' ')
			{
				pStr[0] = 0;
				for(Victim = 0; Victim < MAX_CLIENTS; Victim++)
					if(str_comp(pName, Server()->ClientName(Victim)) == 0)
						break;

				pStr[0] = ' ';

				if(Victim < MAX_CLIENTS)
					break;
			}
			pStr++;
		}
	}

	if(pStr[0] != ' ')
	{
		Error = true;
	}

	*pStr = 0;
	pStr++;

	if(Error)
	{
		SendChatTarget(ClientID, "Invalid whisper");
		return;
	}

	if(Victim >= MAX_CLIENTS || !CheckClientID2(Victim))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "No player with name \"%s\" found", pName);
		SendChatTarget(ClientID, aBuf);
		return;
	}

	WhisperID(ClientID, Victim, pStr);
}

void CGameContext::WhisperID(int ClientID, int VictimID, const char *pMessage)
{
	if(!CheckClientID2(ClientID))
		return;

	if(!CheckClientID2(VictimID))
		return;

	char aCensoredMessage[256];
	CensorMessage(aCensoredMessage, pMessage, sizeof(aCensoredMessage));

	char aBuf[256];

	if(Server()->IsSixup(ClientID))
	{
		protocol7::CNetMsg_Sv_Chat Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Mode = protocol7::CHAT_WHISPER;
		Msg.m_pMessage = aCensoredMessage;
		Msg.m_TargetID = VictimID;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
	else if(GetClientVersion(ClientID) >= VERSION_DDNET_WHISPER)
	{
		CNetMsg_Sv_Chat Msg;
		Msg.m_Team = CHAT_WHISPER_SEND;
		Msg.m_ClientID = VictimID;
		Msg.m_pMessage = aCensoredMessage;
		
		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, ClientID);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "[→ %s] %s", Server()->ClientName(VictimID), aCensoredMessage);
		SendChatTarget(ClientID, aBuf);
	}

	if(Server()->IsSixup(VictimID))
	{
		protocol7::CNetMsg_Sv_Chat Msg;
		Msg.m_ClientID = ClientID;
		Msg.m_Mode = protocol7::CHAT_WHISPER;
		Msg.m_pMessage = aCensoredMessage;
		Msg.m_TargetID = VictimID;

		Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, VictimID);
	}
	else if(GetClientVersion(VictimID) >= VERSION_DDNET_WHISPER)
	{
		CNetMsg_Sv_Chat Msg2;
		Msg2.m_Team = CHAT_WHISPER_RECV;
		Msg2.m_ClientID = ClientID;
		Msg2.m_pMessage = aCensoredMessage;
		
		Server()->SendPackMsg(&Msg2, MSGFLAG_VITAL | MSGFLAG_NORECORD, VictimID);
	}
	else
	{
		str_format(aBuf, sizeof(aBuf), "[← %s] %s", Server()->ClientName(ClientID), aCensoredMessage);
		SendChatTarget(VictimID, aBuf);
	}
}

void *CGameContext::PreProcessMsg(int *pMsgID, CUnpacker *pUnpacker, int ClientID)
{
	if(Server()->IsSixup(ClientID) && *pMsgID < OFFSET_UUID)
	{
		void *pRawMsg = m_NetObjHandler7.SecureUnpackMsg(*pMsgID, pUnpacker);
		if(!pRawMsg)
			return 0;

		CPlayer *pPlayer = m_apPlayers[ClientID];
		static char s_aRawMsg[1024];

		if(*pMsgID == protocol7::NETMSGTYPE_CL_SAY)
		{
			protocol7::CNetMsg_Cl_Say *pMsg7 = (protocol7::CNetMsg_Cl_Say *)pRawMsg;
			// Should probably use a placement new to start the lifetime of the object to avoid future weirdness
			::CNetMsg_Cl_Say *pMsg = (::CNetMsg_Cl_Say *)s_aRawMsg;

			if(pMsg7->m_Target >= 0)
			{
				// Should we maybe recraft the message so that it can go through the usual path?
				WhisperID(ClientID, pMsg7->m_Target, pMsg7->m_pMessage);
				return 0;
			}

			pMsg->m_Team = pMsg7->m_Mode == protocol7::CHAT_TEAM;
			pMsg->m_pMessage = pMsg7->m_pMessage;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_STARTINFO)
		{
			protocol7::CNetMsg_Cl_StartInfo *pMsg7 = (protocol7::CNetMsg_Cl_StartInfo *)pRawMsg;
			::CNetMsg_Cl_StartInfo *pMsg = (::CNetMsg_Cl_StartInfo *)s_aRawMsg;

			pMsg->m_pName = pMsg7->m_pName;
			pMsg->m_pClan = pMsg7->m_pClan;
			pMsg->m_Country = pMsg7->m_Country;
			
			CTeeInfo Info(pMsg7->m_apSkinPartNames, pMsg7->m_aUseCustomColors, pMsg7->m_aSkinPartColors);
			Info.FromSixup();
			pPlayer->m_TeeInfos = Info;

			str_copy(s_aRawMsg + sizeof(*pMsg), Info.m_aSkinName, sizeof(s_aRawMsg) - sizeof(*pMsg));

			pMsg->m_pSkin = s_aRawMsg + sizeof(*pMsg);
			pMsg->m_UseCustomColor = pPlayer->m_TeeInfos.m_UseCustomColor;
			pMsg->m_ColorBody = pPlayer->m_TeeInfos.m_ColorBody;
			pMsg->m_ColorFeet = pPlayer->m_TeeInfos.m_ColorFeet;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_SKINCHANGE)
		{
			return 0;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_SETSPECTATORMODE)
		{
			protocol7::CNetMsg_Cl_SetSpectatorMode *pMsg7 = (protocol7::CNetMsg_Cl_SetSpectatorMode *)pRawMsg;
			::CNetMsg_Cl_SetSpectatorMode *pMsg = (::CNetMsg_Cl_SetSpectatorMode *)s_aRawMsg;

			if(pMsg7->m_SpecMode == protocol7::SPEC_FREEVIEW)
				pMsg->m_SpectatorID = SPEC_FREEVIEW;
			else if(pMsg7->m_SpecMode == protocol7::SPEC_PLAYER)
				pMsg->m_SpectatorID = pMsg7->m_SpectatorID;
			else
				pMsg->m_SpectatorID = SPEC_FREEVIEW; // Probably not needed
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_SETTEAM)
		{
			protocol7::CNetMsg_Cl_SetTeam *pMsg7 = (protocol7::CNetMsg_Cl_SetTeam *)pRawMsg;
			::CNetMsg_Cl_SetTeam *pMsg = (::CNetMsg_Cl_SetTeam *)s_aRawMsg;

			pMsg->m_Team = pMsg7->m_Team;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_COMMAND)
		{
			protocol7::CNetMsg_Cl_Command *pMsg7 = (protocol7::CNetMsg_Cl_Command *)pRawMsg;
			::CNetMsg_Cl_Say *pMsg = (::CNetMsg_Cl_Say *)s_aRawMsg;

			str_format(s_aRawMsg + sizeof(*pMsg), sizeof(s_aRawMsg) - sizeof(*pMsg), "/%s %s", pMsg7->m_pName, pMsg7->m_pArguments);
			pMsg->m_pMessage = s_aRawMsg + sizeof(*pMsg);
			dbg_msg("debug", "line='%s'", s_aRawMsg + sizeof(*pMsg));
			pMsg->m_Team = 0;

			*pMsgID = NETMSGTYPE_CL_SAY;
			return s_aRawMsg;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_CALLVOTE)
		{
			protocol7::CNetMsg_Cl_CallVote *pMsg7 = (protocol7::CNetMsg_Cl_CallVote *)pRawMsg;
			::CNetMsg_Cl_CallVote *pMsg = (::CNetMsg_Cl_CallVote *)s_aRawMsg;

			IServer::CClientInfo Info;
			Server()->GetClientInfo(ClientID, &Info);
			int Authed = Info.m_Authed;
			if(pMsg7->m_Force)
			{
				str_format(s_aRawMsg, sizeof(s_aRawMsg), "force_vote \"%s\" \"%s\" \"%s\"", pMsg7->m_pType, pMsg7->m_pValue, pMsg7->m_pReason);
				Console()->SetAccessLevel(Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : IConsole::ACCESS_LEVEL_MOD);
				Console()->ExecuteLine(s_aRawMsg, ClientID);
				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				return 0;
			}

			pMsg->m_pValue = pMsg7->m_pValue;
			pMsg->m_pReason = pMsg7->m_pReason;
			pMsg->m_pType = pMsg7->m_pType;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_EMOTICON)
		{
			protocol7::CNetMsg_Cl_Emoticon *pMsg7 = (protocol7::CNetMsg_Cl_Emoticon *)pRawMsg;
			::CNetMsg_Cl_Emoticon *pMsg = (::CNetMsg_Cl_Emoticon *)s_aRawMsg;

			pMsg->m_Emoticon = pMsg7->m_Emoticon;
		}
		else if(*pMsgID == protocol7::NETMSGTYPE_CL_VOTE)
		{
			protocol7::CNetMsg_Cl_Vote *pMsg7 = (protocol7::CNetMsg_Cl_Vote *)pRawMsg;
			::CNetMsg_Cl_Vote *pMsg = (::CNetMsg_Cl_Vote *)s_aRawMsg;

			pMsg->m_Vote = pMsg7->m_Vote;
		}

		*pMsgID = Msg_SevenToSix(*pMsgID);

		return s_aRawMsg;
	}
	else
		return m_NetObjHandler.SecureUnpackMsg(*pMsgID, pUnpacker);
}

void CGameContext::CensorMessage(char *pCensoredMessage, const char *pMessage, int Size)
{
	str_copy(pCensoredMessage, pMessage, Size);

	for(auto &Item : m_vCensorlist)
	{
		char *pCurLoc = pCensoredMessage;
		do
		{
			pCurLoc = (char *)str_utf8_find_nocase(pCurLoc, Item.c_str());
			if(pCurLoc)
			{
				for(int i = 0; i < (int)Item.length(); i++)
				{
					pCurLoc[i] = '*';
				}
				pCurLoc++;
			}
		} while(pCurLoc);
	}
}

void CGameContext::OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID)
{

	void *pRawMsg = PreProcessMsg(&MsgID, pUnpacker, ClientID);

	if(!pRawMsg)
		return;

	CPlayer *pPlayer = m_apPlayers[ClientID];

	if(Server()->ClientIngame(ClientID))
	{
		if(MsgID == NETMSGTYPE_CL_SAY)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat+Server()->TickSpeed() > Server()->Tick())
				return;

			CNetMsg_Cl_Say *pMsg = (CNetMsg_Cl_Say *)pRawMsg;
			if(!str_utf8_check(pMsg->m_pMessage))
			{
				return;
			}
			int Team = pMsg->m_Team ? pPlayer->GetTeam() : CGameContext::CHAT_ALL;

			// trim right and set maximum length to 128 utf8-characters
			int Length = 0;
			const char *p = pMsg->m_pMessage;
			const char *pEnd = 0;
			while(*p)
 			{
				const char *pStrOld = p;
				int Code = str_utf8_decode(&p);

				// check if unicode is not empty
				if(Code > 0x20 && Code != 0xA0 && Code != 0x034F && (Code < 0x2000 || Code > 0x200F) && (Code < 0x2028 || Code > 0x202F) &&
					(Code < 0x205F || Code > 0x2064) && (Code < 0x206A || Code > 0x206F) && (Code < 0xFE00 || Code > 0xFE0F) &&
					Code != 0xFEFF && (Code < 0xFFF9 || Code > 0xFFFC))
				{
					pEnd = 0;
				}
				else if(pEnd == 0)
					pEnd = pStrOld;

				if(++Length >= 127)
				{
					*(const_cast<char *>(p)) = 0;
					break;
				}
 			}
			if(pEnd != 0)
				*(const_cast<char *>(pEnd)) = 0;

			// drop empty and autocreated spam messages (more than 16 characters per second)
			if(Length == 0 || (pMsg->m_pMessage[0] != '/' && g_Config.m_SvSpamprotection && pPlayer->m_LastChat && pPlayer->m_LastChat+Server()->TickSpeed()*((15+Length)/16) > Server()->Tick()))
				return;
			
			if(pMsg->m_pMessage[0] == '/' || pMsg->m_pMessage[0] == '\\')
			{
				if(str_startswith_nocase(pMsg->m_pMessage + 1, "w "))
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 3, 256);
					Whisper(pPlayer->GetCID(), aWhisperMsg);
				}
				else if(str_startswith_nocase(pMsg->m_pMessage + 1, "whisper "))
				{
					char aWhisperMsg[256];
					str_copy(aWhisperMsg, pMsg->m_pMessage + 9, 256);
					Whisper(pPlayer->GetCID(), aWhisperMsg);
				}
				else
				{
					switch(m_apPlayers[ClientID]->m_Authed)
					{
						case IServer::AUTHED_ADMIN:
							Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
							break;
						case IServer::AUTHED_MOD:
							Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_MOD);
							break;
						default:
							Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_USER);
					}	
					m_ChatResponseTargetID = ClientID;

					Console()->ExecuteLineFlag(pMsg->m_pMessage + 1, ClientID, CFGFLAG_CHAT);
					
					m_ChatResponseTargetID = -1;
					Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				}
			}
			else
			{
				char aCensoredMessage[256];
				CensorMessage(aCensoredMessage, pMsg->m_pMessage, sizeof(aCensoredMessage));
				SendChat(ClientID, Team, pMsg->m_pMessage);
				pPlayer->m_LastChat = Server()->Tick();
			}
		}
		else if(MsgID == NETMSGTYPE_CL_CALLVOTE)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastVoteTry && pPlayer->m_LastVoteTry+Server()->TickSpeed()*3 > Server()->Tick())
				return;

			int64 Now = Server()->Tick();
			pPlayer->m_LastVoteTry = Now;
			if(pPlayer->GetTeam() == TEAM_SPECTATORS)
			{
				SendChatTarget_Localization(ClientID, _("Spectators aren't allowed to start a vote."), NULL);
				return;
			}

			if(m_VoteCloseTime)
			{
				SendChatTarget_Localization(ClientID, _("Wait for current vote to end before calling a new one."), NULL);
				return;
			}

			int Timeleft = pPlayer->m_LastVoteCall + Server()->TickSpeed()*60 - Now;
			if(pPlayer->m_LastVoteCall && Timeleft > 0)
			{
				int Seconds = (Timeleft/Server()->TickSpeed())+1;
				SendChatTarget_Localization(ClientID, _("You must wait {sec:Time} before making another vote"), "Time", &Seconds, NULL);
				return;
			}

			char aDesc[VOTE_DESC_LENGTH] = {0};
			char aSixupDesc[VOTE_DESC_LENGTH] = {0};
			char aCmd[VOTE_CMD_LENGTH] = {0};
			CNetMsg_Cl_CallVote *pMsg = (CNetMsg_Cl_CallVote *)pRawMsg;
			const char *pReason = pMsg->m_pReason[0] ? pMsg->m_pReason : "No reason given";

			if(str_comp_nocase(pMsg->m_pType, "option") == 0)
			{
				CVoteOptionServer *pOption = m_pVoteOptionFirst;
				while(pOption)
				{
					if(str_comp_nocase(pMsg->m_pValue, pOption->m_aDescription) == 0)
					{
						SendChatTarget_Localization(-1, _("'{str:Player}' called vote to change server option '{str:Option}' ({str:Reason})"),
									"Player", Server()->ClientName(ClientID), 
									"Option", pOption->m_aDescription,
									"Reason", pReason, NULL);
						str_format(aDesc, sizeof(aDesc), "%s", pOption->m_aDescription);
						str_format(aCmd, sizeof(aCmd), "%s", pOption->m_aCommand);
						break;
					}

					pOption = pOption->m_pNext;
				}

				if(!pOption)
				{
					SendChatTarget_Localization(ClientID, _("'{str:Option}' isn't an option on this server"), "Option", pMsg->m_pValue, NULL);
					return;
				}
			}
			else if(str_comp_nocase(pMsg->m_pType, "kick") == 0)
			{
				if(!g_Config.m_SvVoteKick)
				{
					SendChatTarget_Localization(ClientID, _("Server does not allow voting to kick players"), NULL);
					return;
				}

				if(g_Config.m_SvVoteKickMin)
				{
					int PlayerNum = 0;
					for(int i = 0; i < MAX_CLIENTS; ++i)
						if(m_apPlayers[i] && m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS)
							++PlayerNum;

					if(PlayerNum < g_Config.m_SvVoteKickMin)
					{
						int Num = g_Config.m_SvVoteKickMin;
						SendChatTarget_Localization(ClientID, _("Kick voting requires {int:Num} players on the server"), "Num", &Num, NULL);
						return;
					}
				}

				int KickID = str_toint(pMsg->m_pValue);
				if(KickID < 0 || KickID >= MAX_CLIENTS || !m_apPlayers[KickID])
				{
					SendChatTarget_Localization(ClientID, _("Invalid client id to kick"), NULL);
					return;
				}
				if(KickID == ClientID)
				{
					SendChatTarget_Localization(ClientID, _("You can't kick yourself"), NULL);
					return;
				}

				if (!Server()->ReverseTranslate(KickID, ClientID))
					return;

				if(Server()->IsAuthed(KickID))
				{
					SendChatTarget_Localization(ClientID, _("You can't kick admins"), NULL);
					SendChatTarget_Localization(KickID, _("'{str:Player}' called for vote to kick you"), 
						"Player", Server()->ClientName(ClientID),
						NULL);
					return;
				}

				SendChatTarget_Localization(-1, _("'{str:Player}' called for vote to kick '{str:Player2}' ({str:Reason})"),
					"Player", Server()->ClientName(ClientID), 
					"Player2", Server()->ClientName(KickID), 
					"Reason", pReason,
					NULL);
				str_format(aDesc, sizeof(aDesc), "Kick '%s'", Server()->ClientName(KickID));
				str_format(aSixupDesc, sizeof(aSixupDesc), "%2d: %s", KickID, Server()->ClientName(KickID));
				if (!g_Config.m_SvVoteKickBantime)
					str_format(aCmd, sizeof(aCmd), "kick %d Kicked by vote", KickID);\
				else
				{
					char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
					Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
					str_format(aCmd, sizeof(aCmd), "ban %s %d Banned by vote", aAddrStr, g_Config.m_SvVoteKickBantime);
				}
			}
			else if(str_comp_nocase(pMsg->m_pType, "spectate") == 0)
			{
				if(!g_Config.m_SvVoteSpectate)
				{
					SendChatTarget_Localization(ClientID, _("Server does not allow voting to move players to spectators"), NULL);
					return;
				}

				int SpectateID = str_toint(pMsg->m_pValue);
				if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !m_apPlayers[SpectateID] || m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
				{
					SendChatTarget_Localization(ClientID, _("Invalid client id to move"), NULL);
					return;
				}
				if(SpectateID == ClientID)
				{
					SendChatTarget_Localization(ClientID, _("You can't move yourself"), NULL);
					return;
				}
				if (!Server()->ReverseTranslate(SpectateID, ClientID))
					return;

				SendChatTarget_Localization(-1, _("'{str:Player}' called for vote to move '{str:Player2}' to spectators ({str:Reason})"), 
					"Player", Server()->ClientName(ClientID), 
					"Player2", Server()->ClientName(SpectateID), 
					"Reason", pReason,
					NULL);
				str_format(aDesc, sizeof(aDesc), "move '%s' to spectators", Server()->ClientName(SpectateID));
				str_format(aSixupDesc, sizeof(aSixupDesc), "%2d: %s", SpectateID, Server()->ClientName(SpectateID));
				str_format(aCmd, sizeof(aCmd), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
			}

			if(aCmd[0])
			{
				StartVote(aDesc, aCmd, pReason, aSixupDesc[0] ? aDesc : aSixupDesc);
				pPlayer->m_Vote = 1;
				pPlayer->m_VotePos = m_VotePos = 1;
				m_VoteCreator = ClientID;
				pPlayer->m_LastVoteCall = Now;
			}
		}
		else if(MsgID == NETMSGTYPE_CL_VOTE)
		{
			if(!m_VoteCloseTime)
				return;

			if(pPlayer->m_Vote == 0)
			{
				CNetMsg_Cl_Vote *pMsg = (CNetMsg_Cl_Vote *)pRawMsg;
				if(!pMsg->m_Vote)
					return;

				pPlayer->m_Vote = pMsg->m_Vote;
				pPlayer->m_VotePos = ++m_VotePos;
				m_VoteUpdate = true;
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SETTEAM && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetTeam *pMsg = (CNetMsg_Cl_SetTeam *)pRawMsg;

			if (pPlayer->GetTeam() == pMsg->m_Team || (g_Config.m_SvSpamprotection && pPlayer->m_LastSetTeam && pPlayer->m_LastSetTeam + Server()->TickSpeed() * 3 > Server()->Tick()))
				return;

			if (pPlayer->m_TeamChangeTick > Server()->Tick())
			{
				pPlayer->m_LastSetTeam = Server()->Tick();
				int TimeLeft = (pPlayer->m_TeamChangeTick - Server()->Tick()) / Server()->TickSpeed();
				SendBroadcast_Localization(ClientID, _("Time to wait before changing team: {sec:Time}"), 150, BROADCAST_SYSTEM, 
					"Time", &TimeLeft,
					NULL);
				return;
			}
			// Switch team on given client and kill/respawn him
			if (m_pController->CanJoinTeam(pMsg->m_Team, ClientID))
			{
				if (m_pController->CanChangeTeam(pPlayer, pMsg->m_Team))
				{
					pPlayer->m_LastSetTeam = Server()->Tick();
					if (pPlayer->GetTeam() == TEAM_SPECTATORS || pMsg->m_Team == TEAM_SPECTATORS)
						m_VoteUpdate = true;
					pPlayer->SetTeam(pMsg->m_Team);
				}
				else
					SendBroadcast_Localization(ClientID, _("Teams must be balanced, please join other team"), 150, BROADCAST_SYSTEM, NULL);
			}
			else
			{
				int Num = Server()->MaxClients() - g_Config.m_SvSpectatorSlots;
				SendBroadcast_Localization(ClientID, _("Only {int:Num} active players are allowed"), 150, BROADCAST_SYSTEM, 
					"Num", &Num,
					NULL);
			}
		}
		else if (MsgID == NETMSGTYPE_CL_SETSPECTATORMODE && !m_World.m_Paused)
		{
			CNetMsg_Cl_SetSpectatorMode *pMsg = (CNetMsg_Cl_SetSpectatorMode *)pRawMsg;

			if(g_Config.m_SvSpamprotection && pPlayer->m_LastSetSpectatorMode && pPlayer->m_LastSetSpectatorMode+Server()->TickSpeed()*3 > Server()->Tick())
				return;

			if(pMsg->m_SpectatorID != SPEC_FREEVIEW)
				if (!Server()->ReverseTranslate(pMsg->m_SpectatorID, ClientID))
					return;

			if(pPlayer->GetTeam() != TEAM_SPECTATORS || pPlayer->m_SpectatorID == pMsg->m_SpectatorID || ClientID == pMsg->m_SpectatorID)
				return;
			
			pPlayer->m_LastSetSpectatorMode = Server()->Tick();
			if(pMsg->m_SpectatorID != SPEC_FREEVIEW && (!m_apPlayers[pMsg->m_SpectatorID] || m_apPlayers[pMsg->m_SpectatorID]->GetTeam() == TEAM_SPECTATORS))
				SendChatTarget_Localization(ClientID, _("Invalid spectator id used"), NULL);
			else
				pPlayer->m_SpectatorID = pMsg->m_SpectatorID;
		}
		else if (MsgID == NETMSGTYPE_CL_CHANGEINFO)
		{
			if(g_Config.m_SvSpamprotection && pPlayer->m_LastChangeInfo && pPlayer->m_LastChangeInfo+Server()->TickSpeed()*5 > Server()->Tick())
				return;

			bool SixupNeedsUpdate = false;

			CNetMsg_Cl_ChangeInfo *pMsg = (CNetMsg_Cl_ChangeInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set infos
			char aOldName[MAX_NAME_LENGTH];
			str_copy(aOldName, Server()->ClientName(ClientID));
			Server()->SetClientName(ClientID, pMsg->m_pName);
			if(str_comp(aOldName, Server()->ClientName(ClientID)) != 0)
			{
				SendChatTarget_Localization(-1, _("'{str:OldName}' changed name to '{str:NewName}'"), 
					"OldName", aOldName, 
					"NewName", Server()->ClientName(ClientID),
					NULL);
			}

			if(str_comp(Server()->ClientClan(ClientID), pMsg->m_pClan))
				SixupNeedsUpdate = true;
			Server()->SetClientClan(ClientID, pMsg->m_pClan);

			if(Server()->ClientCountry(ClientID) != pMsg->m_Country)
				SixupNeedsUpdate = true;
			Server()->SetClientCountry(ClientID, pMsg->m_Country);

			m_pController->OnPlayerInfoChange(pPlayer);

			if(SixupNeedsUpdate)
			{
				protocol7::CNetMsg_Sv_ClientDrop Drop;
				Drop.m_ClientID = ClientID;
				Drop.m_pReason = "";
				Drop.m_Silent = true;

				protocol7::CNetMsg_Sv_ClientInfo Info;
				Info.m_ClientID = ClientID;
				Info.m_pName = Server()->ClientName(ClientID);
				Info.m_Country = pMsg->m_Country;
				Info.m_pClan = pMsg->m_pClan;
				Info.m_Local = 0;
				Info.m_Silent = true;
				Info.m_Team = pPlayer->GetTeam();

				for(int p = 0; p < 6; p++)
				{
					Info.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
					Info.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
					Info.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
				}

				for(int i = 0; i < Server()->MaxClients(); i++)
				{
					if(i != ClientID)
					{
						Server()->SendPackMsg(&Drop, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
						Server()->SendPackMsg(&Info, MSGFLAG_VITAL | MSGFLAG_NORECORD, i);
					}
				}
			}
			else
			{
				protocol7::CNetMsg_Sv_SkinChange Msg;
				Msg.m_ClientID = ClientID;
				for(int p = 0; p < 6; p++)
				{
					Msg.m_apSkinPartNames[p] = pPlayer->m_TeeInfos.m_apSkinPartNames[p];
					Msg.m_aSkinPartColors[p] = pPlayer->m_TeeInfos.m_aSkinPartColors[p];
					Msg.m_aUseCustomColors[p] = pPlayer->m_TeeInfos.m_aUseCustomColors[p];
				}

				Server()->SendPackMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_NORECORD, -1);
			}

			Server()->ExpireServerInfo();
		}
		else if (MsgID == NETMSGTYPE_CL_EMOTICON && !m_World.m_Paused)
		{
			CNetMsg_Cl_Emoticon *pMsg = (CNetMsg_Cl_Emoticon *)pRawMsg;

			if(g_Config.m_SvSpamprotection && pPlayer->m_LastEmote && pPlayer->m_LastEmote+Server()->TickSpeed()*3 > Server()->Tick())
				return;

			pPlayer->m_LastEmote = Server()->Tick();

			SendEmoticon(ClientID, pMsg->m_Emoticon);
		}
		else if (MsgID == NETMSGTYPE_CL_KILL && !m_World.m_Paused)
		{
			if(pPlayer->m_LastKill && pPlayer->m_LastKill+Server()->TickSpeed()*3 > Server()->Tick())
				return;

			pPlayer->m_LastKill = Server()->Tick();
			if(pPlayer->GetTeam() != TEAM_BLUE)
				pPlayer->KillCharacter(WEAPON_SELF);
		}
	}
	else
	{
		if(MsgID == NETMSGTYPE_CL_STARTINFO)
		{
			if(pPlayer->m_IsReady)
				return;

			CNetMsg_Cl_StartInfo *pMsg = (CNetMsg_Cl_StartInfo *)pRawMsg;
			pPlayer->m_LastChangeInfo = Server()->Tick();

			// set start infos
			Server()->SetClientName(ClientID, pMsg->m_pName);
			// trying to set client name can delete the player object, check if it still exists
			if(!m_apPlayers[ClientID])
			{
				return;
			}

			Server()->SetClientClan(ClientID, pMsg->m_pClan);
			Server()->SetClientCountry(ClientID, pMsg->m_Country);
			m_pController->OnPlayerInfoChange(pPlayer);
			str_copy(pPlayer->m_TeeInfos.m_aSkinName, pMsg->m_pSkin, sizeof(pPlayer->m_TeeInfos.m_aSkinName));
			pPlayer->m_TeeInfos.m_UseCustomColor = 0;
			pPlayer->m_TeeInfos.m_ColorBody = pMsg->m_ColorBody;
			pPlayer->m_TeeInfos.m_ColorFeet = pMsg->m_ColorFeet;
			if(!Server()->IsSixup(ClientID))
				pPlayer->m_TeeInfos.ToSixup();

			// send vote options
			CNetMsg_Sv_VoteClearOptions ClearMsg;
			Server()->SendPackMsg(&ClearMsg, MSGFLAG_VITAL, ClientID);

			CNetMsg_Sv_VoteOptionListAdd OptionMsg;
			int NumOptions = 0;
			OptionMsg.m_pDescription0 = "";
			OptionMsg.m_pDescription1 = "";
			OptionMsg.m_pDescription2 = "";
			OptionMsg.m_pDescription3 = "";
			OptionMsg.m_pDescription4 = "";
			OptionMsg.m_pDescription5 = "";
			OptionMsg.m_pDescription6 = "";
			OptionMsg.m_pDescription7 = "";
			OptionMsg.m_pDescription8 = "";
			OptionMsg.m_pDescription9 = "";
			OptionMsg.m_pDescription10 = "";
			OptionMsg.m_pDescription11 = "";
			OptionMsg.m_pDescription12 = "";
			OptionMsg.m_pDescription13 = "";
			OptionMsg.m_pDescription14 = "";
			CVoteOptionServer *pCurrent = m_pVoteOptionFirst;
			while(pCurrent)
			{
				switch(NumOptions++)
				{
				case 0: OptionMsg.m_pDescription0 = pCurrent->m_aDescription; break;
				case 1: OptionMsg.m_pDescription1 = pCurrent->m_aDescription; break;
				case 2: OptionMsg.m_pDescription2 = pCurrent->m_aDescription; break;
				case 3: OptionMsg.m_pDescription3 = pCurrent->m_aDescription; break;
				case 4: OptionMsg.m_pDescription4 = pCurrent->m_aDescription; break;
				case 5: OptionMsg.m_pDescription5 = pCurrent->m_aDescription; break;
				case 6: OptionMsg.m_pDescription6 = pCurrent->m_aDescription; break;
				case 7: OptionMsg.m_pDescription7 = pCurrent->m_aDescription; break;
				case 8: OptionMsg.m_pDescription8 = pCurrent->m_aDescription; break;
				case 9: OptionMsg.m_pDescription9 = pCurrent->m_aDescription; break;
				case 10: OptionMsg.m_pDescription10 = pCurrent->m_aDescription; break;
				case 11: OptionMsg.m_pDescription11 = pCurrent->m_aDescription; break;
				case 12: OptionMsg.m_pDescription12 = pCurrent->m_aDescription; break;
				case 13: OptionMsg.m_pDescription13 = pCurrent->m_aDescription; break;
				case 14:
					{
						OptionMsg.m_pDescription14 = pCurrent->m_aDescription;
						OptionMsg.m_NumOptions = NumOptions;
						Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
						OptionMsg = CNetMsg_Sv_VoteOptionListAdd();
						NumOptions = 0;
						OptionMsg.m_pDescription1 = "";
						OptionMsg.m_pDescription2 = "";
						OptionMsg.m_pDescription3 = "";
						OptionMsg.m_pDescription4 = "";
						OptionMsg.m_pDescription5 = "";
						OptionMsg.m_pDescription6 = "";
						OptionMsg.m_pDescription7 = "";
						OptionMsg.m_pDescription8 = "";
						OptionMsg.m_pDescription9 = "";
						OptionMsg.m_pDescription10 = "";
						OptionMsg.m_pDescription11 = "";
						OptionMsg.m_pDescription12 = "";
						OptionMsg.m_pDescription13 = "";
						OptionMsg.m_pDescription14 = "";
					}
				}
				pCurrent = pCurrent->m_pNext;
			}
			if(NumOptions > 0)
			{
				OptionMsg.m_NumOptions = NumOptions;
				Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, ClientID);
			}

			// send tuning parameters to client
			SendTuningParams(ClientID);

			// client is ready to enter
			pPlayer->m_IsReady = true;
			CNetMsg_Sv_ReadyToEnter m;
			Server()->SendPackMsg(&m, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);

			Server()->ExpireServerInfo();
		}
	}
}

void CGameContext::ConTuneParam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pParamName = pResult->GetString(0);
	float NewValue = pResult->GetFloat(1);

	if(pSelf->Tuning()->Set(pParamName, NewValue))
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "%s changed to %.2f", pParamName, NewValue);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
		pSelf->SendTuningParams(-1);
	}
	else
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "No such tuning parameter");
}

void CGameContext::ConTuneReset(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	CTuningParams TuningParams;
	*pSelf->Tuning() = TuningParams;
	pSelf->SendTuningParams(-1);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", "Tuning reset");
}

void CGameContext::ConTuneDump(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	char aBuf[256];
	for(int i = 0; i < pSelf->Tuning()->Num(); i++)
	{
		float v;
		pSelf->Tuning()->Get(i, &v);
		str_format(aBuf, sizeof(aBuf), "%s %.2f", pSelf->Tuning()->m_apNames[i], v);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "tuning", aBuf);
	}
}

void CGameContext::ConPause(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->TogglePause();
}

void CGameContext::ConChangeMap(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->ChangeMap(pResult->NumArguments() ? pResult->GetString(0) : "");
}

void CGameContext::ConRestart(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->m_pController->StartRound();
}

void CGameContext::ConBroadcast(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendBroadcast(-1, pResult->GetString(0));
}

void CGameContext::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	pSelf->SendChat(-1, CGameContext::CHAT_ALL, pResult->GetString(0));
}

void CGameContext::ConSetTeam(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	int ClientID = clamp(pResult->GetInteger(0), 0, (int)MAX_CLIENTS-1);
	int Team = clamp(pResult->GetInteger(1), -1, 1);
	int Delay = pResult->NumArguments()>2 ? pResult->GetInteger(2) : 0;
	if(!pSelf->m_apPlayers[ClientID])
		return;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "moved client %d to team %d", ClientID, Team);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	pSelf->m_apPlayers[ClientID]->m_TeamChangeTick = pSelf->Server()->Tick()+pSelf->Server()->TickSpeed()*Delay*60;
	pSelf->m_apPlayers[ClientID]->SetTeam(Team);
}

void CGameContext::ConAddVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);
	const char *pCommand = pResult->GetString(1);

	pSelf->AddVote(pDescription, pCommand);
}

void CGameContext::AddVote(const char *pDescription, const char *pCommand)
{
	if(m_NumVoteOptions == MAX_VOTE_OPTIONS)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "maximum number of vote options reached");
		return;
	}

	// check for valid option
	if(!Console()->LineIsValid(pCommand) || str_length(pCommand) >= VOTE_CMD_LENGTH)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid command '%s'", pCommand);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}
	while(*pDescription && *pDescription == ' ')
		pDescription++;
	if(str_length(pDescription) >= VOTE_DESC_LENGTH || *pDescription == 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "skipped invalid option '%s'", pDescription);
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// check for duplicate entry
	CVoteOptionServer *pOption = m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "option '%s' already exists", pDescription);
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
		pOption = pOption->m_pNext;
	}

	// add the option
	++m_NumVoteOptions;
	int Len = str_length(pCommand);

	pOption = (CVoteOptionServer *)m_pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
	pOption->m_pNext = 0;
	pOption->m_pPrev = m_pVoteOptionLast;
	if(pOption->m_pPrev)
		pOption->m_pPrev->m_pNext = pOption;
	m_pVoteOptionLast = pOption;
	if(!m_pVoteOptionFirst)
		m_pVoteOptionFirst = pOption;

	str_copy(pOption->m_aDescription, pDescription);
	mem_copy(pOption->m_aCommand, pCommand, Len+1);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "added option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// inform clients about added option
	CNetMsg_Sv_VoteOptionAdd OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);
}

void CGameContext::ConRemoveVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pDescription = pResult->GetString(0);

	// check for valid option
	CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
	while(pOption)
	{
		if(str_comp_nocase(pDescription, pOption->m_aDescription) == 0)
			break;
		pOption = pOption->m_pNext;
	}
	if(!pOption)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "option '%s' does not exist", pDescription);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
		return;
	}

	// inform clients about removed option
	CNetMsg_Sv_VoteOptionRemove OptionMsg;
	OptionMsg.m_pDescription = pOption->m_aDescription;
	pSelf->Server()->SendPackMsg(&OptionMsg, MSGFLAG_VITAL, -1);

	// TODO: improve this
	// remove the option
	--pSelf->m_NumVoteOptions;
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "removed option '%s' '%s'", pOption->m_aDescription, pOption->m_aCommand);
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	CHeap *pVoteOptionHeap = new CHeap();
	CVoteOptionServer *pVoteOptionFirst = 0;
	CVoteOptionServer *pVoteOptionLast = 0;
	int NumVoteOptions = pSelf->m_NumVoteOptions;
	for(CVoteOptionServer *pSrc = pSelf->m_pVoteOptionFirst; pSrc; pSrc = pSrc->m_pNext)
	{
		if(pSrc == pOption)
			continue;

		// copy option
		int Len = str_length(pSrc->m_aCommand);
		CVoteOptionServer *pDst = (CVoteOptionServer *)pVoteOptionHeap->Allocate(sizeof(CVoteOptionServer) + Len);
		pDst->m_pNext = 0;
		pDst->m_pPrev = pVoteOptionLast;
		if(pDst->m_pPrev)
			pDst->m_pPrev->m_pNext = pDst;
		pVoteOptionLast = pDst;
		if(!pVoteOptionFirst)
			pVoteOptionFirst = pDst;

		str_copy(pDst->m_aDescription, pSrc->m_aDescription);
		mem_copy(pDst->m_aCommand, pSrc->m_aCommand, Len+1);
	}

	// clean up
	delete pSelf->m_pVoteOptionHeap;
	pSelf->m_pVoteOptionHeap = pVoteOptionHeap;
	pSelf->m_pVoteOptionFirst = pVoteOptionFirst;
	pSelf->m_pVoteOptionLast = pVoteOptionLast;
	pSelf->m_NumVoteOptions = NumVoteOptions;
}

void CGameContext::ConForceVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	const char *pType = pResult->GetString(0);
	const char *pValue = pResult->GetString(1);
	const char *pReason = pResult->NumArguments() > 2 && pResult->GetString(2)[0] ? pResult->GetString(2) : "No reason given";
	char aBuf[128] = {0};

	if(str_comp_nocase(pType, "option") == 0)
	{
		CVoteOptionServer *pOption = pSelf->m_pVoteOptionFirst;
		while(pOption)
		{
			if(str_comp_nocase(pValue, pOption->m_aDescription) == 0)
			{
				pSelf->SendChatTarget_Localization(-1, _("admin forced server option '{str:Value}' ({str:Reason})"), 
					"Value", pValue, 
					"Reason", pReason,
					NULL);
				pSelf->Console()->ExecuteLine(pOption->m_aCommand, -1);
				break;
			}

			pOption = pOption->m_pNext;
		}

		if(!pOption)
		{
			str_format(aBuf, sizeof(aBuf), "'%s' isn't an option on this server", pValue);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
			return;
		}
	}
	else if(str_comp_nocase(pType, "kick") == 0)
	{
		int KickID = str_toint(pValue);
		if(KickID < 0 || KickID >= MAX_CLIENTS || !pSelf->m_apPlayers[KickID])
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to kick");
			return;
		}

		if (!g_Config.m_SvVoteKickBantime)
		{
			str_format(aBuf, sizeof(aBuf), "kick %d %s", KickID, pReason);
			pSelf->Console()->ExecuteLine(aBuf, -1);
		}
		else
		{
			char aAddrStr[NETADDR_MAXSTRSIZE] = {0};
			pSelf->Server()->GetClientAddr(KickID, aAddrStr, sizeof(aAddrStr));
			str_format(aBuf, sizeof(aBuf), "ban %s %d %s", aAddrStr, g_Config.m_SvVoteKickBantime, pReason);
			pSelf->Console()->ExecuteLine(aBuf, -1);
		}
	}
	else if(str_comp_nocase(pType, "spectate") == 0)
	{
		int SpectateID = str_toint(pValue);
		if(SpectateID < 0 || SpectateID >= MAX_CLIENTS || !pSelf->m_apPlayers[SpectateID] || pSelf->m_apPlayers[SpectateID]->GetTeam() == TEAM_SPECTATORS)
		{
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "Invalid client id to move");
			return;
		}
		pSelf->SendChatTarget_Localization(-1, _("admin moved '{str:Player}' to spectator ({str:Reason})"), 
			"Player", pSelf->Server()->ClientName(SpectateID), 
			"Reason", pReason, 
			NULL);
		str_format(aBuf, sizeof(aBuf), "set_team %d -1 %d", SpectateID, g_Config.m_SvVoteSpectateRejoindelay);
		pSelf->Console()->ExecuteLine(aBuf, -1);
	}
}

void CGameContext::ConClearVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "cleared votes");
	CNetMsg_Sv_VoteClearOptions VoteClearOptionsMsg;
	pSelf->Server()->SendPackMsg(&VoteClearOptionsMsg, MSGFLAG_VITAL, -1);
	pSelf->m_pVoteOptionHeap->Reset();
	pSelf->m_pVoteOptionFirst = 0;
	pSelf->m_pVoteOptionLast = 0;
	pSelf->m_NumVoteOptions = 0;
}

void CGameContext::ConVote(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	// check if there is a vote running
	if(!pSelf->m_VoteCloseTime)
		return;

	if(str_comp_nocase(pResult->GetString(0), "yes") == 0)
	{
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_YES;
		pSelf->SendChatTarget_Localization(-1, _("admin forced vote yes"), NULL);
	}
	else if(str_comp_nocase(pResult->GetString(0), "no") == 0)
	{
		pSelf->m_VoteEnforce = CGameContext::VOTE_ENFORCE_NO;
		pSelf->SendChatTarget_Localization(-1, _("admin forced vote no"), NULL);	
	}
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "forcing vote %s", pResult->GetString(0));
	pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
}

void CGameContext::ConAddMapVotes(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;

	pSelf->AddMapVotes();
}

void CGameContext::ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
	{
		CNetMsg_Sv_Motd Msg;
		Msg.m_pMessage = g_Config.m_SvMotd;
		CGameContext *pSelf = (CGameContext *)pUserData;
		for(int i = 0; i < MAX_CLIENTS; ++i)
			if(pSelf->m_apPlayers[i])
				pSelf->Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
	}
}

void CGameContext::ConAbout(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext* pSelf = (CGameContext*) pUserData;
	int ClientID = pResult->GetClientID();

	char aThanksList[256];

	str_copy(aThanksList, "pure_luck, necropotame");
	// infNext by pure_luck , necropotame made this frame

	pSelf->SendChatTarget(ClientID, "=====InfNext=====");
	pSelf->SendChatTarget(ClientID, "InfNext by RemakePower");
	pSelf->SendChatTarget_Localization(ClientID, _("Thanks {str:List}"), "List", aThanksList, NULL);
	pSelf->SendChatTarget_Localization(ClientID, _("Based on the concept of infNext mod by pure_luck"));
	
}

void CGameContext::ConLanguage(IConsole::IResult *pResult, void *pUserData)
{
	CGameContext *pSelf = (CGameContext *)pUserData;
	
	int ClientID = pResult->GetClientID();
	
	const char *pLanguageCode = (pResult->NumArguments()>0) ? pResult->GetString(0) : 0x0;
	char aFinalLanguageCode[8];
	aFinalLanguageCode[0] = 0;

	if(pLanguageCode)
	{
		if(str_comp_nocase(pLanguageCode, "ua") == 0)
			str_copy(aFinalLanguageCode, "uk");
		else
		{
			for(int i=0; i<pSelf->Server()->Localization()->m_pLanguages.size(); i++)
			{
				if(str_comp_nocase(pLanguageCode, pSelf->Server()->Localization()->m_pLanguages[i]->GetFilename()) == 0)
					str_copy(aFinalLanguageCode, pLanguageCode);
			}
		}
	}
	
	

	if(aFinalLanguageCode[0])
	{
		pSelf->SetClientLanguage(ClientID, aFinalLanguageCode);
		pSelf->SendChatTarget_Localization(ClientID, _("Language successfully switched to English"));
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%s change language to %s", pSelf->Server()->ClientName(ClientID), aFinalLanguageCode);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "language", aBuf);	
	}
	else
	{
		const char* pLanguage = pSelf->m_apPlayers[ClientID]->GetLanguage();
		const char* pTxtUnknownLanguage = pSelf->Server()->Localization()->Localize(pLanguage, _("Unknown language or no input language code"));
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "language", pTxtUnknownLanguage);

		std::string BufferList;
		for(int i=0; i<pSelf->Server()->Localization()->m_pLanguages.size(); i++)
		{
			if(i>0)
				BufferList.append(", ");
			BufferList.append(pSelf->Server()->Localization()->m_pLanguages[i]->GetFilename());
			BufferList.append("(");
			BufferList.append(pSelf->Server()->Localization()->m_pLanguages[i]->GetName());
			BufferList.append(")");
		}
		
		dynamic_string Buffer;
		pSelf->Server()->Localization()->Format_L(Buffer, pLanguage, _("Available languages: {str:List}"), "List", BufferList.c_str(), NULL);
		
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_CHAT, "language", Buffer.buffer());
	}
	
	return;
}
void CGameContext::SetClientLanguage(int ClientID, const char *pLanguage)
{
	Server()->SetClientLanguage(ClientID, pLanguage);
	if(m_apPlayers[ClientID])
	{
		m_apPlayers[ClientID]->SetLanguage(pLanguage);
	}
}

const char* CGameContext::Localize(const char *pLanguageCode, const char* pText) const
{
	return Server()->Localization()->Localize(pLanguageCode, pText);
}

const char* CGameContext::GetPlayerLanguage(int ClientID) const
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return "en";

	if(!m_apPlayers[ClientID])
		return "en";
	
	return m_apPlayers[ClientID]->GetLanguage();
}

void CGameContext::ConsoleOutputCallback_Chat(const char *pLine, void *pUser)
{
	CGameContext *pSelf = (CGameContext *)pUser;
	int ClientID = pSelf->m_ChatResponseTargetID;

	if(ClientID < 0 || ClientID >= MAX_CLIENTS)
		return;

	const char *pLineOrig = pLine;

	static volatile int ReentryGuard = 0;

	if(ReentryGuard)
		return;
	ReentryGuard++;

	if(*pLine == '[')
	do
		pLine++;
	while((pLine - 2 < pLineOrig || *(pLine - 2) != ':') && *pLine != 0); // remove the category (e.g. [Console]: No Such Command)

	pSelf->SendChatTarget(ClientID, pLine);

	ReentryGuard--;
}

void CGameContext::OnConsoleInit()
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	m_ChatPrintCBIndex = Console()->RegisterPrintCallback(IConsole::OUTPUT_LEVEL_CHAT, ConsoleOutputCallback_Chat, this);
	
	Console()->Register("tune", "si", CFGFLAG_SERVER, ConTuneParam, this, "Tune variable to value");
	Console()->Register("tune_reset", "", CFGFLAG_SERVER, ConTuneReset, this, "Reset tuning");
	Console()->Register("tune_dump", "", CFGFLAG_SERVER, ConTuneDump, this, "Dump tuning");

	Console()->Register("pause", "", CFGFLAG_SERVER, ConPause, this, "Pause/unpause game");
	Console()->Register("change_map", "?r", CFGFLAG_SERVER|CFGFLAG_STORE, ConChangeMap, this, "Change map");
	Console()->Register("restart", "?i", CFGFLAG_SERVER|CFGFLAG_STORE, ConRestart, this, "Restart in x seconds (0 = abort)");
	Console()->Register("broadcast", "r", CFGFLAG_SERVER, ConBroadcast, this, "Broadcast message");
	Console()->Register("say", "r", CFGFLAG_SERVER, ConSay, this, "Say in chat");
	Console()->Register("set_team", "ii?i", CFGFLAG_SERVER, ConSetTeam, this, "Set team of player to team");

	Console()->Register("add_vote", "sr", CFGFLAG_SERVER, ConAddVote, this, "Add a voting option");
	Console()->Register("remove_vote", "s", CFGFLAG_SERVER, ConRemoveVote, this, "remove a voting option");
	Console()->Register("force_vote", "ss?r", CFGFLAG_SERVER, ConForceVote, this, "Force a voting option");
	Console()->Register("clear_votes", "", CFGFLAG_SERVER, ConClearVotes, this, "Clears the voting options");
	Console()->Register("vote", "r", CFGFLAG_SERVER, ConVote, this, "Force a vote to yes/no");

	Console()->Register("add_map_votes", "", CFGFLAG_SERVER, ConAddMapVotes, this, "Add map votes");
	
	Console()->Register("about", "", CFGFLAG_CHAT, ConAbout, this, "Show information about the mod");
	Console()->Register("language", "?s", CFGFLAG_CHAT, ConLanguage, this, "change language");

	Console()->Chain("sv_motd", ConchainSpecialMotdupdate, this);
}

void CGameContext::OnInit(/*class IKernel *pKernel*/)
{
	m_pServer = Kernel()->RequestInterface<IServer>();
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_World.SetGameServer(this);
	m_Events.SetGameServer(this);

	//if(!data) // only load once
		//data = load_data_from_memory(internal_data);

	for(int i = 0; i < NUM_NETOBJTYPES; i++)
		Server()->SnapSetStaticsize(i, m_NetObjHandler.GetObjSize(i));

	m_Layers.Init(Kernel());
	m_Collision.Init(&m_Layers);

	// reset everything here
	//world = new GAMEWORLD;
	//players = new CPlayer[MAX_CLIENTS];

	// select gametype
	m_pController = new CGameControllerNext(this);

	const char *pCensorFilename = "censorlist.txt";
	IOHANDLE File = Storage()->OpenFile(pCensorFilename, IOFLAG_READ, IStorage::TYPE_ALL);
	if(!File)
	{
		dbg_msg("censorlist", "failed to open '%s'", pCensorFilename);
	}
	else
	{
		CLineReader LineReader;
		LineReader.Init(File);
		char *pLine;
		while((pLine = LineReader.Get()))
		{
			m_vCensorlist.emplace_back(pLine);
		}
		io_close(File);
	}

	// setup core world
	//for(int i = 0; i < MAX_CLIENTS; i++)
	//	game.players[i].core.world = &game.world.core;

	// create all entities from the game layer
	CMapItemLayerTilemap *pTileMap = m_Layers.GameLayer();
	CTile *pTiles = (CTile *)Kernel()->RequestInterface<IMap>()->GetData(pTileMap->m_Data);

	for(int y = 0; y < pTileMap->m_Height; y++)
	{
		for(int x = 0; x < pTileMap->m_Width; x++)
		{
			int Index = pTiles[y*pTileMap->m_Width+x].m_Index;

			if(Index >= ENTITY_OFFSET)
			{
				vec2 Pos(x*32.0f+16.0f, y*32.0f+16.0f);
				m_pController->OnEntity(Index-ENTITY_OFFSET, Pos);
			}
		}
	}
}

void CGameContext::OnShutdown()
{
	delete m_pController;
	m_pController = 0;
	Clear();
}

void CGameContext::OnSnap(int ClientID)
{
	// add tuning to demo
	CTuningParams StandardTuning;
	if(ClientID == -1 && Server()->DemoRecorder_IsRecording() && mem_comp(&StandardTuning, &m_Tuning, sizeof(CTuningParams)) != 0)
	{
		CMsgPacker Msg(NETMSGTYPE_SV_TUNEPARAMS);
		int *pParams = (int *)&m_Tuning;
		for(unsigned i = 0; i < sizeof(m_Tuning)/sizeof(int); i++)
			Msg.AddInt(pParams[i]);
		Server()->SendMsg(&Msg, MSGFLAG_RECORD|MSGFLAG_NOSEND, ClientID);
	}

	m_World.Snap(ClientID);
	m_pController->Snap(ClientID);
	m_Events.Snap(ClientID);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_apPlayers[i])
			m_apPlayers[i]->Snap(ClientID);
	}
}
void CGameContext::OnPreSnap() {}
void CGameContext::OnPostSnap()
{
	m_Events.Clear();
}

bool CGameContext::IsClientReady(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->m_IsReady ? true : false;
}

bool CGameContext::IsClientPlayer(int ClientID)
{
	return m_apPlayers[ClientID] && m_apPlayers[ClientID]->GetTeam() == TEAM_SPECTATORS ? false : true;
}

const char *CGameContext::GameType() { return m_pController->m_pGameType; }
const char *CGameContext::Version() { return GAME_VERSION; }
const char *CGameContext::NetVersion() { return GAME_NETVERSION; }

IGameServer *CreateGameServer() { return new CGameContext; }


void CGameContext::OnUpdatePlayerServerInfo(char *aBuf, int BufSize, int ID)
{
	if(!m_apPlayers[ID])
		return;

	char aCSkinName[64];

	CTeeInfo &TeeInfo = m_apPlayers[ID]->m_TeeInfos;

	char aJsonSkin[400];
	aJsonSkin[0] = '\0';

	if(!Server()->IsSixup(ID))
	{
		// 0.6
		if(TeeInfo.m_UseCustomColor)
		{
			str_format(aJsonSkin, sizeof(aJsonSkin),
				"\"name\":\"%s\","
				"\"color_body\":%d,"
				"\"color_feet\":%d",
				EscapeJson(aCSkinName, sizeof(aCSkinName), TeeInfo.m_aSkinName),
				TeeInfo.m_ColorBody,
				TeeInfo.m_ColorFeet);
		}
		else
		{
			str_format(aJsonSkin, sizeof(aJsonSkin),
				"\"name\":\"%s\"",
				EscapeJson(aCSkinName, sizeof(aCSkinName), TeeInfo.m_aSkinName));
		}
	}
	else
	{
		const char *apPartNames[protocol7::NUM_SKINPARTS] = {"body", "marking", "decoration", "hands", "feet", "eyes"};
		char aPartBuf[64];

		for(int i = 0; i < protocol7::NUM_SKINPARTS; ++i)
		{
			str_format(aPartBuf, sizeof(aPartBuf),
				"%s\"%s\":{"
				"\"name\":\"%s\"",
				i == 0 ? "" : ",",
				apPartNames[i],
				EscapeJson(aCSkinName, sizeof(aCSkinName), TeeInfo.m_apSkinPartNames[i]));

			str_append(aJsonSkin, aPartBuf, sizeof(aJsonSkin));

			if(TeeInfo.m_aUseCustomColors[i])
			{
				str_format(aPartBuf, sizeof(aPartBuf),
					",color:%d",
					TeeInfo.m_aSkinPartColors[i]);
				str_append(aJsonSkin, aPartBuf, sizeof(aJsonSkin));
			}
			str_append(aJsonSkin, "}", sizeof(aJsonSkin));
		}
	}

	str_format(aBuf, BufSize,
		",\"skin\":{"
		"%s"
		"},"
		"\"afk\":%s,"
		"\"team\":%d",
		aJsonSkin,
		JsonBool(0),
		m_apPlayers[ID]->GetTeam());
}

struct CMapNameItem
{
	char m_aName[IO_MAX_PATH_LENGTH - 4];

	bool operator<(const CMapNameItem &Other) const { return str_comp_nocase(m_aName, Other.m_aName) < 0; }
};

int CGameContext::MapScan(const char *pName, int IsDir, int DirType, void *pUserData)
{
	if(IsDir || !str_endswith(pName, ".map"))
		return 0;

	CMapNameItem Item;
	str_truncate(Item.m_aName, sizeof(Item.m_aName), pName, str_length(pName) - str_length(".map"));
	static_cast<std::vector<CMapNameItem> *>(pUserData)->push_back(Item);

	return 0;
}

void CGameContext::AddMapVotes()
{
	std::vector<CMapNameItem> vMapList;
	Storage()->ListDirectory(IStorage::TYPE_ALL, "maps", MapScan, &vMapList);

	AddVote("=======Map=======", "echo ?");

	for(unsigned int i = 0;i < (unsigned int) vMapList.size(); i ++)
	{
		char aDescription[64];
		str_format(aDescription, sizeof(aDescription), "%s", vMapList[i].m_aName);

		char aCommand[IO_MAX_PATH_LENGTH * 2 + 10];
		char aMapEscaped[IO_MAX_PATH_LENGTH * 2];
		char *pDst = aMapEscaped;
		str_escape(&pDst, vMapList[i].m_aName, aMapEscaped + sizeof(aMapEscaped));
		str_format(aCommand, sizeof(aCommand), "change_map \"%s\"", aMapEscaped);

		str_append(g_Config.m_SvMaprotation, vMapList[i].m_aName, sizeof(g_Config.m_SvMaprotation));

		if(i != (unsigned int) vMapList.size() - 1)
			str_append(g_Config.m_SvMaprotation, " ", sizeof(g_Config.m_SvMaprotation));

		AddVote(aDescription, aCommand);
	}

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "added maps to votes");
}

void CGameContext::UpdateBroadcast(int ClientID)
{
	std::string Buffer;
	for(int i = 0;i < (int) m_aBroadcast[ClientID].m_aBroadcast.size(); i ++)
	{
		m_aBroadcast[ClientID].m_aBroadcast[i].m_LastBroadcast = Server()->Tick();
		Buffer.append(m_aBroadcast[ClientID].m_aBroadcast[i].m_Text);
		if(i < (int) m_aBroadcast[ClientID].m_aBroadcast.size()-1)
			Buffer.append("\n");
	}

	CNetMsg_Sv_Broadcast Msg;
	Msg.m_pMessage = Buffer.c_str();
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_NORECORD, ClientID);
}