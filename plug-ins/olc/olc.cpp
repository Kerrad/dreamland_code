/* $Id$
 *
 * ruffina, 2004
 */
#include "grammar_entities_impl.h"
#include "dlfilestream.h"
#include "regexp.h"
#include <xmldocument.h>
#include "stringset.h"
#include "wrapperbase.h"
#include "dlscheduler.h"
#include "schedulertaskroundplugin.h"
#include "plugininitializer.h"

#include <skill.h>
#include <spell.h>
#include <skillmanager.h>
#include "feniamanager.h"
#include "areabehaviormanager.h"
#include <affect.h>
#include <object.h>
#include <pcharacter.h>
#include <npcharacter.h>
#include <commandmanager.h>
#include "race.h"
#include "clanreference.h"
#include "room.h"

#include "olc.h"
#include "olcflags.h"
#include "olcstate.h"
#include "security.h"
#include "areahelp.h"

#include "websocketrpc.h"
#include "interp.h"
#include "merc.h"
#include "handler.h"
#include "act.h"
#include "save.h"
#include "act_move.h"
#include "vnum.h"
#include "mercdb.h"
#include "comm.h"
#include "def.h"


DLString format_longdescr_to_char(const char *descr, Character *ch);
GSN(enchant_weapon);
GSN(enchant_armor);
GSN(none);
CLAN(none);

using namespace std;


enum {
    NDX_ROOM,
    NDX_OBJ,
    NDX_MOB,
};

/** Get self-help article for this area, either a real one or automatically created. */
AreaHelp * get_area_help(AREA_DATA *area)
{
    for (auto &article: area->helps) {
        AreaHelp *ahelp = article.getDynamicPointer<AreaHelp>();
        if (ahelp && ahelp->selfHelp)
            return ahelp;
    }

    return 0;
}


static int next_index_data( Character *ch, Room *r, int ndx_type )
{
    AREA_DATA *pArea;
    
    if (!r)
        return -1;

    pArea = r->area;
    if (!pArea)
        return -1;

    for (int i = pArea->min_vnum; i <= pArea->max_vnum; i++) {
        if (!OLCState::can_edit( ch, i ))
            continue;

        switch (ndx_type) {
        case NDX_ROOM:
            if (!get_room_index( i ))
                return i;
            break;
        case NDX_OBJ:
            if (!get_obj_index( i ))
                return i;
            break;
        case NDX_MOB:
            if (!get_mob_index( i ))
                return i;
            break;
        }
    }

    return -1;
}
    
int next_room( Character *ch, Room *r )
{
    return next_index_data( ch, r, NDX_ROOM );
}

int next_obj_index( Character *ch, Room *r )
{
    return next_index_data( ch, r, NDX_OBJ );
}

int next_mob_index( Character *ch, Room *r )
{
    return next_index_data( ch, r, NDX_MOB );
}

const char *
get_skill_name( int sn, bool verbose )
{
    Skill *skill = SkillManager::getThis( )->find( sn );

    if (skill)
        return skill->getName( ).c_str( );
    else if (verbose)
        return "none";
    else
        return "";
}

void
ptc(Character *ch, const char *fmt, ...)
{
    va_list av;

    va_start(av, fmt);

    DLString rc = vfmt(ch, fmt, av);
    stc(rc.c_str( ), ch);

    va_end(av);
}

void show_fenia_triggers(Character *ch, Scripting::Object *wrapper)
{
    WrapperBase *base = get_wrapper(wrapper);
    if (base) {
        StringSet triggers, misc;

        base->collectTriggers(triggers, misc);
        if (!triggers.empty()) 
            ptc(ch, "{gFenia triggers{x:           %s\r\n", triggers.toString().c_str());
        
        if (!misc.empty()) 
            ptc(ch, "{gFenia fields and methods{x: %s\r\n", misc.toString().c_str());
    }
}

/** Find area with given vnum. */
AREA_DATA *get_area_data(int vnum)
{
    AREA_DATA *pArea;

    for (pArea = area_first; pArea; pArea = pArea->next) {
        if (pArea->vnum == vnum)
            return pArea;
    }
    return 0;
}

