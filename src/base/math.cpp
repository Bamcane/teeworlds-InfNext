#include "math.h"

int random_distribution(int probs[])
{
    if(!probs[0])
        return 0;

    int i = 0;
    do
    {
        if(!probs[i])
            i = 0;

        if(random_int(0, 100) <= probs[i])
            return i;
        
        i ++;
    }while(true);
}