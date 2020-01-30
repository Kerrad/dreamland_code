/* $Id$
 *
 * ruffina, 2004
 */
#include "loadsave.h"
#include "save.h"
#include "logstream.h"

#include "fenia/register-impl.h"
#include "feniamanager.h"
#include "wrapperbase.h"

#include "skillreference.h"
#include "mobilebehaviormanager.h"
#include "objectbehaviormanager.h"
#include "roombehaviormanager.h"

#include "objectmanager.h"
#include "affect.h"
#include "pcharacter.h"
#include "npcharacter.h"
#include "npcharactermanager.h"
#include "core/object.h"
#include "race.h"
#include "room.h"

#include "merc.h"
#include "mercdb.h"
#include "vnum.h"
#include "def.h"

GSN(sanctuary);
GSN(haste);
GSN(protection_evil);
GSN(protection_good);
GSN(stardust);
WEARLOC(none);
WEARLOC(stuck_in);
RELIG(chronos);
PROF(mobile);


/*
 * Create an instance of a mobile.
 */
NPCharacter *create_mobile_org( MOB_INDEX_DATA *pMobIndex, bool count );
NPCharacter *create_mobile( MOB_INDEX_DATA *pMobIndex )
{
    return create_mobile_org( pMobIndex, true );
}
NPCharacter *create_mobile_nocount( MOB_INDEX_DATA *pMobIndex )
{
    return create_mobile_org( pMobIndex, false );
}
NPCharacter *create_mobile_org( MOB_INDEX_DATA *pMobIndex, bool count )
{
        NPCharacter *mob;
        int i;
        Affect af;

        if ( pMobIndex == 0 )
        {
                bug( "Create_mobile: 0 pMobIndex.", 0 );
                exit( 1 );
        }

        mob = NPCharacterManager::getNPCharacter( );

        mob->pIndexData        = pMobIndex;
        
        DLString name( pMobIndex->player_name );
        mob->setName( name );

        mob->spec_fun        = pMobIndex->spec_fun;

        if ( pMobIndex->wealth == 0 )
        {
                mob->silver = 0;
                mob->gold   = 0;
        }
        else
        {
                long wealth;

                wealth = number_range(pMobIndex->wealth/2, 3 * pMobIndex->wealth/2) / 8;
                mob->gold = number_range(wealth/2000,wealth/1000);
                mob->silver = wealth/10 - (mob->gold * 100);
        }

        if (pMobIndex->new_format)    /* load in new style */
        {
                /* read from prototype */
                mob->group                = pMobIndex->group;
                mob->act                 = pMobIndex->act | ACT_IS_NPC;
                mob->affected_by        = pMobIndex->affected_by;
                mob->detection                = pMobIndex->detection;
                mob->alignment                = pMobIndex->alignment;
                mob->setLevel( pMobIndex->level );
//                mob->hitroll                = (mob->getRealLevel( ) / 2) + pMobIndex->hitroll;
                mob->hitroll                = mob->getRealLevel( ) + pMobIndex->hitroll;
                mob->damroll                = pMobIndex->damage[DICE_BONUS];
                mob->max_hit                = dice(pMobIndex->hit[DICE_NUMBER], pMobIndex->hit[DICE_TYPE]) + pMobIndex->hit[DICE_BONUS];
                mob->hit                = mob->max_hit;
                mob->max_mana                = dice(pMobIndex->mana[DICE_NUMBER], pMobIndex->mana[DICE_TYPE]) + pMobIndex->mana[DICE_BONUS];
                mob->mana                = mob->max_mana;
                mob->max_move           = 200 + 16 * mob->getRealLevel( ); // 2 times more than best-trained pc
                mob->move               = mob->max_move;
                mob->damage[DICE_NUMBER]= pMobIndex->damage[DICE_NUMBER];
                mob->damage[DICE_TYPE]        = pMobIndex->damage[DICE_TYPE];
                mob->dam_type                = pMobIndex->dam_type;
        
                if (mob->dam_type == 0)
                        switch(number_range(1,3))
                        {
                        case (1): mob->dam_type = 3;        break;  /* slash */
                        case (2): mob->dam_type = 7;        break;  /* pound */
                        case (3): mob->dam_type = 11;       break;  /* pierce */
                        }
                for (i = 0; i < 4; i++)
                        mob->armor[i]        = pMobIndex->ac[i];
                mob->off_flags                = pMobIndex->off_flags;
                mob->imm_flags                = pMobIndex->imm_flags;
                mob->res_flags                = pMobIndex->res_flags;
                mob->vuln_flags                = pMobIndex->vuln_flags;
                mob->start_pos                = pMobIndex->start_pos;
                mob->default_pos        = pMobIndex->default_pos;
                mob->setSex( pMobIndex->sex );
                if (mob->getSex( ) == SEX_EITHER) /* random sex */
                        mob->setSex( number_range(1,2) );
                DLString raceName = pMobIndex->race;
                raceName = raceName.toLower();
                mob->setRace( raceName );
                mob->form                = pMobIndex->form;
                mob->parts                = pMobIndex->parts;
                mob->size                = pMobIndex->size;
                mob->material                = str_dup(pMobIndex->material);
                mob->extracted                = false;

                /* computed on the spot */

                for (i = 0; i < stat_table.size; i ++)
                        mob->perm_stat[i] = min(25,11 + mob->getRealLevel( )/4);

                if (IS_SET(mob->off_flags,OFF_FAST))
                        mob->perm_stat[STAT_DEX] += 2;

                mob->perm_stat[STAT_STR] += mob->size - SIZE_MEDIUM;
                mob->perm_stat[STAT_CON] += (mob->size - SIZE_MEDIUM) / 2;

                /* let's get some spell action */
                if (IS_AFFECTED(mob,AFF_SANCTUARY))
                {
                    af.type      = gsn_sanctuary; 
                    af.bitvector = AFF_SANCTUARY;
                    af.where     = TO_AFFECTS;
                    af.level     = mob->getRealLevel( );
                    af.duration  = -1;
                    affect_to_char( mob, &af );
                }

                if (IS_AFFECTED(mob,AFF_HASTE))
                {
                        af.where         = TO_AFFECTS;
                        af.type      = gsn_haste; 
                        af.level     = mob->getRealLevel( );
                        af.duration  = -1;
                        af.location  = APPLY_DEX;
                        af.modifier  = 1 + (mob->getRealLevel( ) >= 18) + (mob->getRealLevel( ) >= 25) +
                                (mob->getRealLevel( ) >= 32);
                        af.bitvector = AFF_HASTE;
                        affect_to_char( mob, &af );
                }

                if (IS_AFFECTED(mob,AFF_PROTECT_EVIL))
                {
                        af.where         = TO_AFFECTS;
                        af.type         = gsn_protection_evil;
                        af.level         = mob->getRealLevel( );
                        af.duration         = -1;
                        af.location         = APPLY_SAVES;
                        af.modifier         = -1;
                        af.bitvector = AFF_PROTECT_EVIL;
                        affect_to_char(mob,&af);
                }

                if (IS_AFFECTED(mob,AFF_PROTECT_GOOD))
                {
                        af.where         = TO_AFFECTS;
                        af.type      = gsn_protection_good; 
                        af.level     = mob->getRealLevel( );
                        af.duration  = -1;
                        af.location  = APPLY_SAVES;
                        af.modifier  = -1;
                        af.bitvector = AFF_PROTECT_GOOD;
                        affect_to_char(mob,&af);
                }
        }
        else /* read in old format and convert */
        {
                mob->act                = pMobIndex->act;
                mob->affected_by        = pMobIndex->affected_by;
                mob->detection                = pMobIndex->detection;
                mob->alignment                = pMobIndex->alignment;
                mob->setLevel( pMobIndex->level );
                mob->hitroll                = max(pMobIndex->hitroll,pMobIndex->level/4);
                mob->damroll                = pMobIndex->level /2 ;
                if (mob->getRealLevel( ) < 30)
                        mob->max_hit                = mob->getRealLevel( ) * 20 + number_range(
                                mob->getRealLevel( ),mob->getRealLevel( ) * 5);
                else if (mob->getRealLevel( ) < 60)
                        mob->max_hit                = mob->getRealLevel( ) * 50 + number_range(
                                mob->getRealLevel( ) * 10,mob->getRealLevel( ) * 50);
                else
                        mob->max_hit                = mob->getRealLevel( ) * 100 + number_range(
                                mob->getRealLevel( ) * 20,mob->getRealLevel( ) * 100);
                if (IS_SET(mob->act,ACT_MAGE | ACT_CLERIC))
                        mob->max_hit = ( int )( mob->max_hit * 0.9 );
                mob->hit                = mob->max_hit;
                mob->max_mana                = 100 + dice(mob->getRealLevel( ),10);
                mob->mana                = mob->max_mana;
                switch(number_range(1,3))
                {
                case (1): mob->dam_type = 3;         break;  /* slash */
                case (2): mob->dam_type = 7;        break;  /* pound */
                case (3): mob->dam_type = 11;        break;  /* pierce */
                }
                for (i = 0; i < 3; i++)
                        mob->armor[i]        = interpolate(mob->getRealLevel( ),100,-100);
                mob->armor[3]                = interpolate(mob->getRealLevel( ),100,0);
                mob->setRace( pMobIndex->race );
                mob->off_flags                = pMobIndex->off_flags;
                mob->imm_flags                = pMobIndex->imm_flags;
                mob->res_flags                = pMobIndex->res_flags;
                mob->vuln_flags                = pMobIndex->vuln_flags;
                mob->start_pos                = pMobIndex->start_pos;
                mob->default_pos        = pMobIndex->default_pos;
                mob->setSex( pMobIndex->sex );
                mob->form                = pMobIndex->form;
                mob->parts                = pMobIndex->parts;
                mob->size                = SIZE_MEDIUM;
                mob->material                = str_empty;
                mob->extracted                = false;

                for (i = 0; i < stat_table.size; i ++)
                        mob->perm_stat[i] = min(25,11 + mob->getRealLevel( )/4);

                if (IS_SET(mob->off_flags,OFF_FAST))
                        mob->perm_stat[STAT_DEX] += 2;
        }

        mob->position = mob->start_pos;

        mob->setReligion( god_chronos );
        mob->setProfession( prof_mobile );
        
        /* link the mob to the world list */
        char_to_list( mob, &char_list );

        if (count)
            pMobIndex->count++;
        
        /* assign behavior */        
        if (pMobIndex->behavior) 
            MobileBehaviorManager::assign( mob );
        else
            MobileBehaviorManager::assignBasic( mob );
        
        /* fenia mobprog initialization */
        if (count) {
            WrapperBase *w = get_wrapper(pMobIndex->wrapper);
            if (w) {
                static Scripting::IdRef initId( "init" );
                try {
                    w->call(initId, "C", mob);
                } catch (Exception e) {
                    LogStream::sendError( ) 
                            << "create_mobile #" << pMobIndex->vnum 
                            << ": " <<  e.what( ) << endl;
                }
            }
        }
        
        return mob;
}

