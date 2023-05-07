/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMECONTEXT_H
#define GAME_SERVER_GAMECONTEXT_H

#include <engine/server.h>
#include <engine/storage.h>
#include <engine/console.h>
#include <engine/shared/memheap.h>

#include <teeuniverses/components/localization.h>
#include <base/tl/array.h>

#include <game/layers.h>
#include <game/voting.h>

#include "eventhandler.h"
#include "gamecontroller.h"
#include "gameworld.h"
#include "player.h"

#include <memory>
#include <string>
#ifdef _MSC_VER
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif
/*
	Tick
		Game Context (CGameContext::tick)
			Game World (GAMEWORLD::tick)
				Reset world if requested (GAMEWORLD::reset)
				All entities in the world (ENTITY::tick)
				All entities in the world (ENTITY::tick_defered)
				Remove entities marked for deletion (GAMEWORLD::remove_entities)
			Game Controller (GAMECONTROLLER::tick)
			All players (CPlayer::tick)


	Snap
		Game Context (CGameContext::snap)
			Game World (GAMEWORLD::snap)
				All entities in the world (ENTITY::snap)
			Game Controller (GAMECONTROLLER::snap)
			Events handler (EVENT_HANDLER::snap)
			All players (CPlayer::snap)

*/
class CGameContext : public IGameServer
{
	IServer *m_pServer;
	IStorage *m_pStorage;
	class IConsole *m_pConsole;
	CLayers m_Layers;
	CCollision m_Collision;
	CNetObjHandler m_NetObjHandler;
	protocol7::CNetObjHandler m_NetObjHandler7;
	CTuningParams m_Tuning;

	static void ConsoleOutputCallback_Chat(const char *pLine, void *pUser);

