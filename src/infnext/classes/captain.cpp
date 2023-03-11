#include <game/server/gamecontext.h>
#include <infnext/weapons/hammer.h>
#include <infnext/weapons/captain-rifle.h>
#include "captain.h"

CClassCaptain::CClassCaptain(CGameContext *pGameServer) : CClass(pGameServer)
{
    str_copy(m_ClassName, _("Captain"));
    m_MaxJumpNum = 2;
    m_Infect = false;
    m_pWeapons[WEAPON_HAMMER] = new CWeaponHammer(pGameServer);
    m_pWeapons[WEAPON_GUN] = 0;
    m_pWeapons[WEAPON_SHOTGUN] = 0;
    m_pWeapons[WEAPON_GRENADE] = 0;
    m_pWeapons[WEAPON_RIFLE] = new CWeaponCaptainRifle(pGameServer);
    m_pWeapons[WEAPON_NINJA] = 0;

    // 0.6
    m_Skin.m_UseCustomColor = 0;
    str_copy(m_Skin.m_aSkinName, "limekitty");
    m_Skin.m_ColorBody = 0;
    m_Skin.m_ColorFeet = 0;

    // 0.7
    str_copy(m_Skin.m_apSkinPartNames[0], "kitty");
    str_copy(m_Skin.m_apSkinPartNames[1], "whisker");
    str_copy(m_Skin.m_apSkinPartNames[2], "");
    str_copy(m_Skin.m_apSkinPartNames[3], "standard");
    str_copy(m_Skin.m_apSkinPartNames[4], "standard");
    str_copy(m_Skin.m_apSkinPartNames[5], "negative");

    m_Skin.m_aUseCustomColors[0] = true;
    m_Skin.m_aUseCustomColors[1] = true;
    m_Skin.m_aUseCustomColors[2] = false;
    m_Skin.m_aUseCustomColors[3] = true;
    m_Skin.m_aUseCustomColors[4] = true;
    m_Skin.m_aUseCustomColors[5] = false;

    m_Skin.m_aSkinPartColors[0] = HSLtoint(132, 118, 184);
    m_Skin.m_aSkinPartColors[1] = HSLtoint(130, 109, 219);
    m_Skin.m_aSkinPartColors[2] = -8229413;
    m_Skin.m_aSkinPartColors[3] = HSLtoint(120, 82, 235);
    m_Skin.m_aSkinPartColors[4] = HSLtoint(135, 82, 233);
    m_Skin.m_aSkinPartColors[5] = HSLtoint(137, 255, 0);
}

void CClassCaptain::OnCharacterSnap(CNetObj_Character *pCharacter)
{
    if(pCharacter->m_Weapon == WEAPON_RIFLE)
        pCharacter->m_AmmoCount *= 2;
}

void CClassCaptain::OnCharacterSnap(protocol7::CNetObj_Character *pCharacter)
{
    if(pCharacter->m_Weapon == WEAPON_RIFLE)
        pCharacter->m_AmmoCount *= 2;
}