/* duplicate a mobile exactly -- except inventory */
void clone_mobile(NPCharacter *parent, NPCharacter *clone)
{
    int i;
    Affect *paf;

    if (parent == 0 || clone == 0)
        return;

    /* start fixing values */
    DLString name( parent->getNameP( ) );
    clone->setName( name );

    if (parent->getRealShortDescr( ))
        clone->setShortDescr( parent->getShortDescr( ) );
    if (parent->getRealLongDescr( ))
        clone->setLongDescr( parent->getLongDescr( ) );
    if (parent->getRealDescription( ))
        clone->setDescription( parent->getDescription( ) );

    clone->group        = parent->group;
    clone->setSex( parent->getSex( ) );
    clone->setRace( parent->getRace( )->getName( ) );
    clone->setLevel( parent->getRealLevel( ) );
    clone->timer        = parent->timer;
    clone->wait                = parent->wait;
    clone->hit                = parent->hit;
    clone->max_hit        = parent->max_hit;
    clone->mana                = parent->mana;
    clone->max_mana        = parent->max_mana;
    clone->move                = parent->move;
    clone->max_move        = parent->max_move;
    clone->gold                = parent->gold;
    clone->silver        = parent->silver;
    clone->exp                = parent->exp;
    clone->act                = parent->act;
    clone->comm                = parent->comm;
    clone->imm_flags        = parent->imm_flags;
    clone->res_flags        = parent->res_flags;
    clone->vuln_flags        = parent->vuln_flags;
    clone->invis_level        = parent->invis_level;
    clone->affected_by        = parent->affected_by;
    clone->detection        = parent->detection;
    clone->position        = parent->position;
    clone->saving_throw        = parent->saving_throw;
    clone->alignment        = parent->alignment;
    clone->hitroll        = parent->hitroll;
    clone->damroll        = parent->damroll;
    clone->wimpy        = parent->wimpy;
    clone->form                = parent->form;
    clone->parts        = parent->parts;
    clone->size                = parent->size;
    clone->material        = str_dup(parent->material);
    clone->extracted        = parent->extracted;
    clone->off_flags        = parent->off_flags;
    clone->dam_type        = parent->dam_type;
    clone->start_pos        = parent->start_pos;
    clone->default_pos        = parent->default_pos;
    clone->spec_fun        = parent->spec_fun;

    for (i = 0; i < 4; i++)
            clone->armor[i]        = parent->armor[i];

    for (i = 0; i < stat_table.size; i++)
    {
        clone->perm_stat[i]        = parent->perm_stat[i];
        clone->mod_stat[i]        = parent->mod_stat[i];
    }

    for (i = 0; i < 3; i++)
        clone->damage[i]        = parent->damage[i];

    /* now add the affects */
    for (paf = parent->affected; paf != 0; paf = paf->next)
        affect_to_char(clone,paf);

}

