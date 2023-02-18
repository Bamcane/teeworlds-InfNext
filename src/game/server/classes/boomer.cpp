#include <game/server/gamecontext.h>
#include <game/server/weapons/boomer-hammer.h>
#include "boomer.h"

CClassBoomer::CClassBoomer(CGameContext *pGameServer) : CClass(pGameServer)
{
    str_copy(m_ClassName, _("Boomer"));
    m_MaxJumpNum = 2;
    m_Infect = true;
    m_pWeapons[WEAPON_HAMMER] = new CWeaponBoomerHammer(pGameServer);
    m_pWeapons[WEAPON_GUN] = 0;
    m_pWeapons[WEAPON_SHOTGUN] = 0;
    m_pWeapons[WEAPON_GRENADE] = 0;
    m_pWeapons[WEAPON_RIFLE] = 0;
    m_pWeapons[WEAPON_NINJA] = 0;

    // 0.6
    m_Skin.m_UseCustomColor = 1;
    str_copy(m_Skin.m_aSkinName, "saddo");
    m_Skin.m_ColorBody = 3866368;
    m_Skin.m_ColorFeet = 65414;

    // 0.7
    str_copy(m_Skin.m_apSkinPartNames[0], "standard");
    str_copy(m_Skin.m_apSkinPartNames[1], "saddo");
    str_copy(m_Skin.m_apSkinPartNames[2], "");
    str_copy(m_Skin.m_apSkinPartNames[3], "standard");
    str_copy(m_Skin.m_apSkinPartNames[4], "standard");
    str_copy(m_Skin.m_apSkinPartNames[5], "standard");

    m_Skin.m_aUseCustomColors[0] = true;
    m_Skin.m_aUseCustomColors[1] = true;
    m_Skin.m_aUseCustomColors[2] = false;
    m_Skin.m_aUseCustomColors[3] = true;
    m_Skin.m_aUseCustomColors[4] = true;
    m_Skin.m_aUseCustomColors[5] = false;

    m_Skin.m_aSkinPartColors[0] = HSLtoint(58, 255, 40);
    m_Skin.m_aSkinPartColors[1] = HSLtoint(58, 255, 10);
    m_Skin.m_aSkinPartColors[2] = -8229413;
    m_Skin.m_aSkinPartColors[3] = HSLtoint(58, 255, 40);
    m_Skin.m_aSkinPartColors[4] = HSLtoint(0, 100, 0);
    m_Skin.m_aSkinPartColors[5] = -8229413;
}

void CClassBoomer::OnPlayerDeath(int ClientID, int KillerID, vec2 Pos)
{
    GameServer()->CreateSound(Pos, SOUND_GRENADE_EXPLODE);
	GameServer()->CreateExplosion(Pos, ClientID, WEAPON_HAMMER, false, -1L, DAMAGEMODE_INFECTION);
}