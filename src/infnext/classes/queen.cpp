#include <game/server/gamecontext.h>
#include <infnext/weapons/infect-hammer.h>
#include "queen.h"

CClassQueen::CClassQueen(CGameContext *pGameServer, CPlayer *pOwner) : CClass(pGameServer, pOwner)
{
    str_copy(m_ClassName, _("Queen"));
    m_MaxJumpNum = 2;
    m_Infect = true;
    m_HookDamage = 2;

    m_pWeapons[WEAPON_HAMMER] = new CWeaponInfectHammer(pGameServer);
    m_pWeapons[WEAPON_GUN] = 0;
    m_pWeapons[WEAPON_SHOTGUN] = 0;
    m_pWeapons[WEAPON_GRENADE] = 0;
    m_pWeapons[WEAPON_RIFLE] = 0;
    m_pWeapons[WEAPON_NINJA] = 0;

    // 0.6
    m_Skin.m_UseCustomColor = 1;
    str_copy(m_Skin.m_aSkinName, "twinbop");
    m_Skin.m_ColorBody = HSLtoint(0, 255, 0);
    m_Skin.m_ColorFeet = HSLtoint(0, 100, 0);

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

    m_Skin.m_aSkinPartColors[0] = HSLtoint(0, 255, 40);
    m_Skin.m_aSkinPartColors[1] = HSLtoint(0, 255, 10);
    m_Skin.m_aSkinPartColors[2] = HSLtoint(0, 255, 10);
    m_Skin.m_aSkinPartColors[3] = HSLtoint(0, 255, 40);
    m_Skin.m_aSkinPartColors[4] = HSLtoint(0, 100, 0);
    m_Skin.m_aSkinPartColors[5] = -8229413;

    m_HookDmgTick = 0;
}

void CClassQueen::OnTick()
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

int CClassQueen::OnPlayerDeath(int KillerID, vec2 Pos)
{
    CPlayer *pKiller = GameServer()->m_apPlayers[KillerID];

    if(pKiller->IsHuman())
    {
        GameServer()->SendChatTarget_Localization(KillerID, _("You killed the queen, +5 score!"));
        GameServer()->SendBroadcast_Localization(-1, _("The queen '{str:Queen}' was killed by human '{str:Player}'!"),
             150, BROADCAST_QUEEN,
            "Queen", Server()->ClientName(Player()->GetCID()), 
            "Player", Server()->ClientName(KillerID),
            NULL);
    }else
    {
        GameServer()->SendChatTarget_Localization(KillerID, _("You killed the queen, -5 score!"));
        GameServer()->SendBroadcast_Localization(-1, _("The queen '{str:Queen}' was killed by infected '{str:Player}'!?"),
             150, BROADCAST_QUEEN,
            "Queen", Server()->ClientName(Player()->GetCID()), 
            "Player", Server()->ClientName(KillerID),
            NULL);
    }

    if(Player())
        Player()->ChangeClass();

    return 5;
}

bool CClassQueen::OnPlayerSpawn(bool IsInfected, vec2* Pos)
{
    if(!IsInfected)
        return false;

    if(!Character())
        return false;

    if(random_int(0, 100) < 20)
    {
        *Pos = Character()->GetPos();
        return true;
    }
    return false;
}

CClass *CClassQueen::CreateNewOne(CGameContext *pGameServer, CPlayer *pOwner) 
{ 
    return new CClassQueen(pGameServer, pOwner);
}