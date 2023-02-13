#ifndef GAME_SERVER_CLASS_H
#define GAME_SERVER_CLASS_H

#include "teeinfo.h"
#include "weapon.h"

class CClass
{
public:
    CClass(class CGameContext *pGameServer) {};
    CClass() {};
    ~CClass();

    char m_ClassName[32];
    int m_MaxJumpNum;
    bool m_Infect;
    
    CTeeInfo m_Skin;

    CWeapon *m_pWeapons[NUM_WEAPONS];
};

#endif