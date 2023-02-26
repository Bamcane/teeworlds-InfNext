#include <game/server/gamecontext.h>
#include "weapon.h"

CWeapon::CWeapon(CGameContext *pGameServer)
{
    m_pGameServer = pGameServer;
}

IServer *CWeapon::Server()
{
    return m_pGameServer->Server(); 
}

CGameWorld *CWeapon::GameWorld()
{
    return &m_pGameServer->m_World;
}