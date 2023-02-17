#include "gamecontext.h"
#include "weapons/hammer.h"
#include "class.h"

CClass::CClass(CGameContext *pGameServer)
{
    m_pGameServer = pGameServer;
}

CClass::~CClass()
{
    for(int i = 0;i < NUM_WEAPONS; i ++)
        if(m_pWeapons[i])
            delete m_pWeapons[i];
}