/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <base/math.h>
#include <base/system.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/engine.h>
#include <engine/map.h>
#include <engine/masterserver.h>
#include <engine/server.h>
#include <engine/storage.h>

#include <engine/shared/compression.h>
#include <engine/shared/config.h>
#include <engine/shared/datafile.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/filecollection.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/protocol.h>
#include <engine/shared/protocol_ex.h>
#include <engine/shared/snapshot.h>

#include <game/version.h>

#include <infnext/classes.h>

#include <mastersrv/mastersrv.h>

#include "register.h"
#include "server.h"

#include "mapconverter.h"

#include <teeuniverses/components/localization.h>

#include <engine/external/json-parser/json.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>

#if defined(CONF_FAMILY_WINDOWS)
	#define _WIN32_WINNT 0x0501
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#elif defined(CONF_FAMILY_UNIX)
    #include <cstring>//fixes memset error
#endif

static const char *StrLtrim(const char *pStr)
{
	while(*pStr && *pStr >= 0 && *pStr <= 32)
		pStr++;
	return pStr;
}

static void StrRtrim(char *pStr)
{
	int i = str_length(pStr);
	while(i >= 0)
	{
		if(pStr[i] < 0 || pStr[i] > 32)
			break;
		pStr[i] = 0;
		i--;
	}
}


CSnapIDPool::CSnapIDPool()
{
	Reset();
}

void CSnapIDPool::Reset()
{
	for(int i = 0; i < MAX_IDS; i++)
	{
		m_aIDs[i].m_Next = i+1;
		m_aIDs[i].m_State = 0;
	}

	m_aIDs[MAX_IDS-1].m_Next = -1;
	m_FirstFree = 0;
	m_FirstTimed = -1;
	m_LastTimed = -1;
	m_Usage = 0;
	m_InUsage = 0;
}


void CSnapIDPool::RemoveFirstTimeout()
{
	int NextTimed = m_aIDs[m_FirstTimed].m_Next;

	// add it to the free list
	m_aIDs[m_FirstTimed].m_Next = m_FirstFree;
	m_aIDs[m_FirstTimed].m_State = 0;
	m_FirstFree = m_FirstTimed;

	// remove it from the timed list
	m_FirstTimed = NextTimed;
	if(m_FirstTimed == -1)
		m_LastTimed = -1;

	m_Usage--;
}

int CSnapIDPool::NewID()
{
	int64 Now = time_get();

	// process timed ids
	while(m_FirstTimed != -1 && m_aIDs[m_FirstTimed].m_Timeout < Now)
		RemoveFirstTimeout();

	int ID = m_FirstFree;
	dbg_assert(ID != -1, "id error");
	if(ID == -1)
		return ID;
	m_FirstFree = m_aIDs[m_FirstFree].m_Next;
	m_aIDs[ID].m_State = 1;
	m_Usage++;
	m_InUsage++;
	return ID;
}

void CSnapIDPool::TimeoutIDs()
{
	// process timed ids
	while(m_FirstTimed != -1)
		RemoveFirstTimeout();
}

void CSnapIDPool::FreeID(int ID)
{
	if(ID < 0)
		return;
	dbg_assert(m_aIDs[ID].m_State == 1, "id is not alloced");

	m_InUsage--;
	m_aIDs[ID].m_State = 2;
	m_aIDs[ID].m_Timeout = time_get()+time_freq()*5;
	m_aIDs[ID].m_Next = -1;

	if(m_LastTimed != -1)
	{
		m_aIDs[m_LastTimed].m_Next = ID;
		m_LastTimed = ID;
	}
	else
	{
		m_FirstTimed = ID;
		m_LastTimed = ID;
	}
}


void CServerBan::InitServerBan(IConsole *pConsole, IStorage *pStorage, CServer* pServer)
{
	CNetBan::Init(pConsole, pStorage);

	m_pServer = pServer;

	// overwrites base command, todo: improve this
	Console()->Register("ban", "s?ir", CFGFLAG_SERVER|CFGFLAG_STORE, ConBanExt, this, "Ban player with ip/client id for x minutes for any reason");
}

template<class T>
int CServerBan::BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason)
{
	// validate address
	if(Server()->m_RconClientID >= 0 && Server()->m_RconClientID < MAX_CLIENTS &&
		Server()->m_aClients[Server()->m_RconClientID].m_State != CServer::CClient::STATE_EMPTY)
	{
		if(NetMatch(pData, Server()->m_NetServer.ClientAddr(Server()->m_RconClientID)))
		{
			Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (you can't ban yourself)");
			return -1;
		}

		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(i == Server()->m_RconClientID || Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				continue;

			if(Server()->m_aClients[i].m_Authed >= Server()->m_RconAuthLevel && NetMatch(pData, Server()->m_NetServer.ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}
	else if(Server()->m_RconClientID == IServer::RCON_CID_VOTE)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			if(Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
				continue;

			if(Server()->m_aClients[i].m_Authed != CServer::AUTHED_NO && NetMatch(pData, Server()->m_NetServer.ClientAddr(i)))
			{
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (command denied)");
				return -1;
			}
		}
	}

	int Result = Ban(pBanPool, pData, Seconds, pReason);
	if(Result != 0)
		return Result;

	// drop banned clients
	typename T::CDataType Data = *pData;
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(Server()->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY)
			continue;

		if(NetMatch(&Data, Server()->m_NetServer.ClientAddr(i)))
		{
			CNetHash NetHash(&Data);
			char aBuf[256];
			MakeBanInfo(pBanPool->Find(&Data, &NetHash), aBuf, sizeof(aBuf), MSGTYPE_PLAYER);
			Server()->m_NetServer.Drop(i, aBuf);
		}
	}

	return Result;
}

int CServerBan::BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason)
{
	return BanExt(&m_BanAddrPool, pAddr, Seconds, pReason);
}

int CServerBan::BanRange(const CNetRange *pRange, int Seconds, const char *pReason)
{
	if(pRange->IsValid())
		return BanExt(&m_BanRangePool, pRange, Seconds, pReason);

	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban failed (invalid range)");
	return -1;
}

void CServerBan::ConBanExt(IConsole::IResult *pResult, void *pUser)
{
	CServerBan *pThis = static_cast<CServerBan *>(pUser);

	const char *pStr = pResult->GetString(0);
	int Minutes = pResult->NumArguments()>1 ? clamp(pResult->GetInteger(1), 0, 44640) : 30;
	const char *pReason = pResult->NumArguments()>2 ? pResult->GetString(2) : "No reason given";

	if(StrAllnum(pStr))
	{
		int ClientID = str_toint(pStr);
		if(ClientID < 0 || ClientID >= MAX_CLIENTS || pThis->Server()->m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "net_ban", "ban error (invalid client id)");
		else
			pThis->BanAddr(pThis->Server()->m_NetServer.ClientAddr(ClientID), Minutes*60, pReason);
	}
	else
		ConBan(pResult, pUser);
}


void CServer::CClient::Reset()
{
	// reset input
	for(int i = 0; i < 200; i++)
		m_aInputs[i].m_GameTick = -1;
	m_CurrentInput = 0;
	mem_zero(&m_LatestInput, sizeof(m_LatestInput));

	m_Snapshots.PurgeAll();
	m_LastAckedSnapshot = -1;
	m_LastInputTick = -1;
	m_SnapRate = CClient::SNAPRATE_INIT;
	m_Score = 0;
	
	m_NextMapChunk = 0;
	// It's should be delete
	//str_copy(m_aLanguage, g_Config.m_SvDefaultLanguage, sizeof(m_aLanguage));
}

const char* CServer::GetClientLanguage(int ClientID)
{
	return m_aClients[ClientID].m_aLanguage;
}

void CServer::SetClientLanguage(int ClientID, const char* pLanguage)
{
	str_copy(m_aClients[ClientID].m_aLanguage, pLanguage, sizeof(m_aClients[ClientID].m_aLanguage));
}

bool CServer::GetClientSpec(int ClientID)
{
	return m_aClients[ClientID].m_Spec;
}

void CServer::SetClientSpec(int ClientID, bool Spec)
{
	m_aClients[ClientID].m_Spec = Spec;
}

CServer::CServer() : m_DemoRecorder(&m_SnapshotDelta)
{
	m_TickSpeed = SERVER_TICK_SPEED;

	m_pGameServer = 0;
	m_pClasses = 0;

	m_CurrentGameTick = 0;
	m_RunServer = 1;


	for(int i = 0; i < NUM_MAP_TYPES; i++)
	{
		m_apCurrentMapData[i] = 0;
		m_aCurrentMapSize[i] = 0;
	}

	m_MapReload = 0;

	m_RconClientID = IServer::RCON_CID_SERV;
	m_RconAuthLevel = AUTHED_ADMIN;

	m_ServerInfoFirstRequest = 0;
	m_ServerInfoNumRequests = 0;
	m_ServerInfoHighLoad = false;
	m_ServerInfoNeedsUpdate = false;

	m_pRegister = nullptr;

	Init();
}

CServer::~CServer()
{
	delete m_pRegister;
}

int CServer::TrySetClientName(int ClientID, const char *pName)
{
	char aTrimmedName[64];

	// trim the name
	str_copy(aTrimmedName, StrLtrim(pName), sizeof(aTrimmedName));
	StrRtrim(aTrimmedName);

	// check for empty names
	if(!aTrimmedName[0])
		return -1;

	// check if new and old name are the same
	if(m_aClients[ClientID].m_aName[0] && str_comp(m_aClients[ClientID].m_aName, aTrimmedName) == 0)
		return 0;

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "'%s' -> '%s'", pName, aTrimmedName);
	Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
	pName = aTrimmedName;

	// make sure that two clients doesn't have the same name
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(i != ClientID && m_aClients[i].m_State >= CClient::STATE_READY)
		{
			if(str_comp(pName, m_aClients[i].m_aName) == 0)
				return -1;
		}

	// set the client name
	str_copy(m_aClients[ClientID].m_aName, pName, MAX_NAME_LENGTH);
	return 0;
}



void CServer::SetClientName(int ClientID, const char *pName)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	if(!pName)
		return;

	char aCleanName[MAX_NAME_LENGTH];
	str_copy(aCleanName, pName, sizeof(aCleanName));

	if(TrySetClientName(ClientID, aCleanName))
	{
		// auto rename
		for(int i = 1;; i++)
		{
			char aNameTry[MAX_NAME_LENGTH];
			str_format(aNameTry, sizeof(aCleanName), "(%d)%s", i, aCleanName);
			if(TrySetClientName(ClientID, aNameTry) == 0)
				break;
		}
	}
}

void CServer::SetClientClan(int ClientID, const char *pClan)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY || !pClan)
		return;

	str_copy(m_aClients[ClientID].m_aClan, pClan, MAX_CLAN_LENGTH);
}

void CServer::SetClientCountry(int ClientID, int Country)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;

	m_aClients[ClientID].m_Country = Country;
}

void CServer::SetClientScore(int ClientID, int Score)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State < CClient::STATE_READY)
		return;
	m_aClients[ClientID].m_Score = Score;
}

void CServer::Kick(int ClientID, const char *pReason)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CClient::STATE_EMPTY)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "invalid client id to kick");
		return;
	}
	else if(m_RconClientID == ClientID)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "you can't kick yourself");
 		return;
	}
	else if(m_aClients[ClientID].m_Authed > m_RconAuthLevel)
	{
		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", "kick command denied");
 		return;
	}

	m_NetServer.Drop(ClientID, pReason);
}

