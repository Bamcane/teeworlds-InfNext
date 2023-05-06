#include <game/server/gamecontext.h>
#include <infnext/weapons/infect-hammer.h>
#include "smoker.h"

CClassSmoker::CClassSmoker(CGameContext *pGameServer, CPlayer *pOwner) : CClass(pGameServer, pOwner)
{
    str_copy(m_ClassName, _("Smoker"));
    m_MaxJumpNum = 2;
    m_Infect = true;
    m_HookDamage = 3;
    m_pWeapons[WEAPON_HAMMER] = new CWeaponInfectHammer(pGameServer);
    m_pWeapons[WEAPON_GUN] = 0;
    m_pWeapons[WEAPON_SHOTGUN] = 0;
    m_pWeapons[WEAPON_GRENADE] = 0;
    m_pWeapons[WEAPON_RIFLE] = 0;
    m_pWeapons[WEAPON_NINJA] = 0;

    // 0.6
    m_Skin.m_UseCustomColor = 1;
    str_copy(m_Skin.m_aSkinName, "cammostripes");
    m_Skin.m_ColorBody = HSLtoint(58, 255, 0);
    m_Skin.m_ColorFeet = HSLtoint(0, 255, 134);

    // 0.7
    str_copy(m_Skin.m_apSkinPartNames[0], "standard");
    str_copy(m_Skin.m_apSkinPartNames[1], "cammostripes");
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

    m_HookDmgTick = 0;
}

void CClassSmoker::OnTick()
{
    if(!Character() || !Character()->IsAlive())
        return;

    CCharacterCore *pCore = Character()->GetCore();

    if(!pCore)
        return;

    if(pCore->m_HookedPlayer != -1)
    {
        CCharacter *pHooked = GameServer()->GetPlayerChar(pCore->m_HookedPlayer);
        if(pHooked && pHooked->GetPlayer()->IsHuman() && Server()->Tick() > m_HookDmgTick + Server()->TickSpeed())
        {
            pHooked->TakeDamage(vec2(0.f, 0.f), m_HookDamage, Character()->GetCID(), WEAPON_NINJA);

            if(Character()->IncreaseHealth(1))
                Character()->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
            else if(Character()->IncreaseArmor(1))
                Character()->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
            
            m_HookDmgTick = Server()->Tick();
        }
    }
}

CClass *CClassSmoker::CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) 
{ 
    return new CClassSmoker(pGameServer, pOwner);
}