#ifndef INFNEXT_CLASS_H
#define INFNEXT_CLASS_H

#include <game/server/teeinfo.h>
#include "weapon.h"

class CGameContext;
class CCharacter;
class CPlayer;
namespace protocol7
{
    class CNetObj_Character;
}

class CClass
{
private:
    CGameContext *m_pGameServer;
    CPlayer *m_pOwner;

protected:
    IServer *Server();
    CGameContext *GameServer() { return m_pGameServer; }
    CPlayer *Player() { return m_pOwner; };
    CCharacter *Character();

public:
    CClass(CGameContext *pGameServer, CPlayer *pOwner);
    virtual ~CClass();

    virtual CClass* CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) 
    { 
        return new CClass(pGameServer, pOwner); 
    }

    // return the score
    virtual int OnPlayerDeath(int KillerID, vec2 Pos) { return 0; }

    virtual bool OnPlayerSpawn(bool IsInfected, vec2* Pos) { return false; }

    virtual void OnTick() {};
    virtual void OnCharacterSnap(class CNetObj_Character *pCharacter) {}
    virtual void OnCharacterSnap(class protocol7::CNetObj_Character *pCharacter) {}
    virtual void OnDDNetCharacterSnap(class CNetObj_DDNetCharacter *pCharacter) {}

    char m_ClassName[32];
    int m_MaxJumpNum;
    bool m_Infect;
    
    CTeeInfo m_Skin;

    CWeapon *m_pWeapons[NUM_WEAPONS];
};

#endif