/*int CServer::Tick()
{
	return m_CurrentGameTick;
}*/

int64 CServer::TickStartTime(int Tick)
{
	return m_GameStartTime + (time_freq()*Tick)/SERVER_TICK_SPEED;
}

/*int CServer::TickSpeed()
{
	return SERVER_TICK_SPEED;
}*/

int CServer::Init()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aClients[i].m_State = CClient::STATE_EMPTY;
		m_aClients[i].m_aName[0] = 0;
		m_aClients[i].m_aClan[0] = 0;
		m_aClients[i].m_Country = -1;
		m_aClients[i].m_Sixup = 0;
		m_aClients[i].m_Snapshots.Init();
	}

	m_CurrentGameTick = 0;

	return 0;
}

void CServer::SetRconCID(int ClientID)
{
	m_RconClientID = ClientID;
}

bool CServer::IsAuthed(int ClientID)
{
	return m_aClients[ClientID].m_Authed;
}

bool CServer::GetClientInfo(int ClientID, CClientInfo *pInfo) const
{
	dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "ClientID is not valid");
	dbg_assert(pInfo != nullptr, "pInfo cannot be null");

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
	{
		pInfo->m_pName = m_aClients[ClientID].m_aName;
		pInfo->m_Latency = m_aClients[ClientID].m_Latency;
		pInfo->m_GotDDNetVersion = m_aClients[ClientID].m_DDNetVersionSettled;
		pInfo->m_DDNetVersion = m_aClients[ClientID].m_DDNetVersion >= 0 ? m_aClients[ClientID].m_DDNetVersion : VERSION_VANILLA;
		if(m_aClients[ClientID].m_GotDDNetVersionPacket)
		{
			pInfo->m_pDDNetVersionStr = m_aClients[ClientID].m_aDDNetVersionStr;
		}
		else
		{
			pInfo->m_pDDNetVersionStr = nullptr;
		}
		return true;
	}
	return false;
}

void CServer::SetClientDDNetVersion(int ClientID, int DDNetVersion)
{
	dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "ClientID is not valid");

	if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
	{
		m_aClients[ClientID].m_DDNetVersion = DDNetVersion;
		m_aClients[ClientID].m_DDNetVersionSettled = true;
	}
}

void CServer::GetClientAddr(int ClientID, char *pAddrStr, int Size)
{
	if(ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CClient::STATE_INGAME)
		net_addr_str(m_NetServer.ClientAddr(ClientID), pAddrStr, Size, false);
}


const char *CServer::ClientName(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return "(invalid)";
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientID].m_aName;
	else
		return "(connecting)";

}

const char *CServer::ClientClan(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return "";
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientID].m_aClan;
	else
		return "";
}

int CServer::ClientCountry(int ClientID)
{
	if(ClientID < 0 || ClientID >= MAX_CLIENTS || m_aClients[ClientID].m_State == CServer::CClient::STATE_EMPTY)
		return -1;
	if(m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME)
		return m_aClients[ClientID].m_Country;
	else
		return -1;
}

bool CServer::ClientIngame(int ClientID)
{
	return ClientID >= 0 && ClientID < MAX_CLIENTS && m_aClients[ClientID].m_State == CServer::CClient::STATE_INGAME;
}

int CServer::MaxClients() const
{
	return m_NetServer.MaxClients();
}

int CServer::GetClientVersion(int ClientID) const
{
	// Assume latest client version for server demos
	if(ClientID == -1)
		return CLIENT_VERSIONNR;

	CClientInfo Info;
	if(GetClientInfo(ClientID, &Info))
		return Info.m_DDNetVersion;
	return VERSION_NONE;
}

static inline bool RepackMsg(const CMsgPacker *pMsg, CPacker &Packer, bool Sixup)
{
	int MsgId = pMsg->m_MsgID;
	Packer.Reset();

	if(Sixup && !pMsg->m_NoTranslate)
	{
		if(pMsg->m_System)
		{
			if(MsgId >= OFFSET_UUID)
				;
			else if(MsgId >= NETMSG_MAP_CHANGE && MsgId <= NETMSG_MAP_DATA)
				;
			else if(MsgId >= NETMSG_CON_READY && MsgId <= NETMSG_INPUTTIMING)
				MsgId += 1;
			else if(MsgId == NETMSG_RCON_LINE)
				MsgId = 13;
			else if(MsgId >= NETMSG_AUTH_CHALLANGE && MsgId <= NETMSG_AUTH_RESULT)
				MsgId += 4;
			else if(MsgId >= NETMSG_PING && MsgId <= NETMSG_ERROR)
				MsgId += 4;
			else if(MsgId >= NETMSG_RCON_CMD_ADD && MsgId <= NETMSG_RCON_CMD_REM)
				MsgId -= 11;
			else
			{
				dbg_msg("net", "DROP send sys %d", MsgId);
				return true;
			}
		}
		else
		{

			if(MsgId < 0)
				return true;
		}
	}

	if(MsgId < OFFSET_UUID)
	{
		Packer.AddInt((MsgId << 1) | (pMsg->m_System ? 1 : 0));
	}
	else
	{
		Packer.AddInt((0 << 1) | (pMsg->m_System ? 1 : 0)); // NETMSG_EX, NETMSGTYPE_EX
		g_UuidManager.PackUuid(MsgId, &Packer);
	}
	Packer.AddRaw(pMsg->Data(), pMsg->Size());

	return false;
}

int CServer::SendMsg(CMsgPacker *pMsg, int Flags, int ClientID)
{
	CNetChunk Packet;
	mem_zero(&Packet, sizeof(CNetChunk));
	if(Flags & MSGFLAG_VITAL)
		Packet.m_Flags |= NETSENDFLAG_VITAL;
	if(Flags & MSGFLAG_FLUSH)
		Packet.m_Flags |= NETSENDFLAG_FLUSH;

	if(ClientID < 0)
	{
		CPacker Pack6, Pack7;
		if(RepackMsg(pMsg, Pack6, false))
			return -1;
		if(RepackMsg(pMsg, Pack7, true))
			return -1;

		// write message to demo recorders
		if(!(Flags & MSGFLAG_NORECORD))
		{
			m_DemoRecorder.RecordMessage(Pack6.Data(), Pack6.Size());
		}

		if(!(Flags & MSGFLAG_NOSEND))
		{
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(m_aClients[i].m_State == CClient::STATE_INGAME)
				{
					CPacker *pPack = m_aClients[i].m_Sixup ? &Pack7 : &Pack6;
					Packet.m_pData = pPack->Data();
					Packet.m_DataSize = pPack->Size();
					Packet.m_ClientID = i;
					m_NetServer.Send(&Packet);
				}
			}
		}
	}
	else
	{
		CPacker Pack;
		if(RepackMsg(pMsg, Pack, m_aClients[ClientID].m_Sixup))
			return -1;

		Packet.m_ClientID = ClientID;
		Packet.m_pData = Pack.Data();
		Packet.m_DataSize = Pack.Size();

		// write message to demo recorders
		if(!(Flags & MSGFLAG_NORECORD))
		{
			m_DemoRecorder.RecordMessage(Pack.Data(), Pack.Size());
		}

		if(!(Flags & MSGFLAG_NOSEND))
			m_NetServer.Send(&Packet);
	}

	return 0;
}

void CServer::DoSnapshot()
{
	GameServer()->OnPreSnap();

	// create snapshot for demo recording
	if(m_DemoRecorder.IsRecording())
	{
		char aData[CSnapshot::MAX_SIZE];
		int SnapshotSize;

		// build snap and possibly add some messages
		m_SnapshotBuilder.Init();
		GameServer()->OnSnap(-1);
		SnapshotSize = m_SnapshotBuilder.Finish(aData);

		// write snapshot
		m_DemoRecorder.RecordSnapshot(Tick(), aData, SnapshotSize);
	}

	// create snapshots for all clients
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// client must be ingame to recive snapshots
		if(m_aClients[i].m_State != CClient::STATE_INGAME)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_RECOVER && (Tick()%50) != 0)
			continue;

		// this client is trying to recover, don't spam snapshots
		if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_INIT && (Tick()%10) != 0)
			continue;

		{
			m_SnapshotBuilder.Init(m_aClients[i].m_Sixup);

			GameServer()->OnSnap(i);

			// finish snapshot
			char aData[CSnapshot::MAX_SIZE];
			CSnapshot *pData = (CSnapshot *)aData; // Fix compiler warning for strict-aliasing
			int SnapshotSize = m_SnapshotBuilder.Finish(pData);

			if(m_DemoRecorder.IsRecording())
			{
				// write snapshot
				m_DemoRecorder.RecordSnapshot(Tick(), aData, SnapshotSize);
			}

			int Crc = pData->Crc();

			// remove old snapshots
			// keep 3 seconds worth of snapshots
			m_aClients[i].m_Snapshots.PurgeUntil(m_CurrentGameTick - SERVER_TICK_SPEED * 3);

			// save the snapshot
			m_aClients[i].m_Snapshots.Add(m_CurrentGameTick, time_get(), SnapshotSize, pData, 0, nullptr);

			// find snapshot that we can perform delta against
			static CSnapshot s_EmptySnap;
			s_EmptySnap.Clear();

			int DeltaTick = -1;
			CSnapshot *pDeltashot = &s_EmptySnap;
			{
				int DeltashotSize = m_aClients[i].m_Snapshots.Get(m_aClients[i].m_LastAckedSnapshot, 0, &pDeltashot, 0);
				if(DeltashotSize >= 0)
					DeltaTick = m_aClients[i].m_LastAckedSnapshot;
				else
				{
					// no acked package found, force client to recover rate
					if(m_aClients[i].m_SnapRate == CClient::SNAPRATE_FULL)
						m_aClients[i].m_SnapRate = CClient::SNAPRATE_RECOVER;
				}
			}

			// create delta
			m_SnapshotDelta.SetStaticsize(protocol7::NETEVENTTYPE_SOUNDWORLD, m_aClients[i].m_Sixup);
			m_SnapshotDelta.SetStaticsize(protocol7::NETEVENTTYPE_DAMAGE, m_aClients[i].m_Sixup);
			char aDeltaData[CSnapshot::MAX_SIZE];
			int DeltaSize = m_SnapshotDelta.CreateDelta(pDeltashot, pData, aDeltaData);

			if(DeltaSize)
			{
				// compress it
				const int MaxSize = MAX_SNAPSHOT_PACKSIZE;

				char aCompData[CSnapshot::MAX_SIZE];
				SnapshotSize = CVariableInt::Compress(aDeltaData, DeltaSize, aCompData, sizeof(aCompData));
				int NumPackets = (SnapshotSize + MaxSize - 1) / MaxSize;

				for(int n = 0, Left = SnapshotSize; Left > 0; n++)
				{
					int Chunk = Left < MaxSize ? Left : MaxSize;
					Left -= Chunk;

					if(NumPackets == 1)
					{
						CMsgPacker Msg(NETMSG_SNAPSINGLE, true);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick - DeltaTick);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n * MaxSize], Chunk);
						SendMsg(&Msg, MSGFLAG_FLUSH, i);
					}
					else
					{
						CMsgPacker Msg(NETMSG_SNAP, true);
						Msg.AddInt(m_CurrentGameTick);
						Msg.AddInt(m_CurrentGameTick - DeltaTick);
						Msg.AddInt(NumPackets);
						Msg.AddInt(n);
						Msg.AddInt(Crc);
						Msg.AddInt(Chunk);
						Msg.AddRaw(&aCompData[n * MaxSize], Chunk);
						SendMsg(&Msg, MSGFLAG_FLUSH, i);
					}
				}
			}
			else
			{
				CMsgPacker Msg(NETMSG_SNAPEMPTY, true);
				Msg.AddInt(m_CurrentGameTick);
				Msg.AddInt(m_CurrentGameTick - DeltaTick);
				SendMsg(&Msg, MSGFLAG_FLUSH, i);
			}
		}
	}

	GameServer()->OnPostSnap();
}

