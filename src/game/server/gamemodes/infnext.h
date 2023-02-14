
#ifndef GAME_SERVER_GAMEMODES_INFNEXT_H
#define GAME_SERVER_GAMEMODES_INFNEXT_H

#include <base/tl/array.h>

#include <game/server/class.h>
#include <game/server/gamecontroller.h>

class CGameControllerNext : public IGameController
{
	int m_LastPlayersNum;
public:
	CGameControllerNext(class CGameContext *pGameServer);
	~CGameControllerNext();

	void Tick() override;
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;

	int RoundTick() const;
	int RoundSecond() const;

	class CClassStatus
	{
	public:
		CClassStatus() {};
		CClass *m_pClass;
		int m_Value;
	};

	array<CClassStatus> m_HumanClasses;
	array<CClassStatus> m_InfectClasses;
	
	array<int> m_LastInfect;

	void InitClasses();
	void InitHumanClass(CClass *pClass, bool Enable);
	void InitInfectClass(CClass *pClass, int Proba);

	void SendClassChooser();
	void CheckNoClass();
	void CreateInfects();

	bool PlayerInfected(int ClientID);

	CClass *OnPlayerInfect(class CPlayer *pPlayer) override;
};
#endif
