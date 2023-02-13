#include <game/server/gamecontext.h>
#include <game/server/weapon-entities/projectile.h>
#include "shotgun.h"

CWeaponShotgun::CWeaponShotgun(CGameContext *pGameServer)
    : CWeapon(pGameServer)
{
    m_MaxAmmo = g_pData->m_Weapons.m_Shotgun.m_pBase->m_Maxammo;
    m_InitAmmo = m_MaxAmmo;
    m_AmmoRegen = g_pData->m_Weapons.m_Shotgun.m_pBase->m_Ammoregentime;
    m_FireDelay = g_pData->m_Weapons.m_Shotgun.m_pBase->m_Firedelay;
    m_FullAuto = true;
}

void CWeaponShotgun::Fire(vec2 Pos, vec2 Dir, int Owner)
{
    int ShotSpread = 2;

    for(int i = -ShotSpread; i <= ShotSpread; ++i)
    {
        float Spreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
        float a = GetAngle(Dir);
        a += Spreading[i+2];
        float v = 1-(absolute(i)/(float)ShotSpread);
        float Speed = mix((float)GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
        CProjectile *pProj = new CProjectile(GameWorld(), WEAPON_SHOTGUN,
            Owner,
            Pos,
            vec2(cosf(a), sinf(a))*Speed,
            (int)(GameServer()->Server()->TickSpeed()*GameServer()->Tuning()->m_ShotgunLifetime),
            1, 0, 0, -1, WEAPON_SHOTGUN);
    }

    GameServer()->CreateSound(Pos, SOUND_SHOTGUN_FIRE);
    
    return;
}