int CServer::ClientRejoinCallback(int ClientID, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientID].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientID].m_DDNetVersionSettled = false;

	pThis->m_aClients[ClientID].Reset();

	pThis->SendMap(ClientID);

	return 0;
}

int CServer::NewClientNoAuthCallback(int ClientID, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	pThis->m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientID].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientID].m_DDNetVersionSettled = false;

	str_copy(pThis->m_aClients[ClientID].m_aLanguage, g_Config.m_SvDefaultLanguage);
	pThis->m_aClients[ClientID].m_Spec = false;
	pThis->m_aClients[ClientID].Reset();

	pThis->SendMap(ClientID);
	pThis->SendCapabilities(ClientID);

	return 0;
}

int CServer::NewClientCallback(int ClientID, void *pUser, bool Sixup)
{
	CServer *pThis = (CServer *)pUser;
	pThis->m_aClients[ClientID].m_State = CClient::STATE_PREAUTH;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_DDNetVersion = VERSION_NONE;
	pThis->m_aClients[ClientID].m_GotDDNetVersionPacket = false;
	pThis->m_aClients[ClientID].m_DDNetVersionSettled = false;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	memset(&pThis->m_aClients[ClientID].m_Addr, 0, sizeof(NETADDR));
	
	str_copy(pThis->m_aClients[ClientID].m_aLanguage, g_Config.m_SvDefaultLanguage);
	pThis->m_aClients[ClientID].m_Spec = false;

	pThis->m_aClients[ClientID].Reset();

	pThis->m_aClients[ClientID].m_Sixup = Sixup;
	return 0;
}

int CServer::DelClientCallback(int ClientID, const char *pReason, void *pUser)
{
	CServer *pThis = (CServer *)pUser;

	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pThis->m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "client dropped. cid=%d addr=%s reason='%s'", ClientID, aAddrStr,	pReason);
	pThis->Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);

	// notify the mod about the drop
	if(pThis->m_aClients[ClientID].m_State >= CClient::STATE_READY)
		pThis->GameServer()->OnClientDrop(ClientID, pReason);

	pThis->m_aClients[ClientID].m_State = CClient::STATE_EMPTY;
	pThis->m_aClients[ClientID].m_aName[0] = 0;
	pThis->m_aClients[ClientID].m_aClan[0] = 0;
	pThis->m_aClients[ClientID].m_Country = -1;
	pThis->m_aClients[ClientID].m_Authed = AUTHED_NO;
	pThis->m_aClients[ClientID].m_AuthTries = 0;
	pThis->m_aClients[ClientID].m_pRconCmdToSend = 0;
	pThis->m_aClients[ClientID].m_Sixup = false;
	pThis->m_aClients[ClientID].m_Snapshots.PurgeAll();
	
	pThis->ExpireServerInfo();
	return 0;
}

void CServer::SendRconType(int ClientID, bool UsernameReq)
{
	CMsgPacker Msg(NETMSG_RCONTYPE, true);
	Msg.AddInt(UsernameReq);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendCapabilities(int ClientID)
{
	CMsgPacker Msg(NETMSG_CAPABILITIES, true);
	Msg.AddInt(SERVERCAP_CURVERSION); // version
	int CapFlag = SERVERCAPFLAG_DDNET | SERVERCAPFLAG_ANYPLAYERFLAG | SERVERCAPFLAG_PINGEX | SERVERCAPFLAG_SYNCWEAPONINPUT;
	if(g_Config.m_SvAllowDummy)
		CapFlag |= SERVERCAPFLAG_ALLOWDUMMY;
	Msg.AddInt(CapFlag); // flags
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendMap(int ClientID)
{
	int MapType = IsSixup(ClientID) ? MAP_TYPE_SIXUP : MAP_TYPE_SIX;
	{
		CMsgPacker Msg(NETMSG_MAP_DETAILS, true);
		Msg.AddString(GetMapName(), 0);
		Msg.AddRaw(&m_aCurrentMapSha256[MapType].data, sizeof(m_aCurrentMapSha256[MapType].data));
		Msg.AddInt(m_aCurrentMapCrc[MapType]);
		Msg.AddInt(m_aCurrentMapSize[MapType]);
		Msg.AddString("", 0); // HTTPS map download URL
		SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
	}
	{
		CMsgPacker Msg(NETMSG_MAP_CHANGE, true);
		Msg.AddString(GetMapName(), 0);
		Msg.AddInt(m_aCurrentMapCrc[MapType]);
		Msg.AddInt(m_aCurrentMapSize[MapType]);
		if(MapType == MAP_TYPE_SIXUP)
		{
			Msg.AddInt(g_Config.m_SvMapWindow);
			Msg.AddInt(1024 - 128);
			Msg.AddRaw(m_aCurrentMapSha256[MapType].data, sizeof(m_aCurrentMapSha256[MapType].data));
		}
		SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
	}

	m_aClients[ClientID].m_NextMapChunk = 0;
}

void CServer::SendMapData(int ClientID, int Chunk)
{
	int MapType = IsSixup(ClientID) ? MAP_TYPE_SIXUP : MAP_TYPE_SIX;
	unsigned int ChunkSize = 1024 - 128;
	unsigned int Offset = Chunk * ChunkSize;
	int Last = 0;

	// drop faulty map data requests
	if(Chunk < 0 || (int) Offset > m_aCurrentMapSize[MapType])
		return;

	if((int)Offset + (int)ChunkSize >= m_aCurrentMapSize[MapType])
	{
		ChunkSize = (unsigned int)m_aCurrentMapSize[MapType] - Offset;
		Last = 1;
	}

	CMsgPacker Msg(NETMSG_MAP_DATA, true);
	if(MapType == MAP_TYPE_SIX)
	{
		Msg.AddInt(Last);
		Msg.AddInt(m_aCurrentMapCrc[MAP_TYPE_SIX]);
		Msg.AddInt(Chunk);
		Msg.AddInt(ChunkSize);
	}
	Msg.AddRaw(&m_apCurrentMapData[MapType][Offset], ChunkSize);
	SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);

	if(g_Config.m_Debug)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "sending chunk %d with size %d", Chunk, ChunkSize);
		Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
	}
}
void CServer::SendConnectionReady(int ClientID)
{
	CMsgPacker Msg(NETMSG_CON_READY, true);
	SendMsg(&Msg, MSGFLAG_VITAL|MSGFLAG_FLUSH, ClientID);
}

void CServer::SendRconLine(int ClientID, const char *pLine)
{
	CMsgPacker Msg(NETMSG_RCON_LINE, true);
	Msg.AddString(pLine, 512);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendRconLineAuthed(const char *pLine, void *pUser)
{
	CServer *pThis = (CServer *)pUser;
	static volatile int ReentryGuard = 0;
	int i;

	if(ReentryGuard) return;
	ReentryGuard++;

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY && pThis->m_aClients[i].m_Authed >= pThis->m_RconAuthLevel)
			pThis->SendRconLine(i, pLine);
	}

	ReentryGuard--;
}