/*
 * Create an object with modifying the count
 */
Object *create_object( OBJ_INDEX_DATA *pObjIndex, short level )
{
  return create_object_org(pObjIndex,level,true);
}

/*
 * for player load/quit
 * Create an object and do not modify the count
 */
Object *create_object_nocount(OBJ_INDEX_DATA *pObjIndex, short level )
{
  return create_object_org(pObjIndex,level,false);
}

/*
 * Create an instance of an object.
 */
Object *create_object_org( OBJ_INDEX_DATA *pObjIndex, short level, bool Count )
{
        Object *obj;

        if ( pObjIndex == 0 )
        {
                bug( "Create_object: 0 pObjIndex.", 0 );
                exit( 1 );
        }

        obj = ObjectManager::getObject( );

        obj->pIndexData        = pObjIndex;
        obj->in_room        = 0;
        obj->enchanted        = false;
        obj->updateCachedNoun( );

        if ( ( obj->pIndexData->limit != -1 )
                && ( obj->pIndexData->count >  obj->pIndexData->limit ) )
                if ( pObjIndex->new_format == 1 )
                    LogStream::sendWarning( ) << "Obj limit exceeded for vnum " << pObjIndex->vnum << endl;

        /*    if ( pObjIndex->new_format == 1 ) */
        if (pObjIndex->new_format)
                obj->level = pObjIndex->level;
        else
                obj->level = max(static_cast<short>( 0 ),level);

        obj->item_type        = pObjIndex->item_type;
        obj->extra_flags= pObjIndex->extra_flags;
        obj->wear_flags        = pObjIndex->wear_flags;
        obj->value[0]        = pObjIndex->value[0];
        obj->value[1]        = pObjIndex->value[1];
        obj->value[2]        = pObjIndex->value[2];
        obj->value[3]        = pObjIndex->value[3];
        obj->value[4]        = pObjIndex->value[4];
        obj->weight        = pObjIndex->weight;
        obj->extracted        = false;
        obj->from       = str_dup(""); /* used with body parts */
        obj->condition        = pObjIndex->condition;

        if (level == 0 || pObjIndex->new_format)
                if (obj->cost > 1000)
                    obj->cost        = pObjIndex->cost / 10;
                else
                    obj->cost        = pObjIndex->cost;
                
        else
                obj->cost        = number_fuzzy( level );

        /*
         * Mess with object properties.
         */
        switch ( obj->item_type )
        {
        default:
                bug( "Read_object: vnum %d bad type.", pObjIndex->vnum );
                break;

        case ITEM_LIGHT:
                if ( obj->value[2] == 999 )
                        obj->value[2] = -1;
                break;

        case ITEM_CORPSE_PC:
                obj->value[3] = ROOM_VNUM_ALTAR;

        case ITEM_CORPSE_NPC:
        case ITEM_KEY:
        case ITEM_FURNITURE:
        case ITEM_TRASH:
        case ITEM_CONTAINER:
        case ITEM_DRINK_CON:
        case ITEM_FOOD:
        case ITEM_BOAT:
        case ITEM_FOUNTAIN:
        case ITEM_MAP:
        case ITEM_CLOTHING:
        case ITEM_PORTAL:
                if (!pObjIndex->new_format)
                        obj->cost /= 5;
                break;

        case ITEM_TREASURE:
        case ITEM_KEYRING:
        case ITEM_LOCKPICK:
        case ITEM_WARP_STONE:
        case ITEM_GEM:
        case ITEM_JEWELRY:
        case ITEM_TATTOO:
        case ITEM_SPELLBOOK:
        case ITEM_RECIPE:
        case ITEM_TEXTBOOK:
        case ITEM_PARCHMENT:
                break;

        case ITEM_SCROLL:
                if (level != -1 && !pObjIndex->new_format)
                        obj->value[0]        = number_fuzzy( obj->value[0] );
                break;

        case ITEM_WAND:
        case ITEM_STAFF:
                if (level != -1 && !pObjIndex->new_format)
                {
                        obj->value[0]        = number_fuzzy( obj->value[0] );
                        obj->value[1]        = number_fuzzy( obj->value[1] );
                        obj->value[2]        = obj->value[1];
                }
                if (!pObjIndex->new_format)
                        obj->cost *= 2;
                break;

        case ITEM_WEAPON:
                if (level != -1 && !pObjIndex->new_format)
                {
                        obj->value[1] = number_fuzzy( number_fuzzy( 1 * level / 4 + 2 ) );
                        obj->value[2] = number_fuzzy( number_fuzzy( 3 * level / 4 + 6 ) );
                }
                if ( pObjIndex->new_format )
                {
                //Randomizing value2
                    if ( obj->value[2] < 6)
                            obj->value[2] = number_disperse( obj->value[2], 30 );
                    else
                    if ( obj->value[2] < 10)
                            obj->value[2] = number_disperse( obj->value[2], 20 );
                    else
                            obj->value[2] = number_disperse( obj->value[2], 15 );
                }
                break;

        case ITEM_ARMOR:
                if (level != -1 && !pObjIndex->new_format)
                {
                        obj->value[0]        = number_fuzzy( level / 5 + 3 );
                        obj->value[1]        = number_fuzzy( level / 5 + 3 );
                        obj->value[2]        = number_fuzzy( level / 5 + 3 );
                }
                if ( pObjIndex->new_format )
                {
                        //Randomizing 
                    obj->value[0] = number_disperse( obj->value[0], 35 );
                    obj->value[1] = number_disperse( obj->value[1], 35 );
                    obj->value[2] = number_disperse( obj->value[2], 35 );
                    obj->value[3] = number_disperse( obj->value[3], 15 );
                }
                                                                                                    
                break;

        case ITEM_POTION:
        case ITEM_PILL:
                if (level != -1 && !pObjIndex->new_format)
                        obj->value[0] = number_fuzzy( number_fuzzy( obj->value[0] ) );
                break;

        case ITEM_MONEY:
                if (!pObjIndex->new_format)
                        obj->value[0]        = obj->cost;
                break;
        }
        
        obj_to_list( obj );

        if ( Count )
                pObjIndex->count++;

        /* assign behavior */
        if (pObjIndex->behavior) 
            ObjectBehaviorManager::assign( obj );
        
        /* fenia objprog initialization */
        if (Count) {
            WrapperBase *w = get_wrapper(pObjIndex->wrapper);
            if (w) {
                static Scripting::IdRef initId( "init" );
                try {
                    w->call(initId, "O", obj);
                } catch (Exception e) {
                    LogStream::sendError( ) 
                            << "create_object #" << pObjIndex->vnum 
                            << ": " <<  e.what( ) << endl;
                }
            }
        }

        return obj;
}

