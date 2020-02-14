/* $Id$
 *
 * ruffina, 2004
 */
#include "shoptrader.h"
#include "occupations.h"

#include "room.h"
#include "npcharacter.h"
#include "pcharacter.h"
#include "object.h"

#include "act.h"
#include "interp.h"
#include "handler.h"
#include "merc.h"
#include "mercdb.h"
#include "def.h"

void lore_fmt_item( Character *ch, Object *obj, ostringstream &buf, bool showName );
void deduct_cost(Character *ch, int cost);
int get_cost( NPCharacter *keeper, Object *obj, bool fBuy, ShopTrader::Pointer trader );
Object *get_obj_keeper( Character *ch, ShopTrader::Pointer trader, const DLString &constArguments );

int count_player_room( Room *room )
{
    int cnt = 0;

    for (Character *ch = room->people; ch; ch = ch->next_in_room)
        if (!ch->is_npc( ))
            cnt++;

    return cnt;
}

ShopTrader::ShopTrader( )
            : buys( 0, &item_table )
{
}

int ShopTrader::getOccupation( )
{
    return Repairman::getOccupation( ) | (1 << OCC_SHOPPER);
}

void ShopTrader::load( DLString str )
{
    if (str == "spec_repairman") {
        for (int i = 0; i < item_table.size; i++)        
            repairs.setBitNumber( i );
    }
    else {
        for (int i = 0; i < 5; i++) 
            buys.setBitNumber( str.getOneArgument( ).toInt( ) );
        
        profitBuy  = str.getOneArgument( ).toInt( );
        profitSell = str.getOneArgument( ).toInt( );
        openHour   = str.getOneArgument( ).toInt( );
        closeHour  = str.getOneArgument( ).toInt( );
    }
}


void ShopTrader::give( Character *from, Object *obj )
{
    tell_dim( from, ch, "Извини, но я не беру взяток!" );
    act("$c1 роняет $o4.", ch, obj, 0, TO_ROOM );
    obj_from_char( obj );
    obj_to_room( obj, ch->in_room );
}

void ShopTrader::speech( Character *victim, const char *speech )
{
    describeGoods( victim, speech, false );
}

void ShopTrader::tell( Character *victim, const char *speech )
{
    describeGoods( victim, speech, true );
}

void ShopTrader::describeGoods( Character *client, const DLString &args, bool verbose )
{
    ostringstream buf;
    Object *obj  = get_obj_keeper( client, this, args );

    if (!obj) {
        if (verbose)
            tell_dim( client, ch, "Я не продаю этого - используй '{lRсписок{lElist{x'." );
        return;
    }

    client->recho( ch, "%^C1 просит %C4 подробнее рассказать о %O6.", client, ch, obj );

    if (!IS_OBJ_STAT( obj, ITEM_INVENTORY )) {
        client->pecho( "%1$^C1 говорит тебе '{gЯ раньше в глаза не виде%1$Gло|л|ла %2$O4.{x'", ch, obj );
        return;
    }

    int itemCost = get_cost( ch, obj, true, this );
    int serviceCost = itemCost / 100;
    if ((client->silver + client->gold * 100) < serviceCost) {
        tell_dim( client, ch, "Я не справочная контора. Будут деньги, тогда и возвращайся." );
        return;
    }

    tell_dim( client, ch, "Нигде больше не найдешь такого замечательного товара:" );

    lore_fmt_item( client, obj, buf, false );
    client->send_to( " {Y+------------------------------------------------------------------------------------+{x\r\n" );
    stringstream ss( buf.str( ) );
    DLString line;
    while (std::getline( ss, line, '\n' )) {
        client->send_to( dlprintf( " {Y|{x %-75s {Y|{x\r\n", line.c_str( ) ) );
    }
    client->send_to( " {Y+------------------------------------------------------------------------------------+{x\r\n" );

    if (serviceCost < 1) {
        tell_dim( client, ch, "Я сообщаю тебе это совершенно бесплатно." );
    } else {
        deduct_cost( client, serviceCost );
        client->pecho( "%1$^C1 взя%1$Gло|л|ла с тебя {W%2$d{x моне%2$Iту|ты|т за услуги.", ch, serviceCost );
    }
}






#if 0










/*
 * TODO
 */
bool ShopTrader::canServeClient( PCharacter *client )
{
    if (!IS_AWAKE(ch)) {
        interpret_raw( ch, "snore" );
        return false;
    }
    
    if (IS_SET(ch->in_room->areaInstance->area->area_flag, AREA_HOMETOWN)
         && !client->is_npc( ) 
         && IS_SET(client->act, PLR_WANTED))
    {
        do_say( ch, "Criminals are not welcome!" );
        interpret_raw( ch, "yell", "%s the CRIMINAL is over here!", client->getNameP( ) );
        return false;
    }

    if (time_info.hour > closeHour) {
        do_say( ch, "Извини, магазин уже закрыт. Приходи завтра." );
        return false;
    }

    if (time_info.hour < openHour) {
        do_say( ch, "Извини, магазин еще не открылся. Подожди немного." );
        return false;
    }

    if (!ch->can_see( client ) && !client->is_immortal( )) {
        do_say( ch, "Я не торгую с тем, кого не вижу." );
        return false;
    }

    if (ch->fighting) {
        do_say( ch, "Подожди немного, мне сейчас не до тебя." );
        return false;
    }

    return true;
}

Article::Pointer ShopTrader::findArticle( PCharacter *client, DLString &arguments )
{
}

int ShopTrader::getBuyCost( Object *obj )
{
}

int ShopTrader::getSellCost( Object *obj )
{
}

bool ShopTrader::canSell( Object *obj, PCharacter *client )
{
}

bool ShopTrader::canBuy( Object *obj, PCharacter *client )
{
    if (obj->wear_loc != wear_none)
        return false;
        
    if (!ch->can_see( obj ) || !client->can_see( obj ))
        return false;

    return true;
}

void ShopTrader::toStream( PCharacter *client, ostringstream &buf )
{
    Object *obj;
    int number, cost, count;

    for (obj = ch->carrying, number = 1; obj; obj = obj->next_content) 
        if (( cost = getBuyCost( obj ) ) > 0) {
            if (IS_OBJ_STAT( obj, ITEM_INVENTORY ))
                count = 0;
            else {
                count = 1;

                while (obj->next_content
                        && obj->pIndexData == obj->next_content->pIndexData
                        && !str_cmp(obj->getShortDescr( ), obj->next_content->getShortDescr( ) ))
                {
                    obj = obj->next_content;
                    count++;
                }
            }

            buf << dlprintf( "[ {Y%3d{x |%3d %5d %6d ] %s\n\r",
                             number++, 
                             obj->level, 
                             cost,
                             count == 0 ? "  --  " : DLString( count ).c_str( ),
                             obj->getShortDescr( '1' ).c_str( ) );
        }
}

void ShopTrader::msgListEmpty( PCharacter *client )
{
    tell_dim( client, ch, "Мне сегодня нечего тебе предложить.");
}

void ShopTrader::msgListBefore( PCharacter *client )
{
    client->send_to("[ Ном.| Ур.  Цена Кол-во] Объект\n\r");
}

void ShopTrader::msgListAfter( PCharacter *client )
{
}

void ShopTrader::msgListRequest( PCharacter *client )
{
}

void ShopTrader::msgBuyRequest( PCharacter * )
{
}

void ShopTrader::msgArticleNotFound( PCharacter *client )
{
}

void ShopTrader::msgArticleTooFew( PCharacter *client, Article::Pointer )
{
}

#endif