void CServer::SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_ADD, true);
	Msg.AddString(pCommandInfo->m_pName, IConsole::TEMPCMD_NAME_LENGTH);
	Msg.AddString(pCommandInfo->m_pHelp, IConsole::TEMPCMD_HELP_LENGTH);
	Msg.AddString(pCommandInfo->m_pParams, IConsole::TEMPCMD_PARAMS_LENGTH);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID)
{
	CMsgPacker Msg(NETMSG_RCON_CMD_REM, true);
	Msg.AddString(pCommandInfo->m_pName, 256);
	SendMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CServer::UpdateClientRconCommands()
{
	int ClientID = Tick() % MAX_CLIENTS;

	if(m_aClients[ClientID].m_State != CClient::STATE_EMPTY && m_aClients[ClientID].m_Authed)
	{
		int ConsoleAccessLevel = m_aClients[ClientID].m_Authed == AUTHED_ADMIN ? IConsole::ACCESS_LEVEL_ADMIN : IConsole::ACCESS_LEVEL_MOD;
		for(int i = 0; i < MAX_RCONCMD_SEND && m_aClients[ClientID].m_pRconCmdToSend; ++i)
		{
			SendRconCmdAdd(m_aClients[ClientID].m_pRconCmdToSend, ClientID);
			m_aClients[ClientID].m_pRconCmdToSend = m_aClients[ClientID].m_pRconCmdToSend->NextCommandInfo(ConsoleAccessLevel, CFGFLAG_SERVER);
		}
	}
}

static inline int MsgFromSixup(int Msg, bool System)
{
	if(System)
	{
		if(Msg == NETMSG_INFO)
			;
		else if(Msg >= 14 && Msg <= 15)
			Msg += 11;
		else if(Msg >= 18 && Msg <= 28)
			Msg = NETMSG_READY + Msg - 18;
		else if(Msg < OFFSET_UUID)
			return -1;
	}

	return Msg;
}

void CServer::ProcessClientPacket(CNetChunk *pPacket)
{
	int ClientID = pPacket->m_ClientID;
	CUnpacker Unpacker;
	Unpacker.Reset(pPacket->m_pData, pPacket->m_DataSize);
	CMsgPacker Packer(NETMSG_EX, true);

	// unpack msgid and system flag
	int Msg;
	bool Sys;
	CUuid Uuid;

	int Result = UnpackMessageID(&Msg, &Sys, &Uuid, &Unpacker, &Packer);
	if(Result == UNPACKMESSAGE_ERROR)
	{
		return;
	}

	if(m_aClients[ClientID].m_Sixup && (Msg = MsgFromSixup(Msg, Sys)) < 0)
	{
		return;
	}

	if(Result == UNPACKMESSAGE_ANSWER)
	{
		SendMsg(&Packer, MSGFLAG_VITAL, ClientID);
	}

	if(Sys)
	{
		// system message
		if(Msg == NETMSG_CLIENTVER)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_PREAUTH)
			{
				CUuid *pConnectionID = (CUuid *)Unpacker.GetRaw(sizeof(*pConnectionID));
				int DDNetVersion = Unpacker.GetInt();
				const char *pDDNetVersionStr = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(Unpacker.Error() || !str_utf8_check(pDDNetVersionStr) || DDNetVersion < 0)
				{
					return;
				}
				m_aClients[ClientID].m_DDNetVersion = DDNetVersion;
				str_copy(m_aClients[ClientID].m_aDDNetVersionStr, pDDNetVersionStr);
				m_aClients[ClientID].m_DDNetVersionSettled = true;
				m_aClients[ClientID].m_GotDDNetVersionPacket = true;
				m_aClients[ClientID].m_State = CClient::STATE_AUTH;
			}
		}
		else if(Msg == NETMSG_INFO)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && (m_aClients[ClientID].m_State == CClient::STATE_PREAUTH || m_aClients[ClientID].m_State == CClient::STATE_AUTH))
			{
				const char *pVersion = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(!str_utf8_check(pVersion))
				{
					return;
				}
				if((str_comp(pVersion, GameServer()->NetVersion()) != 0) && str_comp(pVersion, "0.7 802f1be60a05665f") != 0)
				{
					// wrong version
					char aReason[256];
					str_format(aReason, sizeof(aReason), "Wrong version. Server is running '%s' and client '%s'", GameServer()->NetVersion(), pVersion);
					m_NetServer.Drop(ClientID, aReason);
					return;
				}

				const char *pPassword = Unpacker.GetString(CUnpacker::SANITIZE_CC);
				if(g_Config.m_Password[0] != 0 && str_comp(g_Config.m_Password, pPassword) != 0)
				{
					// wrong password
					m_NetServer.Drop(ClientID, "Wrong password");
					return;
				}

				m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
				SendCapabilities(ClientID);
				SendMap(ClientID);
			}
		}
		else if(Msg == NETMSG_REQUEST_MAP_DATA)
		{
			if((pPacket->m_Flags & NET_CHUNKFLAG_VITAL) == 0 || m_aClients[ClientID].m_State < CClient::STATE_CONNECTING)
				return;

			if(m_aClients[ClientID].m_Sixup)
			{
				for(int i = 0; i < g_Config.m_SvMapWindow; i++)
				{
					SendMapData(ClientID, m_aClients[ClientID].m_NextMapChunk++);
				}
				return;
			}

			int Chunk = Unpacker.GetInt();
			if(Chunk != m_aClients[ClientID].m_NextMapChunk || !g_Config.m_SvFastDownload)
			{
				SendMapData(ClientID, Chunk);
				return;
			}

			if(Chunk == 0)
			{
				for(int i = 0; i < g_Config.m_SvMapWindow; i++)
				{
					SendMapData(ClientID, i);
				}
			}
			SendMapData(ClientID, g_Config.m_SvMapWindow + m_aClients[ClientID].m_NextMapChunk);
			m_aClients[ClientID].m_NextMapChunk++;
		}
		else if(Msg == NETMSG_READY)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_CONNECTING)
			{
				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "player is ready. ClientID=%x addr=%s secure=%s", ClientID, aAddrStr, m_NetServer.HasSecurityToken(ClientID)?"yes":"no");
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
				m_aClients[ClientID].m_State = CClient::STATE_READY;
				GameServer()->OnClientConnected(ClientID);
				SendConnectionReady(ClientID);
				ExpireServerInfo();
			}
		}
		else if(Msg == NETMSG_ENTERGAME)
		{
			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State == CClient::STATE_READY && GameServer()->IsClientReady(ClientID))
			{
				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(m_NetServer.ClientAddr(ClientID), aAddrStr, sizeof(aAddrStr), true);

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "player has entered the game. ClientID=%d addr=<{%s}> sixup=%d", ClientID, aAddrStr, IsSixup(ClientID));
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				m_aClients[ClientID].m_State = CClient::STATE_INGAME;
				if(IsSixup(ClientID))
				{
					CMsgPacker Msgp(4, true, true); //NETMSG_SERVERINFO //TODO: Import the shared protocol from 7 aswell
					GetServerInfoSixup(&Msgp, -1, false);
					SendMsg(&Msgp, MSGFLAG_VITAL | MSGFLAG_FLUSH, ClientID);
				}
				GameServer()->OnClientEnter(ClientID);
			}
		}
		else if(Msg == NETMSG_INPUT)
		{
			
			m_aClients[ClientID].m_LastAckedSnapshot = Unpacker.GetInt();
			int IntendedTick = Unpacker.GetInt();
			int Size = Unpacker.GetInt();

			// check for errors
			if(Unpacker.Error() || Size / 4 > MAX_INPUT_SIZE || IntendedTick < MIN_TICK || IntendedTick >= MAX_TICK)
				return;

			if(m_aClients[ClientID].m_LastAckedSnapshot > 0)
				m_aClients[ClientID].m_SnapRate = CClient::SNAPRATE_FULL;

			int64_t TagTime;
			if(m_aClients[ClientID].m_Snapshots.Get(m_aClients[ClientID].m_LastAckedSnapshot, &TagTime, 0, 0) >= 0)
				m_aClients[ClientID].m_Latency = (int)(((time_get() - TagTime) * 1000) / time_freq());

			// add message to report the input timing
			// skip packets that are old
			if(IntendedTick > m_aClients[ClientID].m_LastInputTick)
			{
				const int TimeLeft = (TickStartTime(IntendedTick) - time_get()) / (time_freq() / 1000);

				CMsgPacker Msgp(NETMSG_INPUTTIMING, true);
				Msgp.AddInt(IntendedTick);
				Msgp.AddInt(TimeLeft);
				SendMsg(&Msgp, 0, ClientID);
			}

			m_aClients[ClientID].m_LastInputTick = IntendedTick;

			CClient::CInput *pInput = &m_aClients[ClientID].m_aInputs[m_aClients[ClientID].m_CurrentInput];

			if(IntendedTick <= Tick())
				IntendedTick = Tick() + 1;

			pInput->m_GameTick = IntendedTick;

			for(int i = 0; i < Size / 4; i++)
				pInput->m_aData[i] = Unpacker.GetInt();

			GameServer()->OnClientPrepareInput(ClientID, pInput->m_aData);
			mem_copy(m_aClients[ClientID].m_LatestInput.m_aData, pInput->m_aData, MAX_INPUT_SIZE * sizeof(int));

			m_aClients[ClientID].m_CurrentInput++;
			m_aClients[ClientID].m_CurrentInput %= 200;

			// call the mod with the fresh input data
			if(m_aClients[ClientID].m_State == CClient::STATE_INGAME)
				GameServer()->OnClientDirectInput(ClientID, m_aClients[ClientID].m_LatestInput.m_aData);
		}
		else if(Msg == NETMSG_RCON_CMD)
		{
			const char *pCmd = Unpacker.GetString();

			if(Unpacker.Error() == 0 && !str_comp(pCmd, "crashmeplx"))
			{
				int Version = m_aClients[ClientID].m_DDNetVersion;
				if(GameServer()->PlayerExists(ClientID) && Version < VERSION_DDNET_OLD)
				{
					m_aClients[ClientID].m_DDNetVersion = VERSION_DDNET_OLD;
				}
			}
			else if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Error() == 0 && m_aClients[ClientID].m_Authed)
			{
				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "ClientID=%d rcon='%s'", ClientID, pCmd);
				Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
				m_RconClientID = ClientID;
				m_RconAuthLevel = m_aClients[ClientID].m_Authed;
				switch(m_aClients[ClientID].m_Authed)
				{
					case AUTHED_ADMIN:
						Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
						break;
					case AUTHED_MOD:
						Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_MOD);
						break;
					default:
						Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_USER);
				}	
				Console()->ExecuteLineFlag(pCmd, ClientID, CFGFLAG_SERVER);
				Console()->SetAccessLevel(IConsole::ACCESS_LEVEL_ADMIN);
				m_RconClientID = IServer::RCON_CID_SERV;
				m_RconAuthLevel = AUTHED_ADMIN;
			}
		}
		else if(Msg == NETMSG_RCON_AUTH)
		{
			const char *pPw;
			if(!IsSixup(ClientID))
				Unpacker.GetString(); // login name, not used
			pPw = Unpacker.GetString(CUnpacker::SANITIZE_CC);

			if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && Unpacker.Error() == 0)
			{
				if(g_Config.m_SvRconPassword[0] == 0 && g_Config.m_SvRconModPassword[0] == 0)
				{
					SendRconLine(ClientID, "No rcon password set on server. Set sv_rcon_password and/or sv_rcon_mod_password to enable the remote console.");
				}
				else if(g_Config.m_SvRconPassword[0] && str_comp(pPw, g_Config.m_SvRconPassword) == 0)
				{
					if(!IsSixup(ClientID))
					{
						CMsgPacker Msgp(NETMSG_RCON_AUTH_STATUS, true);
						Msgp.AddInt(1); //authed
						Msgp.AddInt(1); //cmdlist
						SendMsg(&Msgp, MSGFLAG_VITAL, ClientID);
					}
					else
					{
						CMsgPacker Msgp(11, true, true); //NETMSG_RCON_AUTH_ON
						SendMsg(&Msgp, MSGFLAG_VITAL, ClientID);
					}

					m_aClients[ClientID].m_Authed = AUTHED_ADMIN;
					GameServer()->OnSetAuthed(ClientID, m_aClients[ClientID].m_Authed);
					int SendRconCmds = IsSixup(ClientID) ? true : Unpacker.GetInt();
					if(Unpacker.Error() == 0 && SendRconCmds)
						m_aClients[ClientID].m_pRconCmdToSend = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_ADMIN, CFGFLAG_SERVER);
					SendRconLine(ClientID, "Admin authentication successful. Full remote console access granted.");
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "ClientID=%d authed (admin)", ClientID);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				}
				else if(g_Config.m_SvRconModPassword[0] && str_comp(pPw, g_Config.m_SvRconModPassword) == 0)
				{
					if(!IsSixup(ClientID))
					{
						CMsgPacker Msgp(NETMSG_RCON_AUTH_STATUS, true);
						Msgp.AddInt(1); //authed
						Msgp.AddInt(1); //cmdlist
						SendMsg(&Msgp, MSGFLAG_VITAL, ClientID);
					}
					else
					{
						CMsgPacker Msgp(11, true, true); //NETMSG_RCON_AUTH_ON
						SendMsg(&Msgp, MSGFLAG_VITAL, ClientID);
					}

					m_aClients[ClientID].m_Authed = AUTHED_MOD;
					int SendRconCmds = IsSixup(ClientID) ? true : Unpacker.GetInt();
					if(Unpacker.Error() == 0 && SendRconCmds)
						m_aClients[ClientID].m_pRconCmdToSend = Console()->FirstCommandInfo(IConsole::ACCESS_LEVEL_MOD, CFGFLAG_SERVER);
					SendRconLine(ClientID, "Moderator authentication successful. Limited remote console access granted.");
					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "ClientID=%d authed (moderator)", ClientID);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
				}
				else if(g_Config.m_SvRconMaxTries)
				{
					m_aClients[ClientID].m_AuthTries++;
					char aBuf[128];
					str_format(aBuf, sizeof(aBuf), "Wrong password %d/%d.", m_aClients[ClientID].m_AuthTries, g_Config.m_SvRconMaxTries);
					SendRconLine(ClientID, aBuf);
					if(m_aClients[ClientID].m_AuthTries >= g_Config.m_SvRconMaxTries)
					{
						if(!g_Config.m_SvRconBantime)
							m_NetServer.Drop(ClientID, "Too many remote console authentication tries");
						else
							m_ServerBan.BanAddr(m_NetServer.ClientAddr(ClientID), g_Config.m_SvRconBantime*60, "Too many remote console authentication tries");
					}
				}
				else
				{
					SendRconLine(ClientID, "Wrong password.");
				}
			}
		}
		else if(Msg == NETMSG_PING)
		{
			CMsgPacker Msg(NETMSG_PING_REPLY, true);
			SendMsg(&Msg, 0, ClientID);
		}
		else
		{
			if(g_Config.m_Debug)
			{
				char aHex[] = "0123456789ABCDEF";
				char aBuf[512];

				for(int b = 0; b < pPacket->m_DataSize && b < 32; b++)
				{
					aBuf[b*3] = aHex[((const unsigned char *)pPacket->m_pData)[b]>>4];
					aBuf[b*3+1] = aHex[((const unsigned char *)pPacket->m_pData)[b]&0xf];
					aBuf[b*3+2] = ' ';
					aBuf[b*3+3] = 0;
				}

				char aBufMsg[256];
				str_format(aBufMsg, sizeof(aBufMsg), "strange message ClientID=%d msg=%d data_size=%d", ClientID, Msg, pPacket->m_DataSize);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBufMsg);
				Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "server", aBuf);
			}
		}
	}
	else
	{
		// game message
		if((pPacket->m_Flags&NET_CHUNKFLAG_VITAL) != 0 && m_aClients[ClientID].m_State >= CClient::STATE_READY)
			GameServer()->OnMessage(Msg, &Unpacker, ClientID);
	}
}

