#include <game/server/gamecontext.h>
#include <infnext/weapons/medic-hammer.h>
#include <infnext/weapons/medic-shotgun.h>
#include <infnext/weapons/gun.h>
#include "medic.h"

CClassMedic::CClassMedic(CGameContext *pGameServer, CPlayer *pOwner) : CClass(pGameServer, pOwner)
{
    str_copy(m_ClassName, _("Medic"));
    m_MaxJumpNum = 2;
    m_Infect = false;
    m_pWeapons[WEAPON_HAMMER] = new CWeaponMedicHammer(pGameServer);
    m_pWeapons[WEAPON_GUN] = new CWeaponGun(pGameServer);
    m_pWeapons[WEAPON_SHOTGUN] = new CWeaponMedicShotgun(pGameServer);
    m_pWeapons[WEAPON_GRENADE] = 0;
    m_pWeapons[WEAPON_RIFLE] = 0;
    m_pWeapons[WEAPON_NINJA] = 0;

    // 0.6
    m_Skin.m_UseCustomColor = 0;
    str_copy(m_Skin.m_aSkinName, "twinbop");
    m_Skin.m_ColorBody = 0;
    m_Skin.m_ColorFeet = 0;

    // 0.7
    str_copy(m_Skin.m_apSkinPartNames[0], "standard");
    str_copy(m_Skin.m_apSkinPartNames[1], "duodonny");
    str_copy(m_Skin.m_apSkinPartNames[2], "twinbopp");
    str_copy(m_Skin.m_apSkinPartNames[3], "standard");
    str_copy(m_Skin.m_apSkinPartNames[4], "standard");
    str_copy(m_Skin.m_apSkinPartNames[5], "standard");

    m_Skin.m_aUseCustomColors[0] = true;
    m_Skin.m_aUseCustomColors[1] = true;
    m_Skin.m_aUseCustomColors[2] = true;
    m_Skin.m_aUseCustomColors[3] = true;
    m_Skin.m_aUseCustomColors[4] = true;
    m_Skin.m_aUseCustomColors[5] = false;

    m_Skin.m_aSkinPartColors[0] = 15310519;
    m_Skin.m_aSkinPartColors[1] = -1600806;
    m_Skin.m_aSkinPartColors[2] = 15310519;
    m_Skin.m_aSkinPartColors[3] = 15310519;
    m_Skin.m_aSkinPartColors[4] = 37600;
    m_Skin.m_aSkinPartColors[5] = 0;
}

CClass *CClassMedic::CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) 
{ 
    return new CClassMedic(pGameServer, pOwner);
}