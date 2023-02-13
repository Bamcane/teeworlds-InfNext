#include "gamecontext.h"
#include "weapons/hammer.h"
#include "class.h"

CClass::~CClass()
{
    for(int i = 0;i < NUM_WEAPONS; i ++)
        if(m_pWeapons[i])
            delete m_pWeapons[i];
}