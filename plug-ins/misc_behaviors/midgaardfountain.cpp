/* $Id: midgaardfountain.cpp,v 1.1.2.5.22.2 2009/01/17 23:36:51 rufina Exp $
 *
 * ruffina, 2004
 */

#include "midgaardfountain.h"

#include "class.h"

#include "room.h"
#include "object.h"
#include "liquid.h"

#include "merc.h"
#include "act.h"
#include "vnum.h"
#include "def.h"

LIQ(blood);

bool MidgaardFountain::area( ) { 
    Character *wch;
    Object *o;
    int count = 0;
    
    for (o = object_list; o; o = o->next)
        if (o->pIndexData->vnum == OBJ_VNUM_MONUMENT 
            && o->in_room
            && o->in_room->areaInstance == obj->in_room->areaInstance)
        {
            count++;
        }

    wch = obj->in_room->people; 

    if (count < 3) {
        if (obj->value[2] != obj->pIndexData->value[2] && obj->value[2] == liq_blood) {
            obj->value[2] = obj->pIndexData->value[2];

            if (wch)
                act_p("Кровь в $o6 снова превращается в воду..", wch, obj, 0, TO_ALL, POS_RESTING);
        }   
        return false;
    }

    if (obj->value[2] == obj->pIndexData->value[2]) {
        obj->value[2] = liq_blood;
        
        if (wch) 
            act_p("Вода в $o6 медленно окрашивается {rкрасным{x.", wch, obj, 0, TO_ALL, POS_RESTING);
    }

    return false;
}