bool CServer::RateLimitServerInfoConnless()
{
	bool SendClients = true;
	if(g_Config.m_SvServerInfoPerSecond)
	{
		SendClients = m_ServerInfoNumRequests <= g_Config.m_SvServerInfoPerSecond;
		const int64_t Now = Tick();

		if(Now <= m_ServerInfoFirstRequest + TickSpeed())
		{
			m_ServerInfoNumRequests++;
		}
		else
		{
			m_ServerInfoNumRequests = 1;
			m_ServerInfoFirstRequest = Now;
		}
	}

	return SendClients;
}

void CServer::SendServerInfoConnless(const NETADDR *pAddr, int Token, int Type)
{
	SendServerInfo(pAddr, Token, Type, RateLimitServerInfoConnless());
}

static inline int GetCacheIndex(int Type, bool SendClient)
{
	if(Type == SERVERINFO_INGAME)
		Type = SERVERINFO_VANILLA;
	else if(Type == SERVERINFO_EXTENDED_MORE)
		Type = SERVERINFO_EXTENDED;

	return Type * 2 + SendClient;
}

CServer::CCache::CCache()
{
	m_Cache.clear();
}

CServer::CCache::~CCache()
{
	Clear();
}

CServer::CCache::CCacheChunk::CCacheChunk(const void *pData, int Size)
{
	m_vData.assign((const uint8_t *)pData, (const uint8_t *)pData + Size);
}

void CServer::CCache::AddChunk(const void *pData, int Size)
{
	m_Cache.emplace_back(pData, Size);
}

void CServer::CCache::Clear()
{
	m_Cache.clear();
}

void CServer::CacheServerInfo(CCache *pCache, int Type, bool SendClients)
{
	pCache->Clear();

	// One chance to improve the protocol!
	CPacker p;
	char aBuf[128];

	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	p.Reset();

#define ADD_RAW(p, x) (p).AddRaw(x, sizeof(x))
#define ADD_INT(p, x) \
	do \
	{ \
		str_format(aBuf, sizeof(aBuf), "%d", x); \
		(p).AddString(aBuf, 0); \
	} while(0)

	p.AddString(GameServer()->Version(), 32);
	if(Type != SERVERINFO_VANILLA)
	{
		p.AddString(g_Config.m_SvName, 256);
	}
	else
	{
		if(m_NetServer.MaxClients() <= VANILLA_MAX_CLIENTS)
		{
			p.AddString(g_Config.m_SvName, 64);
		}
		else
		{
			const int MaxClients = max(ClientCount, m_NetServer.MaxClients());
			str_format(aBuf, sizeof(aBuf), "%s [%d/%d]", g_Config.m_SvName, ClientCount, MaxClients);
			p.AddString(aBuf, 64);
		}
	}
	p.AddString(GetMapName(), 32);

	if(Type == SERVERINFO_EXTENDED)
	{
		ADD_INT(p, m_aCurrentMapCrc[MAP_TYPE_SIX]);
		ADD_INT(p, m_aCurrentMapSize[MAP_TYPE_SIX]);
	}

	// gametype
	p.AddString(GameServer()->GameType(), 16);

	// flags
	ADD_INT(p, g_Config.m_Password[0] ? SERVER_FLAG_PASSWORD : 0);

	int MaxClients = m_NetServer.MaxClients();
	// How many clients the used serverinfo protocol supports, has to be tracked
	// separately to make sure we don't subtract the reserved slots from it
	int MaxClientsProtocol = MAX_CLIENTS;
	if(Type == SERVERINFO_VANILLA || Type == SERVERINFO_INGAME)
	{
		if(ClientCount >= VANILLA_MAX_CLIENTS)
		{
			if(ClientCount < MaxClients)
				ClientCount = VANILLA_MAX_CLIENTS - 1;
			else
				ClientCount = VANILLA_MAX_CLIENTS;
		}
		MaxClientsProtocol = VANILLA_MAX_CLIENTS;
		if(PlayerCount > ClientCount)
			PlayerCount = ClientCount;
	}

	ADD_INT(p, PlayerCount); // num players
	ADD_INT(p, minimum(MaxClientsProtocol, max(MaxClients - g_Config.m_SvSpectatorSlots, PlayerCount))); // max players
	ADD_INT(p, ClientCount); // num clients
	ADD_INT(p, minimum(MaxClientsProtocol, max(MaxClients, ClientCount))); // max clients

	if(Type == SERVERINFO_EXTENDED)
		p.AddString("", 0); // extra info, reserved

	const void *pPrefix = p.Data();
	int PrefixSize = p.Size();

	CPacker q;
	int ChunksStored = 0;
	int PlayersStored = 0;

#define SAVE(size) \
	do \
	{ \
		pCache->AddChunk(q.Data(), size); \
		ChunksStored++; \
	} while(0)

#define RESET() \
	do \
	{ \
		q.Reset(); \
		q.AddRaw(pPrefix, PrefixSize); \
	} while(0)

	RESET();

	if(Type == SERVERINFO_64_LEGACY)
		q.AddInt(PlayersStored); // offset

	if(!SendClients)
	{
		SAVE(q.Size());
		return;
	}

	if(Type == SERVERINFO_EXTENDED)
	{
		pPrefix = "";
		PrefixSize = 0;
	}

	int Remaining;
	switch(Type)
	{
	case SERVERINFO_EXTENDED: Remaining = -1; break;
	case SERVERINFO_64_LEGACY: Remaining = 24; break;
	case SERVERINFO_VANILLA: Remaining = VANILLA_MAX_CLIENTS; break;
	case SERVERINFO_INGAME: Remaining = VANILLA_MAX_CLIENTS; break;
	default: dbg_assert(0, "caught earlier, unreachable"); return;
	}

	// Use the following strategy for sending:
	// For vanilla, send the first 16 players.
	// For legacy 64p, send 24 players per packet.
	// For extended, send as much players as possible.

	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(Remaining == 0)
			{
				if(Type == SERVERINFO_VANILLA || Type == SERVERINFO_INGAME)
					break;

				// Otherwise we're SERVERINFO_64_LEGACY.
				SAVE(q.Size());
				RESET();
				q.AddInt(PlayersStored); // offset
				Remaining = 24;
			}
			if(Remaining > 0)
			{
				Remaining--;
			}

			int PreviousSize = q.Size();

			q.AddString(ClientName(i), MAX_NAME_LENGTH); // client name
			q.AddString(ClientClan(i), MAX_CLAN_LENGTH); // client clan

			ADD_INT(q, m_aClients[i].m_Country); // client country
			ADD_INT(q, m_aClients[i].m_Score); // client score
			ADD_INT(q, GameServer()->IsClientPlayer(i) ? 1 : 0); // is player?
			if(Type == SERVERINFO_EXTENDED)
				q.AddString("", 0); // extra info, reserved

			if(Type == SERVERINFO_EXTENDED)
			{
				if(q.Size() >= NET_MAX_PAYLOAD - 18) // 8 bytes for type, 10 bytes for the largest token
				{
					// Retry current player.
					i--;
					SAVE(PreviousSize);
					RESET();
					ADD_INT(q, ChunksStored);
					q.AddString("", 0); // extra info, reserved
					continue;
				}
			}
			PlayersStored++;
		}
	}

	SAVE(q.Size());
#undef SAVE
#undef RESET
#undef ADD_RAW
#undef ADD_INT
}

void CServer::CacheServerInfoSixup(CCache *pCache, bool SendClients)
{
	pCache->Clear();

	CPacker Packer;
	Packer.Reset();

	// Could be moved to a separate function and cached
	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	char aVersion[32];
	str_format(aVersion, sizeof(aVersion), "0.7↔%s", GameServer()->Version());
	Packer.AddString(aVersion, 32);
	Packer.AddString(g_Config.m_SvName, 64);
	Packer.AddString(g_Config.m_SvHostname, 128);
	Packer.AddString(GetMapName(), 32);

	// gametype
	Packer.AddString(GameServer()->GameType(), 16);

	// flags
	int Flags = 0;
	if(g_Config.m_Password[0]) // password set
		Flags |= SERVER_FLAG_PASSWORD;
	Packer.AddInt(Flags);

	int MaxClients = m_NetServer.MaxClients();
	Packer.AddInt(1); // server skill level
	Packer.AddInt(PlayerCount); // num players
	Packer.AddInt(maximum(MaxClients - g_Config.m_SvSpectatorSlots, PlayerCount)); // max players
	Packer.AddInt(ClientCount); // num clients
	Packer.AddInt(maximum(MaxClients, ClientCount)); // max clients

	if(SendClients)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			{
				Packer.AddString(ClientName(i), MAX_NAME_LENGTH); // client name
				Packer.AddString(ClientClan(i), MAX_CLAN_LENGTH); // client clan
				Packer.AddInt(m_aClients[i].m_Country); // client country
				Packer.AddInt(m_aClients[i].m_Score == -9999 ? -1 : -m_aClients[i].m_Score); // client score
				Packer.AddInt(GameServer()->IsClientPlayer(i) ? 0 : 1); // flag spectator=1, bot=2 (player=0)
			}
		}
	}

	pCache->AddChunk(Packer.Data(), Packer.Size());
}

void CServer::SendServerInfo(const NETADDR *pAddr, int Token, int Type, bool SendClients)
{
	CPacker p;
	char aBuf[128];
	p.Reset();

	CCache *pCache = &m_aServerInfoCache[GetCacheIndex(Type, SendClients)];

#define ADD_RAW(p, x) (p).AddRaw(x, sizeof(x))
#define ADD_INT(p, x) \
	do \
	{ \
		str_format(aBuf, sizeof(aBuf), "%d", x); \
		(p).AddString(aBuf, 0); \
	} while(0)

	CNetChunk Packet;
	Packet.m_ClientID = -1;
	Packet.m_Address = *pAddr;
	Packet.m_Flags = NETSENDFLAG_CONNLESS;

	for(const auto &Chunk : pCache->m_Cache)
	{
		p.Reset();
		if(Type == SERVERINFO_EXTENDED)
		{
			if(&Chunk == &pCache->m_Cache.front())
				p.AddRaw(SERVERBROWSE_INFO_EXTENDED, sizeof(SERVERBROWSE_INFO_EXTENDED));
			else
				p.AddRaw(SERVERBROWSE_INFO_EXTENDED_MORE, sizeof(SERVERBROWSE_INFO_EXTENDED_MORE));
			ADD_INT(p, Token);
		}
		else if(Type == SERVERINFO_64_LEGACY)
		{
			ADD_RAW(p, SERVERBROWSE_INFO_64_LEGACY);
			ADD_INT(p, Token);
		}
		else if(Type == SERVERINFO_VANILLA || Type == SERVERINFO_INGAME)
		{
			ADD_RAW(p, SERVERBROWSE_INFO);
			ADD_INT(p, Token);
		}
		else
		{
			dbg_assert(false, "unknown serverinfo type");
		}

		p.AddRaw(Chunk.m_vData.data(), Chunk.m_vData.size());
		Packet.m_pData = p.Data();
		Packet.m_DataSize = p.Size();
		m_NetServer.Send(&Packet);
	}
}

