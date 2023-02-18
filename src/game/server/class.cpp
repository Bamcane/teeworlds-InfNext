#include "gamecontext.h"
#include "weapons/hammer.h"
#include "class.h"

CClass::CClass(CGameContext *pGameServer)
{
    m_pGameServer = pGameServer;
    m_MaxJumpNum = 2;
}

CClass::~CClass()
{
    for(int i = 0;i < NUM_WEAPONS; i ++)
        if(m_pWeapons[i])
            delete m_pWeapons[i];
}

IServer *CClass::Server()
{
    return m_pGameServer->Server(); 
}