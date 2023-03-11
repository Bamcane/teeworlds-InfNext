#ifndef GAME_SERVER_INFDEFINE_H
#define GAME_SERVER_INFDEFINE_H

enum DamageMode
{
    DAMAGEMODE_DMG=0,
    DAMAGEMODE_DMGSELF,
    DAMAGEMODE_DMGALL,
    DAMAGEMODE_INFECTION,
};

enum BroadcastType
{
    BROADCAST_SYSTEM=0,
    BROADCAST_ADMIN,
    BROADCAST_FREEZE,
    BROADCAST_DEHYDRATED,
    BROADCAST_CLASS,
    BROADCAST_INFO,
};
#endif