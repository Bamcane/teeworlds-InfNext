#include <game/server/gamecontext.h>
#include "weapons/hammer.h"
#include "class.h"

CClass::CClass(CGameContext *pGameServer, CPlayer *pOwner) :
    m_pGameServer(pGameServer),
    m_pOwner(pOwner)
{
    m_MaxJumpNum = 2;

    for(int i = 0;i < NUM_WEAPONS; i ++)
    {
        m_pWeapons[i] = 0;
    }
}

CClass::~CClass()
{
    for(int i = 0;i < NUM_WEAPONS; i ++)
    {
        if(m_pWeapons[i])
            delete m_pWeapons[i];
        m_pWeapons[i] = 0;
    }
}

IServer *CClass::Server()
{
    return m_pGameServer->Server(); 
}

CCharacter *CClass::Character()
{
    return m_pOwner->GetCharacter(); 
}