void CServer::GetServerInfoSixup(CPacker *pPacker, int Token, bool SendClients)
{
	if(Token != -1)
	{
		pPacker->Reset();
		pPacker->AddRaw(SERVERBROWSE_INFO, sizeof(SERVERBROWSE_INFO));
		pPacker->AddInt(Token);
	}

	SendClients = SendClients && Token != -1;

	CCache::CCacheChunk &FirstChunk = m_aSixupServerInfoCache[SendClients].m_Cache.front();
	pPacker->AddRaw(FirstChunk.m_vData.data(), FirstChunk.m_vData.size());
}

void CServer::ExpireServerInfo()
{
	m_ServerInfoNeedsUpdate = true;
}

void CServer::UpdateRegisterServerInfo()
{
	// count the players
	int PlayerCount = 0, ClientCount = 0;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			if(GameServer()->IsClientPlayer(i))
				PlayerCount++;

			ClientCount++;
		}
	}

	int MaxPlayers = max(m_NetServer.MaxClients() - g_Config.m_SvSpectatorSlots, PlayerCount);
	int MaxClients = max(m_NetServer.MaxClients(), ClientCount);
	char aName[256];
	char aGameType[32];
	char aMapName[64];
	char aVersion[64];
	char aMapSha256[SHA256_MAXSTRSIZE];

	sha256_str(m_aCurrentMapSha256[MAP_TYPE_SIX], aMapSha256, sizeof(aMapSha256));

	char aInfo[16384];
	str_format(aInfo, sizeof(aInfo),
		"{"
		"\"max_clients\":%d,"
		"\"max_players\":%d,"
		"\"passworded\":%s,"
		"\"game_type\":\"%s\","
		"\"name\":\"%s\","
		"\"map\":{"
		"\"name\":\"%s\","
		"\"sha256\":\"%s\","
		"\"size\":%d"
		"},"
		"\"version\":\"%s\","
		"\"client_score_kind\":\"points\","
		"\"clients\":[",
		MaxClients,
		MaxPlayers,
		JsonBool(g_Config.m_Password[0]),
		EscapeJson(aGameType, sizeof(aGameType), GameServer()->GameType()),
		EscapeJson(aName, sizeof(aName), g_Config.m_SvName),
		EscapeJson(aMapName, sizeof(aMapName), GetMapName()),
		aMapSha256,
		m_aCurrentMapSize[MAP_TYPE_SIX],
		EscapeJson(aVersion, sizeof(aVersion), GameServer()->Version()));

	bool FirstPlayer = true;
	for(int i = 0; i < MAX_PLAYERS; i++)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			char aCName[32];
			char aCClan[32];

			char aExtraPlayerInfo[512];
			GameServer()->OnUpdatePlayerServerInfo(aExtraPlayerInfo, sizeof(aExtraPlayerInfo), i);

			char aClientInfo[256];
			str_format(aClientInfo, sizeof(aClientInfo),
				"%s{"
				"\"name\":\"%s\","
				"\"clan\":\"%s\","
				"\"country\":%d,"
				"\"score\":%d,"
				"\"is_player\":%s"
				"%s"
				"}",
				!FirstPlayer ? "," : "",
				EscapeJson(aCName, sizeof(aCName), ClientName(i)),
				EscapeJson(aCClan, sizeof(aCClan), ClientClan(i)),
				m_aClients[i].m_Country,
				m_aClients[i].m_Score,
				JsonBool(GameServer()->IsClientPlayer(i)),
				aExtraPlayerInfo);
			str_append(aInfo, aClientInfo, sizeof(aInfo));
			FirstPlayer = false;
		}
	}

	str_append(aInfo, "]}", sizeof(aInfo));

	m_pRegister->OnNewInfo(aInfo);
}

void CServer::UpdateServerInfo(bool Resend)
{
	if(!m_pRegister)
		return;

	UpdateRegisterServerInfo();

	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 2; j++)
			CacheServerInfo(&m_aServerInfoCache[i * 2 + j], i, j);

	for(int i = 0; i < 2; i++)
		CacheServerInfoSixup(&m_aSixupServerInfoCache[i], i);

	if(Resend)
	{
		for(int i = 0; i < MaxClients(); ++i)
		{
			if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			{
				if(!IsSixup(i))
					SendServerInfo(m_NetServer.ClientAddr(i), -1, SERVERINFO_INGAME, false);
				else
				{
					CMsgPacker Msg(4, true, true); //NETMSG_SERVERINFO //TODO: Import the shared protocol from 7 as well
					GetServerInfoSixup(&Msg, -1, false);
					SendMsg(&Msg, MSGFLAG_VITAL | MSGFLAG_FLUSH, i);
				}
			}
		}
	}

	m_ServerInfoNeedsUpdate = false;
}

void CServer::PumpNetwork(bool PacketWaiting)
{
	CNetChunk Packet;
	SECURITY_TOKEN ResponseToken;

	m_NetServer.Update();

	if(PacketWaiting)
	{
		// process packets
		while(m_NetServer.Recv(&Packet, &ResponseToken))
		{
			if(Packet.m_ClientID == -1)
			{
				if(ResponseToken == NET_SECURITY_TOKEN_UNKNOWN && m_pRegister->OnPacket(&Packet))
					continue;

				{
					int ExtraToken = 0;
					int Type = -1;
					if(Packet.m_DataSize >= (int)sizeof(SERVERBROWSE_GETINFO) + 1 &&
						mem_comp(Packet.m_pData, SERVERBROWSE_GETINFO, sizeof(SERVERBROWSE_GETINFO)) == 0)
					{
						if(Packet.m_Flags & NETSENDFLAG_EXTENDED)
						{
							Type = SERVERINFO_EXTENDED;
							ExtraToken = (Packet.m_aExtraData[0] << 8) | Packet.m_aExtraData[1];
						}
						else
							Type = SERVERINFO_VANILLA;
					}
					else if(Packet.m_DataSize >= (int)sizeof(SERVERBROWSE_GETINFO_64_LEGACY) + 1 &&
						mem_comp(Packet.m_pData, SERVERBROWSE_GETINFO_64_LEGACY, sizeof(SERVERBROWSE_GETINFO_64_LEGACY)) == 0)
					{
						Type = SERVERINFO_64_LEGACY;
					}
					if(Type == SERVERINFO_VANILLA && ResponseToken != NET_SECURITY_TOKEN_UNKNOWN)
					{
						CUnpacker Unpacker;
						Unpacker.Reset((unsigned char *)Packet.m_pData + sizeof(SERVERBROWSE_GETINFO), Packet.m_DataSize - sizeof(SERVERBROWSE_GETINFO));
						int SrvBrwsToken = Unpacker.GetInt();
						if(Unpacker.Error())
							continue;

						CPacker Packer;
						CNetChunk Response;

						GetServerInfoSixup(&Packer, SrvBrwsToken, RateLimitServerInfoConnless());

						Response.m_ClientID = -1;
						Response.m_Address = Packet.m_Address;
						Response.m_Flags = NETSENDFLAG_CONNLESS;
						Response.m_pData = Packer.Data();
						Response.m_DataSize = Packer.Size();
						m_NetServer.SendConnlessSixup(&Response, ResponseToken);
					}
					else if(Type != -1)
					{
						int Token = ((unsigned char *)Packet.m_pData)[sizeof(SERVERBROWSE_GETINFO)];
						Token |= ExtraToken << 8;
						SendServerInfoConnless(&Packet.m_Address, Token, Type);
					}
				}
			}
			else
			{
				int GameFlags = 0;
				if(Packet.m_Flags & NET_CHUNKFLAG_VITAL)
				{
					GameFlags |= MSGFLAG_VITAL;
				}

				ProcessClientPacket(&Packet);
			}
		}
	}
	{
		unsigned char aBuffer[NET_MAX_PAYLOAD];
		mem_zero(&Packet, sizeof(Packet));
		Packet.m_pData = aBuffer;
	}

	m_ServerBan.Update();
	m_Econ.Update();
}

int* CServer::GetIdMap(int ClientID)
{
	return (int*)(IdMap + VANILLA_MAX_CLIENTS * ClientID);
}

char *CServer::GetMapName()
{
	// get the name of the map without his path
	char *pMapShortName = &g_Config.m_SvMap[0];
	for(int i = 0; i < str_length(g_Config.m_SvMap)-1; i++)
	{
		if(g_Config.m_SvMap[i] == '/' || g_Config.m_SvMap[i] == '\\')
			pMapShortName = &g_Config.m_SvMap[i+1];
	}
	return pMapShortName;
}

int CServer::LoadMap(const char *pMapName)
{
	m_MapReload = false;
	//DATAFILE *df;
	char aBuf[512];
	str_format(aBuf, sizeof(aBuf), "maps/%s.map", pMapName);

	/*df = datafile_load(buf);
	if(!df)
		return 0;*/

	if(!m_pMap->Load(aBuf))
		return 0;

	// stop recording when we change map
	m_DemoRecorder.Stop();

	// reinit snapshot ids
	m_IDPool.TimeoutIDs();

	{
		CDataFileReader dfServerMap;
		dfServerMap.Open(Storage(), aBuf, IStorage::TYPE_ALL);
		unsigned ServerMapCrc = dfServerMap.Crc();
		SHA256_DIGEST ServerMapSha256 = dfServerMap.Sha256();
		char aServerSha256[SHA256_MAXSTRSIZE];
		sha256_str(ServerMapSha256, aServerSha256, sizeof(aServerSha256));
		dfServerMap.Close();
		
		char aClientMapName[256];
		str_format(aClientMapName, sizeof(aClientMapName), "clientmaps/next/%s.map", pMapName);
		
		CMapConverter MapConverter(Storage(), m_pMap, Console(), Classes());
		if(!MapConverter.Load())
			return 0;
			
		m_TimeShiftUnit = MapConverter.GetTimeShiftUnit();
		
		CDataFileReader dfClientMap;
		//The map must be converted
		
		char aClientMapDir[256];
		str_format(aClientMapDir, sizeof(aClientMapDir), "clientmaps");
		
		if(!Storage()->CreateFolder(aClientMapDir, IStorage::TYPE_SAVE))
		{
			dbg_msg("infNext", "Can't create the directory '%s'", aClientMapDir);
		}
		
		str_format(aClientMapDir, sizeof(aClientMapDir), "clientmaps/next");
		
		if(!Storage()->CreateFolder(aClientMapDir, IStorage::TYPE_SAVE))
		{
			dbg_msg("infNext", "Can't create the directory '%s'", aClientMapDir);
		}
				
		if(!MapConverter.CreateMap(aClientMapName, pMapName))
			return 0;
			
		CDataFileReader dfGeneratedMap;
		dfGeneratedMap.Open(Storage(), aClientMapName, IStorage::TYPE_ALL);
		m_aCurrentMapCrc[MAP_TYPE_SIX] = dfGeneratedMap.Crc();

		m_aCurrentMapSha256[MAP_TYPE_SIX] = dfGeneratedMap.Sha256();
		dfGeneratedMap.Close();
		char aSha256[SHA256_MAXSTRSIZE];
		sha256_str(m_aCurrentMapSha256[MAP_TYPE_SIX], aSha256, sizeof(aSha256));
	
		char aBufMsg[128];
		str_format(aBufMsg, sizeof(aBufMsg), "map crc is %08x, generated map crc is %08x", ServerMapCrc, m_aCurrentMapCrc[MAP_TYPE_SIX]);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBufMsg);
		str_format(aBufMsg, sizeof(aBufMsg), "map sha256 is %s, generated map sha256 is %s", aServerSha256, aSha256);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBufMsg);
		
		//Download the generated map in memory to send it to clients
		IOHANDLE File = Storage()->OpenFile(aClientMapName, IOFLAG_READ, IStorage::TYPE_ALL);
		m_aCurrentMapSize[MAP_TYPE_SIX] = (int)io_length(File);
		if(m_apCurrentMapData[MAP_TYPE_SIX])
			mem_free(m_apCurrentMapData[MAP_TYPE_SIX]);
		m_apCurrentMapData[MAP_TYPE_SIX] = (unsigned char *)mem_alloc(m_aCurrentMapSize[MAP_TYPE_SIX], 1);
		io_read(File, m_apCurrentMapData[MAP_TYPE_SIX], m_aCurrentMapSize[MAP_TYPE_SIX]);
		io_close(File);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", "map 06 version loaded in memory");
	}
	str_copy(m_aCurrentMap, pMapName);
	// load complete map07 into memory for download
	{
		char aBufMsg[128];
		str_format(aBuf, sizeof(aBuf), "clientmaps/next/%s7.map", pMapName);
		IOHANDLE File = Storage()->OpenFile(aBuf, IOFLAG_READ, IStorage::TYPE_ALL);
		m_aCurrentMapSize[MAP_TYPE_SIXUP] = (int)io_length(File);
		if(m_apCurrentMapData[MAP_TYPE_SIXUP])
			mem_free(m_apCurrentMapData[MAP_TYPE_SIXUP]);
		m_apCurrentMapData[MAP_TYPE_SIXUP] = (unsigned char *)mem_alloc(m_aCurrentMapSize[MAP_TYPE_SIXUP], 1);
		io_read(File, m_apCurrentMapData[MAP_TYPE_SIXUP], m_aCurrentMapSize[MAP_TYPE_SIXUP]);
		io_close(File);
		
		m_aCurrentMapSha256[MAP_TYPE_SIXUP] = sha256(m_apCurrentMapData[MAP_TYPE_SIXUP], m_aCurrentMapSize[MAP_TYPE_SIXUP]);
		m_aCurrentMapCrc[MAP_TYPE_SIXUP] = crc32(0, m_apCurrentMapData[MAP_TYPE_SIXUP], m_aCurrentMapSize[MAP_TYPE_SIXUP]);
		
		char aSha256[SHA256_MAXSTRSIZE];
		sha256_str(m_aCurrentMapSha256[MAP_TYPE_SIXUP], aSha256, sizeof(aSha256));
		
		str_format(aBufMsg, sizeof(aBufMsg), "%s sha256 is %s", aBuf, aSha256);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "sixup", aBufMsg);
	}

	LoadMapConfig(pMapName);
	return 1;
}

