#ifndef GAME_SERVER_WEAPON_H
#define GAME_SERVER_WEAPON_H

#include <base/vmath.h>
#include <game/generated/protocol.h>

class CWeapon
{
private:
    class CGameContext *m_pGameServer;
protected:
    int m_MaxAmmo;
	// fire delay(ms)
    int m_FireDelay;
	// AmmoRegen time (ms)
	int m_AmmoRegen;

	int m_InitAmmo;

	bool m_FullAuto;
public:
	CWeapon(class CGameContext *pGameServer);
    
	class IServer *Server();
	class CGameContext *GameServer() { return m_pGameServer; }
	class CGameWorld *GameWorld();

	int GetMaxAmmo() { return m_MaxAmmo; }
	int GetFireDelay() { return m_FireDelay; }
	int GetAmmoRegen() { return m_AmmoRegen; }
	int GetInitAmmo() {return m_InitAmmo; }
	inline bool IsFullAuto() { return m_FullAuto; }

    virtual int WeaponType() { return WEAPON_HAMMER; }
	virtual void Fire(vec2 Pos, vec2 Direction, int Owner) = 0;
};

#endif