
#ifndef GAME_SERVER_GAMEMODES_INFNEXT_H
#define GAME_SERVER_GAMEMODES_INFNEXT_H

#include <vector>

#include <game/server/gamecontroller.h>

class CGameControllerNext : public IGameController
{
	int m_LastPlayersNum;
public:
	CGameControllerNext(class CGameContext *pGameServer);
	~CGameControllerNext();

	class CClasses *Classes();

	void Tick() override;
	void Snap(int SnappingClient) override;
	
	int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;

	bool PreSpawn(CPlayer* pPlayer, vec2 *pPos) override;
	bool IsSpawnable(vec2 Pos) override;

	void OnPlayerSelectClass(CPlayer* pPlayer) override;

	int RoundTick() const;
	int RoundSecond() const;

	std::vector<int> m_LastInfect;

	void CheckNoClass();
	void CreateInfects();

	bool PlayerInfected(int ClientID);

	CClass *OnPlayerInfect(class CPlayer *pPlayer) override;
};
#endif
