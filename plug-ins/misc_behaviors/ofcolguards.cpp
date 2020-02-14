/* $Id$
 *
 * ruffina, 2004
 */
#include "ofcolguards.h"

#include "npcharacter.h"
#include "room.h"
#include "skillreference.h"

#include "act.h"
#include "interp.h"
#include "fight.h"
#include "merc.h"
#include "mercdb.h"
#include "handler.h"
#include "def.h"

GSN(garble);

void OfcolMarshal::fight( Character *victim )
{
    Character *ach, *ach_next;
    
    BasicMobileDestiny::fight( victim );

    if (ch->in_room->areaInstance != ch->zone) 
        return;

    if (number_percent() < 25) 
        return;
   
    do_yell( ch, "Охрана! На помощь!" );

    if (ch->isAffected( gsn_garble ))
        return;

    for( ach = char_list; ach != 0; ach = ach_next )
    {
        NPCharacter *mob;
        OfcolGuard::Pointer guard;

        ach_next = ach->next;

        if (!ach->in_room
            || ach == ch
            || ach->in_room->areaInstance != ch->in_room->areaInstance
            || !ach->is_npc( ))
            continue;
        
        mob = ach->getNPC( );
        
        if (mob->fighting || mob->last_fought)
            continue;

        if (!mob->behavior)
            continue;

        guard = mob->behavior.getDynamicPointer<OfcolGuard>( );

        if (!guard)
            continue;

        if (mob->in_room == ch->in_room)
        {
            int i;

            act_p("$c1 призывает Богов на помощь.", ch,0,0,TO_ROOM,POS_SLEEPING);
            act_p("Боги призывают $c4 на помощь Диане.", mob,0,0,TO_ROOM,POS_SLEEPING);

            mob->max_hit = 6000;
            mob->hit = 6000;
            mob->setLevel( 60 );
            mob->damage[DICE_NUMBER] = number_range(3,5);
            mob->damage[DICE_TYPE] = number_range(12,22);
            mob->damage[DICE_BONUS] = number_range(6, 8);

            for(i=0;i<stat_table.size;i++)
                mob->perm_stat[i] = 23;

            do_say(mob, "Диана, я иду на помощь...");
            multi_hit( mob, victim );
        }
        else {
            guard->pathToTarget( mob->in_room, ch->in_room, 2000 );

            if (!guard->path.empty( )) {
                if (number_percent() < 25)
                    do_yell(mob, "Держись Диана! Я иду на помощь!");
                else
                    do_say(mob, "Диане необходима моя помощь.");

                guard->makeOneStep( );
            }
        }
    }
}

void OfcolGuard::fight( Character *victim )
{
   Character *ach, *ach_next;

    BasicMobileDestiny::fight( victim );

    if (ch->in_room->areaInstance != ch->zone) 
        return;

    if (number_percent( ) < 25) 
        return;

    interpret_raw( ch, "yell", "Стража на помощь! %s убивает меня.", 
                  victim->getNameP( '1' ).c_str( ) );
   
    for (ach = char_list; ach != 0; ach = ach_next)
    {
        NPCharacter *mob;
        OfcolGuard::Pointer guard;

        ach_next = ach->next;

        if (!ach->in_room
            || ach == ch
            || ach->in_room->areaInstance != ch->in_room->areaInstance
            || !ach->is_npc( ))
            continue;
        
        mob = ach->getNPC( );
        
        if (mob->fighting || mob->last_fought || mob->isDead( ))
            continue;

        if (!mob->behavior)
            continue;

        guard = mob->behavior.getDynamicPointer<OfcolGuard>( );

        if (!guard)
            continue;

        if (ch->in_room == mob->in_room) {
            interpret_raw( mob, "say", "Теперь, %s, ты поплатишься за нападение на стражника.",
                           victim->getNameP( '1' ).c_str( ) );
            multi_hit( mob, victim );
        }
        else {
            guard->pathToTarget( mob->in_room, ch->in_room, 2000 );

            if (!guard->path.empty( )) {
                if (number_percent() < 25)
                    do_yell(mob, "Держись стражник! Я иду на помощь.");
                else
                    do_say(mob, "Страже необходима моя помощь.");

                guard->makeOneStep( );
            }
        }
    }
}