/* duplicate an object exactly -- except contents */
void clone_object(Object *parent, Object *clone)
{
    int i;
    Affect *paf;
    EXTRA_DESCR_DATA *ed,*ed_new;

    if (parent == 0 || clone == 0)
        return;

    /* start fixing the object */
    if (parent->getRealName( ))
        clone->setName( parent->getName( ) );
    if (parent->getRealShortDescr( ))
        clone->setShortDescr( parent->getShortDescr( ) );
    if (parent->getRealDescription( ))
        clone->setDescription( parent->getDescription( ) );
    if (parent->getRealMaterial( ))
        clone->setMaterial( parent->getMaterial( ) );
    if (parent->getOwner( ))
        clone->setOwner( parent->getOwner( ) );

    clone->item_type        = parent->item_type;
    clone->extra_flags        = parent->extra_flags;
    clone->wear_flags        = parent->wear_flags;
    clone->weight        = parent->weight;
    clone->cost                = parent->cost;
    clone->level        = parent->level;
    clone->condition        = parent->condition;

    clone->pocket = parent->pocket;
    clone->timer        = parent->timer;
    clone->from         = str_dup(parent->from);
    clone->extracted    = parent->extracted;

    for (i = 0;  i < 5; i ++)
        clone->value[i]        = parent->value[i];

    /* affects */
    clone->enchanted        = parent->enchanted;

    for (paf = parent->affected; paf != 0; paf = paf->next)
        affect_to_obj( clone, paf);

    /* extended desc */
    for (ed = parent->extra_descr; ed != 0; ed = ed->next)
    {
        ed_new                  = new_extra_descr();
        ed_new->keyword            = str_dup( ed->keyword);
        ed_new->description     = str_dup( ed->description );
        ed_new->next                   = clone->extra_descr;
        clone->extra_descr          = ed_new;
    }

}



