#include "gamecontext.h"
#include "weapon.h"

CWeapon::CWeapon(CGameContext *pGameServer)
{
    m_pGameServer = pGameServer;
}

CGameWorld *CWeapon::GameWorld()
{
    return &m_pGameServer->m_World;
}