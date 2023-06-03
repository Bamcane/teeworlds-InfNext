#include <game/server/gamecontext.h>
#include <infnext/weapons/hammer.h>
#include <infnext/weapons/gun.h>
#include <infnext/weapons/sniper-rifle.h>
#include "sniper.h"

CClassSniper::CClassSniper(CGameContext *pGameServer, CPlayer *pOwner) : CClass(pGameServer, pOwner)
{
    str_copy(m_ClassName, _("Sniper"));
    m_MaxJumpNum = 3;
    m_Infect = false;
    m_pWeapons[WEAPON_HAMMER] = new CWeaponHammer(pGameServer);
    m_pWeapons[WEAPON_GUN] = new CWeaponGun(pGameServer);
    m_pWeapons[WEAPON_SHOTGUN] = 0;
    m_pWeapons[WEAPON_GRENADE] = 0;
    m_pWeapons[WEAPON_RIFLE] = new CWeaponSniperRifle(pGameServer);
    m_pWeapons[WEAPON_NINJA] = 0;

    // 0.6
    m_Skin.m_UseCustomColor = 0;
    str_copy(m_Skin.m_aSkinName, "warpaint");
    m_Skin.m_ColorBody = 0;
    m_Skin.m_ColorFeet = 0;

    // 0.7
    str_copy(m_Skin.m_apSkinPartNames[0], "standard");
    str_copy(m_Skin.m_apSkinPartNames[1], "warpaint");
    str_copy(m_Skin.m_apSkinPartNames[2], "");
    str_copy(m_Skin.m_apSkinPartNames[3], "standard");
    str_copy(m_Skin.m_apSkinPartNames[4], "standard");
    str_copy(m_Skin.m_apSkinPartNames[5], "standard");

    m_Skin.m_aUseCustomColors[0] = true;
    m_Skin.m_aUseCustomColors[1] = false;
    m_Skin.m_aUseCustomColors[2] = false;
    m_Skin.m_aUseCustomColors[3] = true;
    m_Skin.m_aUseCustomColors[4] = true;
    m_Skin.m_aUseCustomColors[5] = false;

    m_Skin.m_aSkinPartColors[0] = 1944919;
    m_Skin.m_aSkinPartColors[1] = 0;
    m_Skin.m_aSkinPartColors[2] = 0;
    m_Skin.m_aSkinPartColors[3] = 750337;
    m_Skin.m_aSkinPartColors[4] = 1944919;
    m_Skin.m_aSkinPartColors[5] = 0;
}

int CClassSniper::OnPlayerDeath(int KillerID, vec2 Pos)
{
    return 1;
}

void CClassSniper::OnPlayerKill(int VictimID, int Weapon)
{
    CPlayer *pKiller = GameServer()->m_apPlayers[VictimID];
    
    if(Character() && pKiller && pKiller->IsInfect() && Weapon == WEAPON_RIFLE)
        Character()->AddWeaponAmmo(WEAPON_RIFLE, 1); // give 1 ammo
}

CClass *CClassSniper::CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) 
{ 
    return new CClassSniper(pGameServer, pOwner);
}