Room *create_room_instance(Room *proto, DLString key)
{
    Room *pRoom;

    pRoom = new Room;
    pRoom->instance = key;
    pRoom->area = proto->area;

    EXTRA_DESCR_DATA *ed, *ed_new;
    for (ed = proto->extra_descr; ed != 0; ed = ed->next) {
        ed_new = new_extra_descr();
        ed_new->keyword         = str_dup( ed->keyword);
        ed_new->description     = str_dup( ed->description );
        ed_new->next            = pRoom->extra_descr;
        pRoom->extra_descr      = ed_new;
    }

    EXTRA_EXIT_DATA *eexit;
    for (eexit = proto->extra_exit; eexit; eexit = eexit->next) {    
        EXTRA_EXIT_DATA *peexit = (EXTRA_EXIT_DATA*)alloc_perm(sizeof(EXTRA_EXIT_DATA));
        
        peexit->description = str_dup(eexit->description);
        peexit->exit_info_default = peexit->exit_info = eexit->exit_info_default;
        peexit->key = eexit->key;
        peexit->u1.vnum = eexit->u1.vnum;
        peexit->level = eexit->level;

        peexit->short_desc_from = str_dup(eexit->short_desc_from);
        peexit->short_desc_to = str_dup(eexit->short_desc_to);
        peexit->room_description = str_dup(eexit->room_description);
        peexit->max_size_pass = eexit->max_size_pass;

        peexit->moving_from = eexit->moving_from;
        peexit->moving_mode_from = eexit->moving_mode_from;
        peexit->moving_to = eexit->moving_to;
        peexit->moving_mode_to = eexit->moving_mode_to;

        peexit->keyword = str_dup(eexit->keyword);
        peexit->next = pRoom->extra_exit;
        pRoom->extra_exit = peexit;
    }

    for (int door = 0; door < DIR_SOMEWHERE; door++) {
        EXIT_DATA *exit = proto->exit[door];
        if (!exit)
            continue;

        EXIT_DATA *pexit = (EXIT_DATA*)alloc_perm(sizeof(EXIT_DATA));
    
        pexit->keyword = str_dup(exit->keyword);
        pexit->short_descr = str_dup(exit->short_descr);
        pexit->description = str_dup(exit->description);
        pexit->exit_info_default = pexit->exit_info = exit->exit_info_default;
        pexit->key = exit->key;
        pexit->u1.vnum = exit->u1.vnum;
        pexit->level = 0;
        pRoom->exit[door] = pexit;
        pRoom->old_exit[door] = pexit;
    }

    pRoom->name = str_dup(proto->name);
    pRoom->description = str_dup(proto->description);
    pRoom->room_flags = pRoom->room_flags_default = proto->room_flags;
    pRoom->sector_type = proto->sector_type;
    pRoom->owner = &str_empty[0];
    pRoom->heal_rate_default = pRoom->heal_rate = proto->heal_rate_default;
    pRoom->mana_rate_default = pRoom->mana_rate = proto->mana_rate_default;
    pRoom->clan = proto->clan;
    pRoom->guilds.set(proto->guilds);
    pRoom->liquid = proto->liquid;
    pRoom->properties = proto->properties;
    RoomBehaviorManager::copy(proto, pRoom);

    return pRoom;
}