	static void ConLanguage(IConsole::IResult *pResult, void *pUserData);
	static void ConAbout(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneParam(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneReset(IConsole::IResult *pResult, void *pUserData);
	static void ConTuneDump(IConsole::IResult *pResult, void *pUserData);
	static void ConPause(IConsole::IResult *pResult, void *pUserData);
	static void ConChangeMap(IConsole::IResult *pResult, void *pUserData);
	static void ConRestart(IConsole::IResult *pResult, void *pUserData);
	static void ConBroadcast(IConsole::IResult *pResult, void *pUserData);
	static void ConSay(IConsole::IResult *pResult, void *pUserData);
	static void ConSetTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConAddVote(IConsole::IResult *pResult, void *pUserData);
	static void ConRemoveVote(IConsole::IResult *pResult, void *pUserData);
	static void ConForceVote(IConsole::IResult *pResult, void *pUserData);
	static void ConClearVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConVote(IConsole::IResult *pResult, void *pUserData);
	static void ConAddMapVotes(IConsole::IResult *pResult, void *pUserData);
	static void ConchainSpecialMotdupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	
	CGameContext(int Resetting);
	void Construct(int Resetting);
	void AddVote(const char *pDescription, const char *pCommand);
	static int MapScan(const char *pName, int IsDir, int DirType, void *pUserData);

	bool m_Resetting;

public:
	int m_ChatResponseTargetID;
	int m_ChatPrintCBIndex;

private:
	class CBroadcast
	{
	public:
		std::string m_Text;
		int64_t m_StartTick;
		int m_Type;
		int m_Time;
	};

	struct CPlayerBroadcast
	{
		array<CBroadcast> m_aBroadcast;
		int64_t m_LastBroadcast;
	};

	CPlayerBroadcast m_aBroadcast[MAX_CLIENTS];

	void AddBroadcast(int ClientID, CBroadcast Broadcast);

public:
	int m_ZoneHandle_Next;

	IServer *Server() const { return m_pServer; }
	IStorage *Storage() const { return m_pStorage; }
	class IConsole *Console() { return m_pConsole; }
	CCollision *Collision() { return &m_Collision; }
	CTuningParams *Tuning() { return &m_Tuning; }
	class CLayers *Layers() override { return &m_Layers; }

	CGameContext();
	~CGameContext();

	void Clear();

	CEventHandler m_Events;
	CPlayer *m_apPlayers[MAX_CLIENTS];

	IGameController *m_pController;
	CGameWorld m_World;
	std::vector<std::string> m_vCensorlist;

	CTile *m_pTiles;

	// helper functions
	class CCharacter *GetPlayerChar(int ClientID);

	int m_LockTeams;

	// voting
	void StartVote(const char *pDesc, const char *pCommand, const char *pReason, const char *pSixupDesc);
	void EndVote();
	void SendVoteSet(int ClientID);
	void SendVoteStatus(int ClientID, int Total, int Yes, int No);
	void AbortVoteKickOnDisconnect(int ClientID);

	int m_VoteType;
	int m_VoteCreator;
	int64 m_VoteCloseTime;
	bool m_VoteUpdate;
	int m_VotePos;
	char m_aVoteDescription[VOTE_DESC_LENGTH];
	char m_aSixupVoteDescription[VOTE_DESC_LENGTH];
	char m_aVoteCommand[VOTE_CMD_LENGTH];
	char m_aVoteReason[VOTE_REASON_LENGTH];
	int m_NumVoteOptions;
	int m_VoteEnforce;
	enum
	{
		VOTE_ENFORCE_UNKNOWN=0,
		VOTE_ENFORCE_NO,
		VOTE_ENFORCE_YES,
		VOTE_ENFORCE_ABORT,
	};
	CHeap *m_pVoteOptionHeap;
	CVoteOptionServer *m_pVoteOptionFirst;
	CVoteOptionServer *m_pVoteOptionLast;

	// helper functions
	void CreateDamageInd(vec2 Pos, float AngleMod, int Amount, int64_t Mask=-1LL);
	void CreateExplosion(vec2 Pos, int Owner, int Weapon, bool NoDamage, int64_t Mask=-1LL, int DamageMode=0);
	void CreateHammerHit(vec2 Pos, int64_t Mask=-1LL);
	void CreatePlayerSpawn(vec2 Pos, int64_t Mask=-1LL);
	void CreateDeath(vec2 Pos, int Who, int64_t Mask=-1LL);
	void CreateSound(vec2 Pos, int Sound, int64_t Mask=-1LL);
	void CreateSoundGlobal(int Sound, int Target=-1);


	enum
	{
		CHAT_ALL=-2,
		CHAT_SPEC=-1,
		CHAT_RED=0,
		CHAT_BLUE=1,
		CHAT_WHISPER_SEND = 2,
		CHAT_WHISPER_RECV = 3,

		CHAT_SIX = 1 << 0,
		CHAT_SIXUP = 1 << 1,
	};

	// network
	void SendMotd(int To, const char* pText);
	void SendSettings(int ClientID);
	void SendChatTarget(int To, const char *pText);
	void SendChat(int ClientID, int Team, const char *pText);
	void SendEmoticon(int ClientID, int Emoticon);
	void SendWeaponPickup(int ClientID, int Weapon);
	void SendBroadcast(int ClientID, const char *pText, int Time=150, int Type=0);
	void SendSkinChange(int ClientID, int TargetID);
	void SendClanChange(int ClientID, int TargetID, const char *pClan);

	void SendBroadcast_Localization(int ClientID, const char *pText, int Time, int Type, ...);
	void SendBroadcast_Localization_P(int ClientID, const char* pText, int Time, int Type, int Number, ...);

	void SendChatTarget_Localization(int To, const char *pText, ...);
	void SendChatTarget_Localization_P(int To, const char* pText, int Number, ...);
	
	void SetClientLanguage(int ClientID, const char *pLanguage);

	void UpdateBroadcast(int ClientID);

	const char* Localize(const char *pLanguageCode, const char* pText) const;
	const char* GetPlayerLanguage(int ClientID) const;

	int GetClientVersion(int ClientID) const;
	bool PlayerExists(int ClientID) const override { return m_apPlayers[ClientID]; }

	void Whisper(int ClientID, char *pStr);
	void WhisperID(int ClientID, int VictimID, const char *pMessage);


	//
	void CheckPureTuning();
	void SendTuningParams(int ClientID);

	// engine events
	void OnInit() override;
	void OnConsoleInit() override;
	void OnShutdown() override;

	void OnTick() override;
	void OnPreSnap() override;
	void OnSnap(int ClientID) override;
	void OnPostSnap() override;


	void *PreProcessMsg(int *pMsgID, CUnpacker *pUnpacker, int ClientID);
	void CensorMessage(char *pCensoredMessage, const char *pMessage, int Size);
	void OnMessage(int MsgID, CUnpacker *pUnpacker, int ClientID) override;

	void OnClientConnected(int ClientID) override;
	void OnClientEnter(int ClientID) override;
	void OnClientDrop(int ClientID, const char *pReason) override;
	void OnClientPrepareInput(int ClientID, void *pInput) override;
	void OnClientDirectInput(int ClientID, void *pInput) override;
	void OnClientPredictedInput(int ClientID, void *pInput) override;

	bool IsClientReady(int ClientID) override;
	bool IsClientPlayer(int ClientID) override;

	void OnSetAuthed(int ClientID,int Level) override;
	
	const char *GameType() override;
	const char *Version() override;
	const char *NetVersion() override;

	void OnUpdatePlayerServerInfo(char *aBuf, int BufSize, int ID) override;

	CGameContext *GameContext() override { return this; }

	enum
	{
		VOTE_ENFORCE_NO_ADMIN = VOTE_ENFORCE_YES + 1,
		VOTE_ENFORCE_YES_ADMIN,

		VOTE_TYPE_UNKNOWN = 0,
		VOTE_TYPE_OPTION,
		VOTE_TYPE_KICK,
		VOTE_TYPE_SPECTATE,
	};
	int m_VoteVictim;
	int m_VoteEnforcer;

	inline bool IsOptionVote() const { return m_VoteType == VOTE_TYPE_OPTION; }
	inline bool IsKickVote() const { return m_VoteType == VOTE_TYPE_KICK; }
	inline bool IsSpecVote() const { return m_VoteType == VOTE_TYPE_SPECTATE; }

	void AddMapVotes();
};

inline int64_t CmaskAll() { return -1LL; }
inline int64_t CmaskOne(int ClientID) { return 1LL<<ClientID; }
inline int64_t CmaskAllExceptOne(int ClientID) { return CmaskAll()^CmaskOne(ClientID); }
inline bool CmaskIsSet(int64_t Mask, int ClientID) { return (Mask&CmaskOne(ClientID)) != 0; }

#endif