void CServer::LoadMapConfig(const char *pMapName)
{
	char aBuf[512];
	str_copy(aBuf, "maps/map.json");

	IOHANDLE File = Storage()->OpenFile(aBuf, IOFLAG_READ, CStorage::TYPE_ALL);
	if(!File)
	{
		str_format(aBuf, sizeof(aBuf), "Can't load the map config file %s", aBuf);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
		return;
	}

	// load the file as a string
	int FileSize = (int)io_length(File);
	int FileDataSize = FileSize + 1;
	char *pFileData = new char[FileDataSize];
	io_read(File, pFileData, FileSize);
	pFileData[FileSize] = 0;
	io_close(File);

	// parse json data
	json_settings JsonSettings;
	mem_zero(&JsonSettings, sizeof(JsonSettings));
	char aError[256];
	json_value *pJsonData = json_parse_ex(&JsonSettings, pFileData, aError);
	if(pJsonData == 0)
	{
		str_format(aBuf, sizeof(aBuf), "Can't load the map config file %s : %s", aBuf, aError);
		Console()->Print(IConsole::OUTPUT_LEVEL_ADDINFO, "server", aBuf);
		delete[] pFileData;
		return;
	}

	const json_value &rStart = (*pJsonData)["maps"];
	if(rStart.type == json_array)
	{
		for(unsigned i = 0; i < rStart.u.array.length; ++i)
		{
			if(str_comp((const char *)rStart[i]["name"], pMapName))
				continue;
			long TimeLimit = (long)rStart[i]["timelimit"];

			if(TimeLimit)
			{
				g_Config.m_SvTimelimit = TimeLimit;
				dbg_msg("server", "Time limit: %dmin", TimeLimit);
			}
		}
	}

	// clean up
	json_value_free(pJsonData);
	delete[] pFileData;
	
	return;

}

int CServer::Run()
{
	//
	m_PrintCBIndex = Console()->RegisterPrintCallback(g_Config.m_ConsoleOutputLevel, SendRconLineAuthed, this);

	InitClasses();
	// load map
	if(!LoadMap(g_Config.m_SvMap))
	{
		dbg_msg("server", "failed to load map. mapname='%s'", g_Config.m_SvMap);
		return -1;
	}

	// start server
	NETADDR BindAddr;
	if(g_Config.m_Bindaddr[0] && net_host_lookup(g_Config.m_Bindaddr, &BindAddr, NETTYPE_ALL) == 0)
	{
		// sweet!
		BindAddr.type = NETTYPE_ALL;
		BindAddr.port = g_Config.m_SvPort;
	}
	else
	{
		mem_zero(&BindAddr, sizeof(BindAddr));
		BindAddr.type = NETTYPE_ALL;
		BindAddr.port = g_Config.m_SvPort;
	}

	int Port = g_Config.m_SvPort;
	for(BindAddr.port = Port != 0 ? Port : 8303; !m_NetServer.Open(BindAddr, &m_ServerBan, g_Config.m_SvMaxClients, g_Config.m_SvMaxClientsPerIP); BindAddr.port++)
	{
		if(Port != 0 || BindAddr.port >= 8310)
		{
			dbg_msg("server", "couldn't open socket. port %d might already be in use", BindAddr.port);
			return -1;
		}
	}


	IEngine *pEngine = Kernel()->RequestInterface<IEngine>();
	m_pRegister = CreateRegister(m_pConsole, pEngine, g_Config.m_SvPort, m_NetServer.GetGlobalToken());

	m_NetServer.SetCallbacks(NewClientCallback, NewClientNoAuthCallback, ClientRejoinCallback, DelClientCallback, this);

	m_Econ.Init(Console(), &m_ServerBan);

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "server name is '%s'", g_Config.m_SvName);
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	GameServer()->OnInit();
	str_format(aBuf, sizeof(aBuf), "version %s", GameServer()->NetVersion());
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);

	// process pending commands
	m_pConsole->StoreCommands(false);
	m_pRegister->OnConfigChange();

	// start game
	{
		bool NonActive = false;
		bool PacketWaiting = false;

		m_GameStartTime = time_get();

		UpdateServerInfo();
		while(m_RunServer)
		{
			if(NonActive)
				PumpNetwork(PacketWaiting);

			set_new_tick();

			int64_t t = time_get();
			int NewTicks = 0;

			// load new map
			if(m_MapReload || m_CurrentGameTick >= 0x6FFFFFFF) // force reload to make sure the ticks stay within a valid range
			{
				// load map
				if(LoadMap(g_Config.m_SvMap))
				{
					// new map loaded
					GameServer()->OnShutdown();

					for(int ClientID = 0; ClientID < MAX_CLIENTS; ClientID++)
					{
						if(m_aClients[ClientID].m_State <= CClient::STATE_AUTH)
							continue;

						SendMap(ClientID);
						m_aClients[ClientID].Reset();
						m_aClients[ClientID].m_State = CClient::STATE_CONNECTING;
					}

					m_GameStartTime = time_get();
					m_CurrentGameTick = 0;
					m_ServerInfoFirstRequest = 0;
					Kernel()->ReregisterInterface(GameServer());
					GameServer()->OnInit();
					UpdateServerInfo(true);
				}
				else
				{
					str_format(aBuf, sizeof(aBuf), "failed to load map. mapname='%s'", g_Config.m_SvMap);
					Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
					str_copy(g_Config.m_SvMap, m_aCurrentMap, sizeof(g_Config.m_SvMap));
				}
			}

			while(t > TickStartTime(m_CurrentGameTick + 1))
			{
				m_CurrentGameTick++;
				NewTicks++;

				// apply new input
				for(int c = 0; c < g_Config.m_SvMaxClients; c++)
				{
					if(m_aClients[c].m_State != CClient::STATE_INGAME)
						continue;
					for(auto &Input : m_aClients[c].m_aInputs)
					{
						if(Input.m_GameTick == Tick())
						{
							GameServer()->OnClientPredictedInput(c, Input.m_aData);
							break;
						}
					}
				}

				GameServer()->OnTick();
			}

			// snap game
			if(NewTicks)
			{
				if(g_Config.m_SvHighBandwidth || (m_CurrentGameTick % 2) == 0)
					DoSnapshot();

				UpdateClientRconCommands();
			}

			// master server stuff
			m_pRegister->Update();

			if(m_ServerInfoNeedsUpdate)
				UpdateServerInfo();

			if(!NonActive)
				PumpNetwork(PacketWaiting);

			NonActive = true;

			for(auto &Client : m_aClients)
			{
				if(Client.m_State != CClient::STATE_EMPTY)
				{
					NonActive = false;
					break;
				}
			}

			// wait for incoming data
			if(NonActive)
			{
				if(g_Config.m_SvReloadWhenEmpty == 1)
				{
					m_MapReload = true;
					g_Config.m_SvReloadWhenEmpty = 0;
				}
				else if(g_Config.m_SvReloadWhenEmpty == 2 && !m_ReloadedWhenEmpty)
				{
					m_MapReload = true;
					m_ReloadedWhenEmpty = true;
				}

				if(g_Config.m_SvShutdownWhenEmpty)
					m_RunServer = 0;
				else
					PacketWaiting = net_socket_read_wait(m_NetServer.Socket(), 1000000);
			}
			else
			{
				m_ReloadedWhenEmpty = false;

				set_new_tick();
				t = time_get();
				int x = (TickStartTime(m_CurrentGameTick + 1) - t) * 1000000 / time_freq() + 1;

				PacketWaiting = x > 0 ? net_socket_read_wait(m_NetServer.Socket(), x) : true;
			}
		}
	}
	// disconnect all clients on shutdown
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(m_aClients[i].m_State != CClient::STATE_EMPTY)
			m_NetServer.Drop(i, "Server shutdown");

		m_Econ.Shutdown();
	}

	GameServer()->OnShutdown();
	m_pMap->Unload();

	if(m_apCurrentMapData[MAP_TYPE_SIX])
		mem_free(m_apCurrentMapData[MAP_TYPE_SIX]);
	if(m_apCurrentMapData[MAP_TYPE_SIXUP])
		mem_free(m_apCurrentMapData[MAP_TYPE_SIXUP]);

	m_pRegister->OnShutdown();
	return 0;
}

void CServer::ConKick(IConsole::IResult *pResult, void *pUser)
{
	if(pResult->NumArguments() > 1)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "Kicked (%s)", pResult->GetString(1));
		((CServer *)pUser)->Kick(pResult->GetInteger(0), aBuf);
	}
	else
		((CServer *)pUser)->Kick(pResult->GetInteger(0), "Kicked by console");
}