bool create_area_instance(AREA_DATA *area, PCMemoryInterface *player)
{
    DLString key = player->getName();

    // Check no instance of given name already exists;
    if (area->instances.find(key) != area->instances.end())
        return false;

    // Init new area instance for this player.
    AreaInstance ai;
    ai.key = key;
    ai.area = area;
    area->instances[key] = ai;
    
    // Create duplicates of all rooms in this area.
    for (auto r: area->rooms) {
        int vnum = r.first;
        Room *proto = r.second;

        Room *newroom = create_room_instance(proto, key);        
        roomInstances.push_back(newroom);
        ai.rooms[vnum] = newroom;
    }

    // Resolve room exits from virtual to real, as during initial area load.
    for (auto r: ai.rooms) {
        Room *room = r.second;
        EXIT_DATA *pexit;

        for (int door = 0; door <= 5; door++ ) {
            if ((pexit = room->exit[door])) {
                if (pexit->u1.vnum > 0)
                    pexit->u1.to_room = get_room_instance(pexit->u1.vnum, key);
                else
                    pexit->u1.to_room = 0;
            }
        }

        for (EXTRA_EXIT_DATA *peexit = room->extra_exit; peexit; peexit = peexit->next) {
            if (peexit->u1.vnum > 0) 
                peexit->u1.to_room = get_room_instance(peexit->u1.vnum, key);
            else
                peexit->u1.to_room = 0;
        }
    }

    notice("room: created instance of area %s for player %s, %d rooms",
            area->area_file->file_name, key.c_str(), ai.rooms.size());
    return true;
}