void display_resets(Character * ch)
{
    Room *pRoom;
    RESET_DATA *pReset;
    MOB_INDEX_DATA *pMob = NULL;
    char buf[MAX_STRING_LENGTH];
    char final[MAX_STRING_LENGTH];
    int iReset = 0;

    pRoom = ch->in_room;

    final[0] = '\0';

    stc(
           " No.  Loads       Description          Location   Vnum   Mx Mn Description"
           "\n\r"
           "==== ======== ======================== ======== ======== ===== ==========="
           "\n\r", ch);

    for (pReset = pRoom->reset_first; pReset; pReset = pReset->next) {
        OBJ_INDEX_DATA *pObj;
        MOB_INDEX_DATA *pMobIndex;
        OBJ_INDEX_DATA *pObjIndex;
        OBJ_INDEX_DATA *pObjToIndex;
        Room *pRoomIndex;

        final[0] = '\0';
        sprintf(final, "[%2d] ", ++iReset);

        switch (pReset->command) {
        default:
            sprintf(buf, "Bad reset command: %c.", pReset->command);
            strcat(final, buf);
            break;
        case 'M':
            if (!(pMobIndex = get_mob_index(pReset->arg1))) {
                sprintf(buf, "Load Mobile - Bad Mob %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }
            if (!(pRoomIndex = get_room_index(pReset->arg3))) {
                sprintf(buf, "Load Mobile - Bad Room %u\n\r", pReset->arg3);
                strcat(final, buf);
                continue;
            }

            pMob = pMobIndex;
            sprintf(buf, "M[%5u] %-24.24s %-8s R[%5u] %2d-%2d %-15.15s{x\n\r",
                      pReset->arg1, 
                      russian_case(pMob->short_descr, '1').colourStrip( ).c_str( ),
                      "in room", 
                      pReset->arg3,
                      pReset->arg2, 
                      pReset->arg4, 
                      pRoomIndex->name);
            strcat(final, buf);
            break;

        case 'O':
            if (!(pObjIndex = get_obj_index(pReset->arg1))) {
                sprintf(buf, "Load Object - Bad Object %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }
            pObj = pObjIndex;
            if (!(pRoomIndex = get_room_index(pReset->arg3))) {
                sprintf(buf, "Load Object - Bad Room %u\n\r", pReset->arg3);
                strcat(final, buf);
                continue;
            }
            sprintf(buf, "O[%5u] %-24.24s %-8s R[%5u]       %-15.15s{x\n\r",
                      pReset->arg1, 
                      russian_case(pObj->short_descr, '1').colourStrip( ).c_str( ),
                      "in room",
                      pReset->arg3, 
                      pRoomIndex->name);
            strcat(final, buf);
            break;

        case 'P':
            if (!(pObjIndex = get_obj_index(pReset->arg1))) {
                sprintf(buf, "Put Object - Bad Object %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }

            pObj = pObjIndex;

            if (!(pObjToIndex = get_obj_index(pReset->arg3))) {
                sprintf(buf, "Put Object - Bad To Object %u\n\r", pReset->arg3);
                strcat(final, buf);
                continue;
            }

            sprintf(buf, "O[%5u] %-24.24s %-8s O[%5u] %2d-%2d %-15.15s{x\n\r",
                      pReset->arg1,
                      russian_case(pObj->short_descr, '1').colourStrip( ).c_str( ),
                      "inside",
                      pReset->arg3,
                      pReset->arg2,
                      pReset->arg4,
                      russian_case(pObjToIndex->short_descr, '1').colourStrip( ).c_str( ));
            strcat(final, buf);
            break;

        case 'G':
        case 'E':
            if (!(pObjIndex = get_obj_index(pReset->arg1))) {
                sprintf(buf, "Give/Equip Object - Bad Object %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }

            pObj = pObjIndex;

            if (!pMob) {
                sprintf(buf, "Give/Equip Object - No Previous Mobile\n\r");
                strcat(final, buf);
                break;
            }

            sprintf(buf,
                      "O[%5u] %-24.24s %-8.8s M[%5u]       %-15.15s{x\n\r",
                      pReset->arg1,
                      russian_case(pObj->short_descr, '1').colourStrip( ).c_str( ),
                      (pReset->command == 'G') ?
                          wear_none.getName( ).c_str( )
                          : wearlocationManager->find( pReset->arg3 )->getName( ).c_str( ),
                      pMob->vnum,
                      russian_case(pMob->short_descr, '1').colourStrip( ).c_str( ));
            strcat(final, buf);
            break;

        case 'D':
            pRoomIndex = get_room_index(pReset->arg1);
            sprintf(buf, "R[%5u] %s door of %-19s reset to %s{x\n\r",
                      pReset->arg1,
                      DLString(dirs[pReset->arg2].name).capitalize( ).c_str( ),
                      pRoomIndex->name,
                      door_resets_table.name(pReset->arg3).c_str());
            strcat(final, buf);

            break;
        case 'R':
            if (!(pRoomIndex = get_room_index(pReset->arg1))) {
                sprintf(buf, "Randomize Exits - Bad Room %u\n\r", pReset->arg1);
                strcat(final, buf);
                continue;
            }

            sprintf(buf, "R[%5u] Exits are randomized in %s{x\n\r",
                      pReset->arg1, pRoomIndex->name);
            strcat(final, buf);
            break;
        }
        stc(final, ch);
    }
}

struct editor_table_entry {
    const char *arg, *cmd;
} editor_table[] = {
    {"area",   "aedit"},
    {"room",   "redit"},
    {"object", "oedit"},
    {"mobile", "medit"},
    {"help",   "hedit"},
    {NULL, 0,}
};

CMD(edit, 50, "", POS_DEAD, 103, LOG_ALWAYS, 
        "Online editor.")
{
    char command[MAX_INPUT_LENGTH];
    int cmd;

    argument = one_argument(argument, command);

    if (command[0] == '\0') {
//        do_help(ch, "olc");
        return;
    }

    for (cmd = 0; editor_table[cmd].arg != NULL; cmd++) {
        if (!str_prefix(command, editor_table[cmd].arg)) {
            interpret_raw(ch, editor_table[cmd].cmd, "%s", argument);
            return;
        }
    }

//    do_help(ch, "olc");
}

void add_reset(Room * room, RESET_DATA * pReset, int index)
{
    RESET_DATA *reset;
    int iReset = 0;

    if (!room->reset_first) {
        room->reset_first = pReset;
        room->reset_last = pReset;
        pReset->next = NULL;
        return;
    }
    index--;

    if (index == 0) {                /* First slot (1) selected. */
        pReset->next = room->reset_first;
        room->reset_first = pReset;
        return;
    }

    // If negative slot( <= 0 selected) then this will find the last.
    for (reset = room->reset_first; reset->next; reset = reset->next) {
        if (++iReset == index)
            break;
    }

    pReset->next = reset->next;
    reset->next = pReset;
    if (!pReset->next)
        room->reset_last = pReset;
}

CMD(resets, 50, "", POS_DEAD, 103, LOG_ALWAYS, 
        "Online resets editor.")
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char arg4[MAX_INPUT_LENGTH];
    char arg5[MAX_INPUT_LENGTH];
    char arg6[MAX_INPUT_LENGTH];
    char arg7[MAX_INPUT_LENGTH];
    RESET_DATA *pReset = NULL;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    argument = one_argument(argument, arg3);
    argument = one_argument(argument, arg4);
    argument = one_argument(argument, arg5);
    argument = one_argument(argument, arg6);
    argument = one_argument(argument, arg7);
    
    if (!OLCState::can_edit( ch, ch->in_room->vnum )) {
        stc("Resets: Invalid security for editing this area.\n\r", ch);
        return;
    }

    if (arg1[0] == '\0') {
        if (ch->in_room->reset_first) {
            stc("Resets: M = mobile, R = room, O = object\n\r", ch);
            display_resets(ch);
        }
        else
            stc("No resets in this room.\n\r", ch);
    }

    if (is_number(arg1)) {
        Room *pRoom = ch->in_room;

        if (!str_cmp(arg2, "delete")) {
            int insert_loc = atoi(arg1);

            if (!ch->in_room->reset_first) {
                stc("No resets in this area.\n\r", ch);
                return;
            }
            if (insert_loc - 1 <= 0) {
                pReset = pRoom->reset_first;
                pRoom->reset_first = pRoom->reset_first->next;
                if (!pRoom->reset_first)
                    pRoom->reset_last = NULL;
            }
            else {
                int iReset = 0;
                RESET_DATA *prev = NULL;

                for (pReset = pRoom->reset_first; pReset; pReset = pReset->next) {
                    if (++iReset == insert_loc)
                        break;
                    prev = pReset;
                }
                if (!pReset) {
                    stc("Reset not found.\n\r", ch);
                    return;
                }
                if (prev)
                    prev->next = prev->next->next;
                else
                    pRoom->reset_first = pRoom->reset_first->next;

                for (pRoom->reset_last = pRoom->reset_first;
                     pRoom->reset_last->next;
                     pRoom->reset_last = pRoom->reset_last->next);
            }
            free_reset_data(pReset);
            SET_BIT(ch->in_room->area->area_flag, AREA_CHANGED);
            stc("Reset deleted.\n\r", ch);
        }
        else if ((!str_cmp(arg2, "mob") && is_number(arg3))
                 || (!str_cmp(arg2, "obj") && is_number(arg3))) {
            if (!str_cmp(arg2, "mob")) {
                pReset = new_reset_data();
                pReset->command = 'M';
                if (get_mob_index(is_number(arg3) ? atoi(arg3) : 1) == NULL) {
                    stc("Монстр не существует.\n\r", ch);
                    return;
                }
                pReset->arg1 = atoi(arg3);
                pReset->arg2 = is_number(arg4) ? atoi(arg4) : 1;        /* Max # */
                pReset->arg3 = ch->in_room->vnum;
                pReset->arg4 = is_number(arg5) ? atoi(arg5) : 1;        /* Min # */
            }
            else if (!str_cmp(arg2, "obj")) {
                pReset = new_reset_data();
                pReset->arg1 = atoi(arg3);
                if (!str_prefix(arg4, "inside")) {
                    pReset->command = 'P';
                    pReset->arg2 = 0;
                    if ((get_obj_index(is_number(arg5) ? atoi(arg5) : 1))->item_type != ITEM_CONTAINER) {
                        stc("Предмет не является контейнером.\n\r", ch);
                        return;
                    }
                    pReset->arg2 = is_number(arg6) ? atoi(arg6) : 1;
                    pReset->arg3 = is_number(arg5) ? atoi(arg5) : 1;
                    pReset->arg4 = is_number(arg7) ? atoi(arg7) : 1;
                }
                else if (!str_cmp(arg4, "room")) {
                    pReset = new_reset_data();
                    pReset->command = 'O';
                    if (get_obj_index(atoi(arg3)) == NULL) {
                        stc("Предметов с таким номером не существует.\n\r", ch);
                        return;
                    }
                    pReset->arg1 = atoi(arg3);
                    pReset->arg2 = 0;
                    pReset->arg3 = ch->in_room->vnum;
                    pReset->arg4 = 0;
                }
                else {
                    Wearlocation *wl;
                    
                    if (!( wl = wearlocationManager->findExisting( arg4 ) )) {
                        stc("Resets: '? wear-loc'\n\r", ch);
                        return;
                    }
                    pReset = new_reset_data();
                    if (get_obj_index(atoi(arg3)) == NULL) {
                        stc("Предметов с таким номером не существует.\n\r", ch);
                        return;
                    }
                    pReset->arg1 = atoi(arg3);
                    pReset->arg3 = wl->getIndex( );
                    if (pReset->arg3 == wear_none)
                        pReset->command = 'G';
                    else
                        pReset->command = 'E';
                }
            }
            add_reset(ch->in_room, pReset, atoi(arg1));
            SET_BIT(ch->in_room->area->area_flag, AREA_CHANGED);
            stc("Reset added.\n\r", ch);
        }
        else {
            stc("Syntax: RESET <number> OBJ <vnum> <wear_loc>\n\r", ch);
            stc("        RESET <number> OBJ <vnum> inside <vnum> [limit] [count]\n\r", ch);
            stc("        RESET <number> OBJ <vnum> room\n\r", ch);
            stc("        RESET <number> MOB <vnum> [max # area] [max # room]\n\r", ch);
            stc("        RESET <number> DELETE\n\r", ch);
        }
    }
}

CMD(alist, 50, "", POS_DEAD, 103, LOG_ALWAYS, 
        "List areas.")
{
    AREA_DATA *pArea;

    const DLString lineFormat = 
            "[" + web_cmd(ch, "aedit $1", "%3d") 
            + "] {W%-29s {x(%5u-%5u) %17s %s\n\r";

    ptc(ch, "[%3s] %-29s   (%5s-%5s) %-17s %s\n\r",
      "Num", "Area Name", "lvnum", "uvnum", "Filename", "Help");

    for (pArea = area_first; pArea; pArea = pArea->next) {
        DLString hedit = "";
        AreaHelp *ahelp = get_area_help(pArea);
        if (ahelp && ahelp->getID() > 0) {
            DLString id(ahelp->getID());
            hedit = web_cmd(ch, "hedit " + id, "hedit " + id);
        }
            
        ch->send_to(
            dlprintf(lineFormat.c_str(), 
                pArea->vnum, 
                DLString(pArea->name).colourStrip().cutSize(29).c_str(),
                pArea->min_vnum, pArea->max_vnum,
                pArea->area_file->file_name,
                hedit.c_str()));
    }
}

static int get_obj_reset_level( AREA_DATA *pArea, int keyVnum )
{
    int mobVnum = -1;
    int level = 200;
    
    if (keyVnum <= 0 || !get_obj_index( keyVnum ))
        return pArea->low_range;
        
    for (auto room: roomPrototypes)
        for(RESET_DATA *pReset = room->reset_first;pReset;pReset = pReset->next)
            switch(pReset->command) {
                case 'M':
                    mobVnum = pReset->arg1;
                    break;
                case 'G':
                case 'E':
                    if (pReset->arg1 == keyVnum && mobVnum > 0)
                        level = min( get_mob_index(mobVnum)->level, level );
                    break;
                case 'O':
                    if (pReset->arg1 == keyVnum) 
                        level = min( pArea->low_range, level );
                    break; 
                case 'P':
                    if (pReset->arg1 == keyVnum) {
                        OBJ_INDEX_DATA *in = get_obj_index( pReset->arg3 );

                        if (in->item_type == ITEM_CONTAINER
                            && IS_SET(in->value[1], CONT_LOCKED))
                        {
                            level = min( get_obj_reset_level( room->area, in->value[2] ), 
                                         level );
                        }
                        else
                            level = min( pArea->low_range, level );
                    }
                    break;
            }
    
    return (level == 200 ? pArea->low_range : level);
}            

static DLString trim(const DLString& str, const string& chars = "\t\n\v\f\r ")
{
    DLString line = str;
    line.erase(line.find_last_not_of(chars) + 1);
    line.erase(0, line.find_first_not_of(chars));
    return line;
}

static void door_rename(EXIT_DATA *pexit, const char *keyword, const char *short_descr, const char *extra_keyword = NULL)
{
    DLString newKeyword = keyword;
    if (extra_keyword && extra_keyword[0] && !is_name(extra_keyword, keyword))
            newKeyword << " " << extra_keyword;

    LogStream::sendNotice() << "door [" << pexit->keyword << "] -> [" << newKeyword << "], [" << short_descr << endl;
    free_string(pexit->keyword);
    pexit->keyword = str_dup(newKeyword.c_str());
    free_string(pexit->short_descr);
    pexit->short_descr = str_dup(short_descr);
}

struct door_rename_info {
    const char *orig_keyword;
    const char *keyword;
    const char *short_descr;
};
static const struct door_rename_info renames [] = {
    { "doors", "двери", "двер|и|ей|ям|и|ьми|ях" },
    { "door", "дверь", "двер|ь|и|и|ь|ью|и" },
    { "celldoor", "door дверь", "двер|ь|и|и|ь|ью|и" },
    { "marble square", "мраморная клетка", "мраморн|ая|ой|ой|ую|ой|ой клетк|а|и|е|у|ой|е" },
    { "steel", "steel door дверь", "двер|ь|и|и|ь|ью|и" },
    { "дверь", "door", "двер|ь|и|и|ь|ью|и" },
    { "ldthm", "door дверь", "двер|ь|и|и|ь|ью|и" },
    { "двустворчатая", "door дверь", "двустворчат|ая|ой|ой|ую|ой|ой двер|ь|и|и|ь|ью|и" },
    { "opening", "отверстие", "отверсти|е|я|ю|е|ем|и" },
    { "калитка", "wicket", "калитк|а|у|е|у|ой|е" },
    { "cell", "door дверь", "двер|ь|и|и|ь|ью|и" },
    { "дверь домика", "door house дверь домика", "двер|ь|и|и|ь|ью|и домика" },
    { "дверь клетки", "door cage дверь клетки", "двер|ь|и|и|ь|ью|и клетки" },
    { "door cage дверь клетки", "door cage дверь клетки", "двер|ь|и|и|ь|ью|и клетки" },
    { "могильный камень tomb stone", "могильный камень tomb stone", "могильн|ый|ого|ому|ый|ым|ом кам|ень|ня|ню|ень|нем|не" },
    { "hatch", "люк", "люк||а|у||ом|е" },
    { "gates", "ворота", "ворот|а||ам|а|ами|ах" },
    { "stones", "камни", "камн|и|ей|ям|и|ями|ях" },
    { "воротца", "gates", "ворот|ца|ец|цам|ца|цами|цах" },
    { "gate", "ворота", "ворот|а||ам|а|ами|ах" },
    { "gateway", "ворота", "ворот|а||ам|а|ами|ах" },
    { "trapdoor", "люк", "люк||а|у||ом|е" },
    { "тайник", "secret", "тайник||а|у||ом|е" },
    { "люк", "trapdoor", "люк||а|у||ом|е" },
    { "tapestry", "гобелен", "гобелен||а|у||ом|е" },
    { "manhole", "проход", "проход||а|у||ом|е" },
    { "wall", "стена", "стен|а|у|е|у|ой|е" },
    { "floorboard", "половица", "половиц|а|у|е|у|ей|е" },
    { "floorboards", "половица", "половиц|а|у|е|у|ей|е" },
    { "curtain", "штора", "штор|а|у|е|у|ой|е" },
    { "cage", "клетка", "клетк|а|у|е|у|ой|е" },
    { "hole", "дыра", "дыр|а|у|е|у|ой|е" },
    { "tree", "дерево", "дерев|о|а|у|о|ом|е" },
    { "bookcase", "шкаф книжный", "шкаф||а|у||ом|е" },
    { "skeleton", "скелет", "скелет||а|у||ом|е" },
    { "painting", "картина", "картин|а|у|е|у|ой|е" },
    { "panel", "панель", "панел|ь|и||ь|ью|и" },
    { "statue", "статуя", "стату|я|ю|е|ю|ей|е" },
    { "stone", "камень", "кам|ень|ня|ню|ень|нем|не" },
    { "grate", "решетка", "решетк|а|и|е|у|ой|е" },
    { "bookshelf", "полка", "полк|а|и|е|у|ой|е" },
    { "tomb", "могила", "могил|а|ы|е|у|ой|е" },
    { "crack", "трещина", "трещин|а|ы|е|у|ой|е" },
    { "sculpture", "скульптура", "скульптур|а|ы|е|у|ой|е" },
    { "throne", "трон", "трон||а|у||ом|е" },
    { "bars", "решетка", "решетк|а|и|е|у|ой|е" },
    { "bushes", "кусты", "куст|ы|ов|ам|ы|ами|ах" },
    { "thicket", "заросли", "заросл|и|ей|ям|и|ями|ях" },
    { "drapes", "портьеры", "портьер|ы||ам|ы|ами|ах" },
    { "underbrush", "подлесок", "подлес|ок|ка|ку|ок|ком|ке" },
    { "forcefield", "поле силовое", "силов|ое|ого|ому|ое|ым|ом пол|е|я|ю|е|ем|е" },
    { "enterance", "entrance вход", "вход||а|у||ом|е" },
    { "lid", "крышка", "крышк|а|и|е|у|ой|е" },
    { "floor", "пол", "пол||а|у||ом|е" },
    { "boulder", "валун", "валун||а|у||ом|е" },
    { "sesame", "сезам", "сезам||а|у||ом|е" },
    { "exit", "выход", "выход||а|у||ом|е" },
    { "ranks", "ряды", "ряд|ы|ом|ам|ы|ами|ах" },
    { "tank", "аквариум", "аквариум||а|у||ом|е" },
    { "ceiling", "потолок", "потол|ок|ка|ку|ок|ком|ке" },
    { "hope", "надежда", "надежд|а|у|е|у|ой|е" },
    { "secret", "секретная потайная дверь", "потайн|ая|ой|ой|ую|ой|ой двер|ь|и|и|ь|ью|и" },
    { 0 },
};

static bool door_match_and_rename_exact(EXIT_DATA *pexit) 
{
    const char *k = pexit->keyword;
    for (int r = 0; renames[r].orig_keyword; r++) {
        if (!str_cmp(renames[r].orig_keyword, k)) {
            door_rename(pexit, k, renames[r].short_descr, renames[r].keyword);
            return true;
        }
    }
    return false;
}
    
static bool door_match_and_rename_substring(EXIT_DATA *pexit) 
{
    const char *k = pexit->keyword;
    DLString kStr(k);

    for (int r = 0; renames[r].orig_keyword; r++) {
        if (kStr.isName(renames[r].orig_keyword)) {
            door_rename(pexit, k, renames[r].short_descr, renames[r].keyword);
            return true;
        }
    }
    return false;
}

static bool door_rename_as_russian(EXIT_DATA *pexit)
{
    const char *k = pexit->keyword;
    if (DLString(k).isRussian()) {
        door_rename(pexit, k, k, "door дверь");
        return true;
    }
    return false;
}

static DLString find_word_mention(const char *text, const list<RussianString> &words)
{
    DLString t = text;
    t.colourStrip();
    t.toLower();

    for (const auto &word: words)
        for (int c = Grammar::Case::NONE; c < Grammar::Case::MAX; c++) {
            DLString myword = word.decline(c);
            if (t.isName(myword))
                return myword;
        }

    return DLString::emptyString;
}

CMD(abc, 50, "", POS_DEAD, 106, LOG_ALWAYS, "")
{
    DLString args = argument;
    DLString arg = args.getOneArgument();

    if (arg == "water") {
        ostringstream buf;
        const DLString lineFormat = "[" + web_cmd(ch, "goto $1", "%5d") + "] {W%-29s{x ({C%s{x)";

        list<RussianString> water;
        water.push_back(RussianString("вод|а|ы|е|у|ой|е"));
        water.push_back(RussianString("водоем||а|у||ом|е" ));
        water.push_back(RussianString("водопад||а|у||ом|е" ));
        water.push_back(RussianString("озер|о|а|у|о|ом|е"));
        water.push_back(RussianString("мор|е|я|ю|е|ем|е" ));
        water.push_back(RussianString("рек|а|и|е|у|ой|е" ));
        water.push_back(RussianString("причал||а|у||ом|е" ));
        water.push_back(RussianString("лодк|а|и|е|у|ой|е" ));
        water.push_back(RussianString("верф|ь|и|и|ь|ью|и" ));
        water.push_back(RussianString("набережн|ая|ой|ой|ую|ой|ой"));
        water.push_back(RussianString("корабл|ь|я|ю|ь|ем|я"));

        buf << "Всех неводные комнаты с упоминанием воды, без флагов indoors и near_water:" << endl;

        for (auto room: roomPrototypes) {
            if (IS_SET(room->room_flags, ROOM_INDOORS|ROOM_NEAR_WATER|ROOM_MANSION))
                continue;

            if (IS_WATER(room))
                continue;

            if (!room->isCommon())
                continue;

            if (!str_cmp(room->area->area_file->file_name, "galeon.are"))
                continue;

            if (room->sector_type == SECT_AIR)
                continue;
            if (room->sector_type == SECT_UNDERWATER)
                continue;

            DLString myword = find_word_mention(room->description, water);
            if (!myword.empty()) {
                buf << dlprintf(lineFormat.c_str(), room->vnum, room->name, myword.c_str()) << endl;
                continue;
            }

            myword = find_word_mention(room->name, water);
            if (!myword.empty())
                buf << dlprintf(lineFormat.c_str(), room->vnum, room->name, myword.c_str()) << endl;
        }

        

        page_to_char( buf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "eexit") {
        ostringstream abuf, cbuf, mbuf;
        abuf << endl << "Экстравыходы везде:" << endl;
        mbuf << endl << "Экстравыходы в особняках и пригородах:" << endl;
        cbuf << endl << "Экстравыходы в кланах:" << endl;

        const DLString lineFormat = "[" + web_cmd(ch, "goto $1", "%5d") + "] %-35s{x [{C%s{x]";
        for (auto room: roomPrototypes) {
            ostringstream *buf;
            if (IS_SET(room->room_flags, ROOM_MANSION) || !str_prefix("ht", room->area->area_file->file_name))
                buf = &mbuf;
            else if (room->clan != clan_none)
                buf = &cbuf;
            else
                buf = &abuf;
            for (EXTRA_EXIT_DATA *eexit = room->extra_exit; eexit; eexit = eexit->next) {
                (*buf) << dlprintf(lineFormat.c_str(), room->vnum, room->name, eexit->keyword) << endl;
            }
        }
        
        page_to_char( mbuf.str( ).c_str( ), ch );
        page_to_char( cbuf.str( ).c_str( ), ch );
        page_to_char( abuf.str( ).c_str( ), ch );

        return;
    }

    if (!ch->isCoder( ))
        return;

    ostringstream buf;

    if (arg == "magic") {
        for (AREA_DATA *area = area_first; area; area = area->next) {
            buf << "{C       ******   {C" << area->name << "      ********{x" << endl;
            for (int i = area->min_vnum; i <= area->max_vnum; i++) {
                OBJ_INDEX_DATA *pObj = get_obj_index( i );
                if (!pObj)
                    continue;
                if (!IS_SET(pObj->extra_flags, ITEM_MAGIC))
                    continue;
                if (pObj->item_type == ITEM_PORTAL)
                    continue;
                
                REMOVE_BIT(pObj->extra_flags, ITEM_MAGIC);
                SET_BIT(pObj->area->area_flag, AREA_CHANGED);

                buf << dlprintf("[%5d] %s\n", 
                        pObj->vnum, russian_case(pObj->short_descr, '1').c_str( ));
            }
        }

        page_to_char(buf.str().c_str(), ch);
        return;
    }
    
    if (arg == "nomagic") {
        for (Object *obj = object_list; obj; obj = obj->next) {
            if (!IS_SET(obj->extra_flags, ITEM_MAGIC))
                continue;
            
            if (IS_SET(obj->pIndexData->extra_flags, ITEM_MAGIC))
                continue;
            
            if (obj->enchanted) {
                if (obj->affected 
                    && (obj->affected->affect_find( gsn_enchant_armor )
                       || obj->affected->affect_find( gsn_enchant_weapon )))
                    continue;
            }

            Room *r = obj->getRoom( );

            REMOVE_BIT( obj->extra_flags, ITEM_MAGIC );
            buf << "nomagic [" << obj->pIndexData->vnum << "] " 
                << "[" << obj->getID( ) << "] "
                << obj->getShortDescr( '1' ) << " "
                << (obj->getRealShortDescr( ) ? "*" : "")
                << " " << (obj->getOwner( ) ? obj->getOwner( ) : "")
                << " " << "[" << r->vnum << "] " 
                << r->name 
                << endl;
            save_items( r );
        }

        page_to_char( buf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "gender") {
        DLString prefix = args.getOneArgument();
        DLString mg = args.getOneArgument();

        ch->printf("Prefix=[%s] mg=[%s]\n", prefix.c_str(), mg.c_str());

        for (int i = 0; i < MAX_KEY_HASH; i++)
            for(OBJ_INDEX_DATA *pObj = obj_index_hash[i]; pObj; pObj = pObj->next) {
                DLString oname = russian_case(pObj->short_descr, '1').colourStrip().toLower();
                
                if (oname.isName(prefix)) {
                    buf << dlprintf("[%5d] %s\n", pObj->vnum, oname.c_str());

                    if (!mg.empty()) {
                        pObj->gram_gender = Grammar::MultiGender(mg.c_str());
                        SET_BIT(pObj->area->area_flag, AREA_CHANGED);
                    }
                }
            }

        page_to_char(buf.str().c_str(), ch);
        return;
    }
    
    if (arg == "swim") {
        for (Character *wch = char_list; wch; wch = wch->next) {
            if (!wch->is_npc())
                continue;
            if (!IS_WATER(wch->in_room))
                continue;
            if (IS_AFFECTED(wch, AFF_FLYING) || IS_AFFECTED(wch, AFF_SWIM))
                continue;
            if (boat_get_type(wch) != BOAT_NONE)
                continue;

            buf << dlprintf("[%5d] (%14s) %s\n", 
                            wch->getNPC()->pIndexData->vnum, 
                            wch->getRace()->getName().c_str(),
                            wch->getNameP('1').c_str());
        }

        page_to_char(buf.str().c_str(), ch);
        return;
    }


    if (arg == "linkwrapper") {
        DLString vnumStr = args.getOneArgument();
        if (vnumStr.isNumber()) {
            Room *r = get_room_index(vnumStr.toInt());
            if (r) {
                if (FeniaManager::wrapperManager)
                    FeniaManager::wrapperManager->linkWrapper(r);
                ch->println("Room wrapped.");
            }
            else 
                ch->println("Room not found.");
        }
        else
            ch->println("Usage: abc linkwrapper <room vnum>");

    }

    if (arg == "objdup") {
        typedef list<Object *> ObjectList;
        ObjectList::iterator o;
        map<long long, ObjectList> ids;
        map<long long, ObjectList>::iterator i;

        for (Object *obj = object_list; obj; obj = obj->next) {
            ids[obj->getID( )].push_back( obj );
        }

        for (i = ids.begin( ); i != ids.end( ); i++)
            if (i->second.size( ) > 1)
                for (o = i->second.begin( ); o != i->second.end( ); o++)
                    buf << "[" << i->first << "] " 
                        << (*o)->getShortDescr( '1' ) 
                        << " [" << (*o)->getRoom( )->vnum << "]" << endl;
            
        page_to_char( buf.str( ).c_str( ), ch );
        return;
    }


    if (arg == "mobdup") {
        typedef list<NPCharacter *> MobileList;
        MobileList::iterator m;
        map<long long, MobileList> ids;
        map<long long, MobileList>::iterator i;

        for (Character *wch = char_list; wch; wch = wch->next) {
            if (wch->is_npc( ))
                ids[wch->getID( )].push_back( wch->getNPC( ) );
        }

        for (i = ids.begin( ); i != ids.end( ); i++)
            if (i->second.size( ) > 1)
                for (m = i->second.begin( ); m != i->second.end( ); m++)
                    buf << "[" << i->first << "] " 
                        << (*m)->getNameP( '1' ) 
                        << " [" << (*m)->in_room->vnum << "]" << endl;
            
        page_to_char( buf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "objname") {
        int cnt = 0, hcnt = 0, rcnt = 0;
        ostringstream buf, hbuf, rbuf;

        for (int i = 0; i < MAX_KEY_HASH; i++)
        for (OBJ_INDEX_DATA *pObj = obj_index_hash[i]; pObj; pObj = pObj->next) {
            DLString longd = pObj->description;
            longd.colourstrip( );

            static RegExp pattern_rus("[а-я]");
            static RegExp pattern_longd("^.*\\(([-a-z ]+)\\).*$");
            
            if (!pattern_rus.match( longd )) 
                continue;

            if (IS_SET(pObj->area->area_flag, AREA_NOQUEST|AREA_WIZLOCK|AREA_HIDDEN))
                continue;
            
            {
                DLString names = DLString( pObj->name );
                RussianString rshortd(pObj->short_descr, pObj->gram_gender );
                DLString shortd = rshortd.decline( '7' ).colourStrip( );
                if (!arg_contains_someof( shortd, pObj->name )) {
                    rcnt ++;
                    rbuf << pObj->vnum << ": [" << rshortd.decline( '1' ) << "] [" << pObj->name << "]" <<  endl;
                }
            }
            if (!pattern_longd.match( longd )) {
                buf << pObj->vnum << ": [" << longd << "] [" << pObj->name << "]" <<  endl;
                cnt++;
            } else {
                RegExp::MatchVector matches = pattern_longd.subexpr( longd.c_str( ) );
                if (matches.size( ) < 1) {
                    buf << pObj->vnum << ": [" << longd << "] [" << pObj->short_descr << "]" <<  endl;
                    cnt++;
                } else {
                    
                    DLString hint = matches.front( );
                    if (!is_name( hint.c_str( ), pObj->name )) {
                        hbuf << dlprintf( "%6d : [%35s] hint [{G%10s{x] [{W%s{x]\r\n",
                                pObj->vnum, longd.c_str( ), hint.c_str( ), pObj->name );
                        hcnt++;
                    }
                }
            }
        }

//        ch->printf("Найдено %d длинных имен предметов без подсказок (пустых).\r\n", cnt);
//        page_to_char( buf.str( ).c_str( ), ch );
//        ch->printf("Найдено %d несоответствий подсказок в длинном имени предметов.\r\n", hcnt);
//        page_to_char( hbuf.str( ).c_str( ), ch );
        ch->printf("Найдено %d предметов где в short descr не встречаются имена.\r\n", rcnt);
        page_to_char( rbuf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "rlim") {
        
        if (args.empty( )) {
            for (Object *obj = object_list; obj; obj = obj->next) {
                    if (obj->pIndexData->limit < 0)
                        continue;
                    if (obj->in_room == NULL)
                        continue;
                    if (obj->pIndexData->area == obj->in_room->area)
                        continue;
                    if (obj->timestamp > 0)
                        continue;
                    
                    ch->printf("Found %s [%d] in [%d] %s\r\n", 
                            obj->getShortDescr('1').c_str( ), obj->pIndexData->vnum,
                            obj->in_room->vnum, obj->in_room->area->name);
            }

            return;
        }

        if (!is_number( args.c_str( )))
            return;

        Room *r = get_room_index( atoi( args.c_str( ) ) );
        if (!r) {
            ch->println("Room vnum not found.");
            return;
        }

        for (Object *obj = r->contents; obj; obj = obj->next_content) {
                if (obj->pIndexData->limit < 0)
                    continue;
                if (obj->in_room == NULL)
                    continue;
                if (obj->pIndexData->area == obj->in_room->area)
                    continue;
                if (obj->timestamp > 0)
                    continue;

                obj->timestamp = 1531034011;
                save_items( obj->in_room );
                ch->printf("Marking %s [%d] in [%d] %s\r\n", 
                        obj->getShortDescr('1').c_str( ), obj->pIndexData->vnum,
                            obj->in_room->vnum, obj->in_room->area->name);
        }
        ch->println("Done marking limits.");
        return;
    }

    if (arg == "screenreader") {
        ostringstream buf;

        SET_BIT(ch->config, CONFIG_SCREENREADER);

        for (int i = 0; i < MAX_KEY_HASH; i++)
        for (OBJ_INDEX_DATA *pObj = obj_index_hash[i]; pObj; pObj = pObj->next) {
            buf << format_longdescr_to_char(pObj->description, ch) << "{x" << endl;
        }

        for (int i = 0; i < MAX_KEY_HASH; i++)
        for (MOB_INDEX_DATA *pMob = mob_index_hash[i]; pMob; pMob = pMob->next) {
            buf << pMob->long_descr << format_longdescr_to_char(pMob->long_descr, ch) << "{x" << endl;            
        }

        REMOVE_BIT(ch->config, CONFIG_SCREENREADER);
        page_to_char(buf.str().c_str(), ch);
        return;
    }


    if (arg == "mobname") {
        int cnt = 0, hcnt = 0, rcnt = 0;
        ostringstream buf, hbuf, rbuf;

        for (int i = 0; i < MAX_KEY_HASH; i++)
        for (MOB_INDEX_DATA *pMob = mob_index_hash[i]; pMob; pMob = pMob->next) {
            DLString names = DLString(pMob->player_name).toLower();
            DLString longd = trim(pMob->long_descr).toLower();
            longd.colourstrip( );

            static RegExp pattern_rus("[а-я]");
            static RegExp pattern_longd("^.*\\(([-a-z ]+)\\).*$");
            
            if (!pattern_rus.match( longd )) 
                continue;

            if (IS_SET(pMob->area->area_flag, AREA_WIZLOCK|AREA_HIDDEN))
                continue;
            
            {
                RussianString rshortd(pMob->short_descr, MultiGender(pMob->sex, pMob->gram_number));
                DLString shortd = rshortd.decline( '7' ).colourStrip( ).toLower();
                if (!arg_contains_someof( shortd, pMob->player_name )) {
                    rcnt ++;
                    rbuf << pMob->vnum << ": [" << rshortd.decline( '1' ) << "] [" << pMob->player_name << "]" <<  endl;
                }
            }

            RegExp::MatchVector matches = pattern_longd.subexpr( longd.c_str( ) );
            if (matches.size( ) < 1) {
                buf << pMob->vnum << ": [" << longd << "] [" << pMob->short_descr << "]" <<  endl;
                cnt++;
            } else {
                
                DLString hint = matches.front( );
                if (!is_name(hint.c_str(), names.c_str())) {
                    hbuf << dlprintf( "%6d : [%35s] hint [{G%10s{x] [{W%s{x]\r\n",
                            pMob->vnum, longd.c_str( ), hint.c_str( ), pMob->player_name );
                    hcnt++;
                }
            }
        }

        ch->printf("\r\n{RНайдено %d несоответствий подсказок в длинном имени моба.{x\r\n", hcnt);
        page_to_char( hbuf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "doors") {
        ostringstream buf;
        int cnt = 0, total = 0, unprocessed = 0;

        for (auto room: roomPrototypes) {
            for (int door = 0; door < DIR_SOMEWHERE; door++) {
                EXIT_DATA *pexit;

                if (!(pexit = room->exit[door]))
                    continue;
                if (!IS_SET(pexit->exit_info_default, EX_ISDOOR))
                    continue;
                if (!pexit->keyword || !pexit->keyword[0]) 
                    continue;

                total++;

                unprocessed++;

                if (door_match_and_rename_exact(pexit)
                    || door_match_and_rename_substring(pexit)
                    || door_rename_as_russian(pexit))
                    continue;

                cnt++;
                buf << dlprintf("[{G%5d{x] %30s %20s: {g%s{x\r\n",
                        room->vnum, room->name, room->area->name,
                        pexit->keyword && pexit->keyword[0] ? pexit->keyword : "");
                door_rename(pexit, pexit->keyword, "двер|ь|и|и|ь|ью|и");
            }
        }

        buf << "Всего " << cnt << "/" << unprocessed << "/" << total << "." << endl;           
        page_to_char( buf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "relig") {
        for (int i = 0; i < religionManager->size( ); i++) {
            Religion *r = religionManager->find(i);
            if (r)
                buf << dlprintf("{c%-14s {w%-14s{x : %s",
                    r->getRussianName().ruscase('1').c_str(), 
                    r->getShortDescr().c_str(), 
                    r->getDescription().c_str())
                    << endl;
        }
        page_to_char( buf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "rot") {
        for (int i = 0; i < MAX_KEY_HASH; i++)
        for (OBJ_INDEX_DATA *pObj = obj_index_hash[i]; pObj; pObj = pObj->next) {
           if (IS_SET(pObj->extra_flags, ITEM_ROT_DEATH))
                buf << dlprintf("[%20s] [%6d] %s",
                    pObj->area->name, pObj->vnum, pObj->short_descr) << endl;
        }
        page_to_char( buf.str( ).c_str( ), ch );
        return;
    }

    if (arg == "maxhelp") {
        ch->printf("Max help ID is %d.\r\n", helpManager->getLastID());
        return;
    }

    if (arg == "readroom") {
        Integer vnum;
        Room *room;

        if (args.empty() || !Integer::tryParse(vnum, args)) {
            ch->println("abc readroom <vnum>");
            return;
        }

        room = get_room_index(vnum);
        if (!room) {
            ch->printf("Room vnum [%d] not found.\r\n", vnum.getValue());
            return;
        }
        
        ch->printf("Loading room objects for '%s' [%d], check logs for details.\r\n", 
                    room->name, room->vnum);
        load_room_objects(room, const_cast<char *>("/tmp"), false);
        return;
    }

}