void CServer::ConStatus(IConsole::IResult *pResult, void *pUser)
{
	char aBuf[1024];
	char aAddrStr[NETADDR_MAXSTRSIZE];
	CServer* pThis = static_cast<CServer *>(pUser);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(pThis->m_aClients[i].m_State != CClient::STATE_EMPTY)
		{
			net_addr_str(pThis->m_NetServer.ClientAddr(i), aAddrStr, sizeof(aAddrStr), true);
			if(pThis->m_aClients[i].m_State == CClient::STATE_INGAME)
			{
				const char *pAuthStr = pThis->m_aClients[i].m_Authed == CServer::AUTHED_ADMIN ? "(Admin)" :
										pThis->m_aClients[i].m_Authed == CServer::AUTHED_MOD ? "(Mod)" : "";
				str_format(aBuf, sizeof(aBuf), "id=%d addr=%s name='%s' score=%d secure=%s %s", i, aAddrStr,
					pThis->m_aClients[i].m_aName, pThis->m_aClients[i].m_Score, pThis->m_NetServer.HasSecurityToken(i) ? "yes":"no", pAuthStr);
			}
			else
				str_format(aBuf, sizeof(aBuf), "id=%d addr=%s connecting", i, aAddrStr);
			pThis->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "Server", aBuf);
		}
	}
}

void CServer::ConShutdown(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_RunServer = 0;
}

void CServer::DemoRecorder_HandleAutoStart()
{
	if(g_Config.m_SvAutoDemoRecord)
	{
		m_DemoRecorder.Stop();
		char aFilename[128];
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "demos/%s_%s.demo", "auto/autorecord", aDate);
		m_DemoRecorder.Start(Storage(), m_pConsole, aFilename, GameServer()->NetVersion(), m_aCurrentMap, m_aCurrentMapCrc[MAP_TYPE_SIX], "server");
		if(g_Config.m_SvAutoDemoMax)
		{
			// clean up auto recorded demos
			CFileCollection AutoDemos;
			AutoDemos.Init(Storage(), "demos/server", "autorecord", ".demo", g_Config.m_SvAutoDemoMax);
		}
	}
}

bool CServer::DemoRecorder_IsRecording()
{
	return m_DemoRecorder.IsRecording();
}

void CServer::ConRecord(IConsole::IResult *pResult, void *pUser)
{
	CServer* pServer = (CServer *)pUser;
	char aFilename[128];

	if(pResult->NumArguments())
		str_format(aFilename, sizeof(aFilename), "demos/%s.demo", pResult->GetString(0));
	else
	{
		char aDate[20];
		str_timestamp(aDate, sizeof(aDate));
		str_format(aFilename, sizeof(aFilename), "demos/demo_%s.demo", aDate);
	}
	pServer->m_DemoRecorder.Start(pServer->Storage(), pServer->Console(), aFilename, pServer->GameServer()->NetVersion(), pServer->m_aCurrentMap, pServer->m_aCurrentMapCrc[MAP_TYPE_SIX], "server");
}

void CServer::ConStopRecord(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_DemoRecorder.Stop();
}

void CServer::ConMapReload(IConsole::IResult *pResult, void *pUser)
{
	((CServer *)pUser)->m_MapReload = 1;
}

void CServer::ConLogout(IConsole::IResult *pResult, void *pUser)
{
	CServer *pServer = (CServer *)pUser;

	if(pServer->m_RconClientID >= 0 && pServer->m_RconClientID < MAX_CLIENTS &&
		pServer->m_aClients[pServer->m_RconClientID].m_State != CServer::CClient::STATE_EMPTY)
	{
		if(!pServer->IsSixup(pServer->m_RconClientID))
		{
			CMsgPacker Msg(NETMSG_RCON_AUTH_STATUS, true);
			Msg.AddInt(0); //authed
			Msg.AddInt(0); //cmdlist
			pServer->SendMsg(&Msg, MSGFLAG_VITAL, pServer->m_RconClientID);
		}
		else
		{
			CMsgPacker Msg(12, true, true); //NETMSG_RCON_AUTH_OFF
			pServer->SendMsg(&Msg, MSGFLAG_VITAL, pServer->m_RconClientID);
		}

		pServer->m_aClients[pServer->m_RconClientID].m_Authed = AUTHED_NO;
		pServer->m_aClients[pServer->m_RconClientID].m_AuthTries = 0;
		pServer->m_aClients[pServer->m_RconClientID].m_pRconCmdToSend = 0;
		pServer->SendRconLine(pServer->m_RconClientID, "Logout successful.");
		char aBuf[32];
		str_format(aBuf, sizeof(aBuf), "ClientID=%d logged out", pServer->m_RconClientID);
		pServer->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "server", aBuf);
	}
}

void CServer::ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CServer *)pUserData)->UpdateServerInfo(true);
}

void CServer::ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments())
		((CServer *)pUserData)->m_NetServer.SetMaxClientsPerIP(pResult->GetInteger(0));
}

void CServer::ConchainModCommandUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	if(pResult->NumArguments() == 2)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		const IConsole::CCommandInfo *pInfo = pThis->Console()->GetCommandInfo(pResult->GetString(0), CFGFLAG_SERVER, false);
		int OldAccessLevel = 0;
		if(pInfo)
			OldAccessLevel = pInfo->GetAccessLevel();
		pfnCallback(pResult, pCallbackUserData);
		if(pInfo && OldAccessLevel != pInfo->GetAccessLevel())
		{
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{
				if(pThis->m_aClients[i].m_State == CServer::CClient::STATE_EMPTY || pThis->m_aClients[i].m_Authed != CServer::AUTHED_MOD ||
					(pThis->m_aClients[i].m_pRconCmdToSend && str_comp(pResult->GetString(0), pThis->m_aClients[i].m_pRconCmdToSend->m_pName) >= 0))
					continue;

				if(OldAccessLevel == IConsole::ACCESS_LEVEL_ADMIN)
					pThis->SendRconCmdAdd(pInfo, i);
				else
					pThis->SendRconCmdRem(pInfo, i);
			}
		}
	}
	else
		pfnCallback(pResult, pCallbackUserData);
}

void CServer::ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() == 1)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		pThis->Console()->SetPrintOutputLevel(pThis->m_PrintCBIndex, pResult->GetInteger(0));
	}
}

void CServer::ConchainMapUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	if(pResult->NumArguments() >= 1)
	{
		CServer *pThis = static_cast<CServer *>(pUserData);
		pThis->m_MapReload = str_comp(g_Config.m_SvMap, pThis->m_aCurrentMap) != 0;
	}
}

void CServer::RegisterCommands()
{
	m_pConsole = Kernel()->RequestInterface<IConsole>();
	m_pGameServer = Kernel()->RequestInterface<IGameServer>();
	m_pMap = Kernel()->RequestInterface<IEngineMap>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();

	HttpInit(m_pStorage);

	// register console commands
	Console()->Register("kick", "i?r", CFGFLAG_SERVER, ConKick, this, "Kick player with specified id for any reason");
	Console()->Register("status", "", CFGFLAG_SERVER, ConStatus, this, "List players");
	Console()->Register("shutdown", "", CFGFLAG_SERVER, ConShutdown, this, "Shut down");
	Console()->Register("logout", "", CFGFLAG_SERVER, ConLogout, this, "Logout of rcon");

	Console()->Register("record", "?s", CFGFLAG_SERVER|CFGFLAG_STORE, ConRecord, this, "Record to a file");
	Console()->Register("stoprecord", "", CFGFLAG_SERVER, ConStopRecord, this, "Stop recording");

	Console()->Register("reload", "", CFGFLAG_SERVER, ConMapReload, this, "Reload the map");

	Console()->Chain("sv_name", ConchainSpecialInfoupdate, this);
	Console()->Chain("password", ConchainSpecialInfoupdate, this);

	Console()->Chain("sv_max_clients_per_ip", ConchainMaxclientsperipUpdate, this);
	Console()->Chain("mod_command", ConchainModCommandUpdate, this);
	Console()->Chain("console_output_level", ConchainConsoleOutputLevelUpdate, this);
	Console()->Chain("sv_map", ConchainMapUpdate, this);
	// register console commands in sub parts
	m_ServerBan.InitServerBan(Console(), Storage(), this);
	m_pGameServer->OnConsoleInit();
}


int CServer::SnapNewID()
{
	return m_IDPool.NewID();
}

void CServer::SnapFreeID(int ID)
{
	m_IDPool.FreeID(ID);
}


void *CServer::SnapNewItem(int Type, int ID, int Size)
{
	dbg_assert(ID >= 0 && ID <=0xffff, "incorrect id");
	return ID < 0 ? 0 : m_SnapshotBuilder.NewItem(Type, ID, Size);
}

void CServer::SnapSetStaticsize(int ItemType, int Size)
{
	m_SnapshotDelta.SetStaticsize(ItemType, Size);
}

static CServer *CreateServer() { return new CServer(); }

int main(int argc, const char **argv) // ignore_convention
{
#if defined(CONF_FAMILY_WINDOWS)
	for(int i = 1; i < argc; i++) // ignore_convention
	{
		if(str_comp("-s", argv[i]) == 0 || str_comp("--silent", argv[i]) == 0) // ignore_convention
		{
			ShowWindow(GetConsoleWindow(), SW_HIDE);
			break;
		}
	}
#endif

	if(secure_random_init() != 0)
	{
		dbg_msg("secure", "could not initialize secure RNG");
		return -1;
	}

	CServer *pServer = CreateServer();
	IKernel *pKernel = IKernel::Create();

	// create the components
	IEngine *pEngine = CreateEngine("Teeworlds", 2);

	IEngineMap *pEngineMap = CreateEngineMap();
	IGameServer *pGameServer = CreateGameServer();
	IConsole *pConsole = CreateConsole(CFGFLAG_SERVER|CFGFLAG_ECON);
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_SERVER, argc, argv); // ignore_convention
	IConfig *pConfig = CreateConfig();

	{
		bool RegisterFail = false;

		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pServer); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pEngine);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IEngineMap*>(pEngineMap)); // register as both
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(static_cast<IMap*>(pEngineMap));
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pGameServer);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConsole);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pStorage);
		RegisterFail = RegisterFail || !pKernel->RegisterInterface(pConfig);

		if(RegisterFail)
			return -1;
	}

	pEngine->Init();
	pConfig->Init();

	// register all console commands
	pServer->RegisterCommands();

	// execute autoexec file
	pConsole->ExecuteFile("autoexec.cfg");

	pServer->m_pLocalization = new CLocalization(pStorage);
	if(!pServer->m_pLocalization->Init())
	{
		dbg_msg("localization", "could not initialize localization");
		return -1;
	}

	// parse the command line arguments
	if(argc > 1) // ignore_convention
		pConsole->ParseArguments(argc-1, &argv[1]); // ignore_convention

	// restore empty config strings to their defaults
	pConfig->RestoreStrings();

	// run the server
	dbg_msg("server", "starting...");
	pServer->Run();

	// free
	delete pServer->m_pLocalization;
	delete pServer->m_pClasses;
	
	delete pServer;
	delete pKernel;
	delete pEngineMap;
	delete pGameServer;
	delete pConsole;
	delete pStorage;
	delete pConfig;
	return 0;
	
}

void CServer::InitClientBot(int ClientID)
{
	if (ClientID < MAX_PLAYERS || ClientID >= MAX_CLIENTS)
		return;

	m_aClients[ClientID].m_State = CClient::STATE_INGAME;
	m_aClients[ClientID].m_Score = 0;

	SendConnectionReady(ClientID);
}

void CServer::ReloadMap()
{
	m_MapReload = 1;
}

void CServer::InitClasses()
{
	m_pClasses = new CClasses(GameServer()->GameContext());
	m_pClasses->InitClasses();
}