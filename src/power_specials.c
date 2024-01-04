/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file power_specials.c
 *     power_specials support functions.
 * @par Purpose:
 *     Functions to power_specials.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     11 Mar 2010 - 12 May 2010
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "power_specials.h"

#include "globals.h"
#include "bflib_basics.h"
#include "bflib_math.h"

#include "thing_creature.h"
#include "thing_navigate.h"
#include "thing_effects.h"
#include "config_terrain.h"
#include "player_data.h"
#include "dungeon_data.h"
#include "creature_control.h"
#include "creature_states_pray.h"
#include "power_hand.h"
#include "map_blocks.h"
#include "spdigger_stack.h"
#include "thing_corpses.h"
#include "thing_objects.h"
#include "front_simple.h"
#include "frontend.h"
#include "frontmenu_ingame_map.h"
#include "gui_frontmenu.h"
#include "gui_soundmsgs.h"
#include "game_legacy.h"

#include "keeperfx.hpp"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
/******************************************************************************/
#ifdef __cplusplus
}
#endif
/******************************************************************************/
long transfer_creature_scroll_offset;
long resurrect_creature_scroll_offset;
unsigned short dungeon_special_selected;
/******************************************************************************/

/**
 * Makes a bonus level for current SP level visible on the land map screen.
 */
TbBool activate_bonus_level(struct PlayerInfo *player)
{
  SYNCDBG(5,"Starting");
  set_flag(game.flags_font, FFlg_unk02);
  LevelNumber sp_lvnum = get_loaded_level_number();
  TbBool result = set_bonus_level_visibility_for_singleplayer_level(player, sp_lvnum, true);
  if (!result)
    ERRORLOG("No Bonus level assigned to level %d",(int)sp_lvnum);
  clear_flag(game.operation_flags, GOF_SingleLevel);
  return result;
}

void multiply_creatures_in_dungeon_list(struct Dungeon *dungeon, long list_start)
{
    unsigned long k = 0;
    int i = list_start;
    while (i != 0)
    {
        struct Thing* thing = thing_get(i);
        struct CreatureControl* cctrl = creature_control_get_from_thing(thing);
        if (thing_is_invalid(thing) || creature_control_invalid(cctrl))
        {
            ERRORLOG("Jump to invalid creature detected");
            break;
        }
        i = cctrl->players_next_creature_idx;
        // Thing list loop body
        struct Thing* tncopy = create_creature(&thing->mappos, thing->model, dungeon->owner);
        if (thing_is_invalid(tncopy))
        {
            WARNLOG("Can't create a copy of creature");
            break;
        }
        set_creature_level(tncopy, cctrl->explevel);
        tncopy->health = thing->health;
        // Thing list loop body ends
        k++;
        if (k > CREATURES_COUNT)
        {
            ERRORLOG("Infinite loop detected when sweeping creatures list");
            break;
        }
    }
}

void multiply_creatures(struct PlayerInfo *player)
{
    struct Dungeon* dungeon = get_players_dungeon(player);
    // Copy 'normal' creatures
    multiply_creatures_in_dungeon_list(dungeon, dungeon->creatr_list_start);
    // Copy 'special digger' creatures
    multiply_creatures_in_dungeon_list(dungeon, dungeon->digger_list_start);
}

void increase_level(struct PlayerInfo *player, int count)
{
    struct CreatureControl *cctrl;
    struct Thing *thing;
    struct Dungeon* dungeon = get_dungeon(player->id_number);
    // Increase level of normal creatures
    unsigned long k = 0;
    int i = dungeon->creatr_list_start;
    while (i != 0)
    {
        thing = thing_get(i);
        cctrl = creature_control_get_from_thing(thing);
        if (thing_is_invalid(thing) || creature_control_invalid(cctrl))
        {
          ERRORLOG("Jump to invalid creature detected");
          break;
        }
        i = cctrl->players_next_creature_idx;
        // Thing list loop body
        if (count != 1) creature_change_multiple_levels(thing, count);
        else creature_increase_level(thing);
        // Thing list loop body ends
        k++;
        if (k > CREATURES_COUNT)
        {
          ERRORLOG("Infinite loop detected when sweeping creatures list");
          break;
        }
    }
    // Increase level of special diggers
    k = 0;
    i = dungeon->digger_list_start;
    while (i != 0)
    {
        thing = thing_get(i);
        cctrl = creature_control_get_from_thing(thing);
        if (thing_is_invalid(thing) || creature_control_invalid(cctrl))
        {
          ERRORLOG("Jump to invalid creature detected");
          break;
        }
        i = cctrl->players_next_creature_idx;
        // Thing list loop body
        if (count > 1) creature_change_multiple_levels(thing, count);
        else creature_increase_level(thing);
        // Thing list loop body ends
        k++;
        if (k > CREATURES_COUNT)
        {
          ERRORLOG("Infinite loop detected when sweeping creatures list");
          break;
        }
    }
}

TbBool steal_hero(struct PlayerInfo *player, struct Coord3d *pos)
{
    //TODO CONFIG creature models dependency; put them in config files
    static ThingModel skip_steal_models[] = {6, 7};
    static ThingModel prefer_steal_models[] = {3, 12};
    struct Thing* herotng = INVALID_THING;
    int heronum;
    int i;
    SYNCDBG(8,"Starting");
    struct Dungeon* herodngn = get_players_num_dungeon(game.hero_player_num);
    unsigned long k = 0;
    if (herodngn->num_active_creatrs > 0) {
        heronum = PLAYER_RANDOM(game.hero_player_num, herodngn->num_active_creatrs);
        i = herodngn->creatr_list_start;
        SYNCDBG(4,"Selecting random creature %d out of %d heroes",(int)heronum,(int)herodngn->num_active_creatrs);
    } else {
        heronum = 0;
        i = 0;
        SYNCDBG(4,"No heroes on map, skipping selection");
    }
    while (i != 0)
    {
        struct Thing* thing = thing_get(i);
        TRACE_THING(thing);
        struct CreatureControl* cctrl = creature_control_get_from_thing(thing);
        if (thing_is_invalid(thing) || creature_control_invalid(cctrl))
        {
            ERRORLOG("Jump to invalid creature detected");
            break;
        }
        i = cctrl->players_next_creature_idx;
        // Thing list loop body
        TbBool heroallow = true;
        for (ThingModel skipidx = 0; skipidx < sizeof(skip_steal_models) / sizeof(skip_steal_models[0]); skipidx++)
        {
            if (thing->model == skip_steal_models[skipidx]) {
                heroallow = false;
            }
        }
        if (heroallow) {
            herotng = thing;
        }
        // If we've reached requested hero number, return either current hero on previously selected one
        if ((heronum <= 0) && thing_is_creature(herotng)) {
            break;
        }
        heronum--;
        if (i == 0) {
            i = herodngn->creatr_list_start;
        }
        // Thing list loop body ends
        k++;
        if (k > CREATURES_COUNT)
        {
            ERRORLOG("Infinite loop detected when sweeping creatures list");
            break;
        }
    }
    if (!thing_is_invalid(herotng))
    {
        move_thing_in_map(herotng, pos);
        reset_interpolation_of_thing(herotng);
        change_creature_owner(herotng, player->id_number);
        SYNCDBG(3,"Converted %s to owner %d",thing_model_name(herotng),(int)player->id_number);
    }
    else
    {
        i = PLAYER_RANDOM(game.hero_player_num, sizeof(prefer_steal_models)/sizeof(prefer_steal_models[0]));
        struct Thing* creatng = create_creature(pos, prefer_steal_models[i], player->id_number);
        if (thing_is_invalid(creatng))
            return false;
        SYNCDBG(3,"Created %s owner %d",thing_model_name(creatng),(int)player->id_number);
    }
    return true;
}

void make_safe(struct PlayerInfo *player)
{
    unsigned char* areamap = (unsigned char*)big_scratch;
    MapSlabCoord slb_x;
    MapSlabCoord slb_y;
    // Prepare the array to remember which slabs were already taken care of
    for (slb_y=0; slb_y < gameadd.map_tiles_y; slb_y++)
    {
        for (slb_x=0; slb_x < gameadd.map_tiles_x; slb_x++)
        {
            SlabCodedCoords slb_num = get_slab_number(slb_x, slb_y);
            struct SlabMap* slb = get_slabmap_direct(slb_num);
            struct SlabAttr* slbattr = get_slab_attrs(slb);
            if ((slbattr->block_flags & (SlbAtFlg_Filled|SlbAtFlg_Digable|SlbAtFlg_Valuable)) != 0)
                areamap[slb_num] = 0x01;
            else
                areamap[slb_num] = 0x00;
        }
    }
    {
        const struct Coord3d* center_pos = dungeon_get_essential_pos(player->id_number);
        slb_x = subtile_slab(center_pos->x.stl.num);
        slb_y = subtile_slab(center_pos->y.stl.num);
        SlabCodedCoords slb_num = get_slab_number(slb_x, slb_y);
        areamap[slb_num] |= 0x02;
    }

    PlayerNumber plyr_idx = player->id_number;
    SlabCodedCoords* slblist = (SlabCodedCoords*)(big_scratch + gameadd.map_tiles_x * gameadd.map_tiles_y);
    unsigned int list_len = 0;
    unsigned int list_cur = 0;
    while (list_cur <= list_len)
    {
        SlabCodedCoords slb_num;

        if (slb_x > 0)
        {
            slb_num = get_slab_number(slb_x-1, slb_y);
            if ((areamap[slb_num] & 0x01) != 0)
            {
                areamap[slb_num] |= 0x02;
                struct SlabMap* slb = get_slabmap_direct(slb_num);
                struct SlabAttr* slbattr = get_slab_attrs(slb);
                if ((slbattr->category == SlbAtCtg_FriableDirt) && slab_by_players_land(plyr_idx, slb_x-1, slb_y))
                {
                    unsigned char pretty_type = choose_pretty_type(plyr_idx, slb_x - 1, slb_y);
                    place_slab_type_on_map(pretty_type, slab_subtile(slb_x-1,0), slab_subtile(slb_y,0), plyr_idx, 1);
                    do_slab_efficiency_alteration(slb_x-1, slb_y);
                    fill_in_reinforced_corners(plyr_idx, slb_x-1, slb_y);
                }
            } else
            if ((areamap[slb_num] & 0x02) == 0)
            {
                areamap[slb_num] |= 0x02;
                slblist[list_len] = slb_num;
                list_len++;
            }
        }
        if (slb_x < gameadd.map_tiles_x-1)
        {
            slb_num = get_slab_number(slb_x+1, slb_y);
            if ((areamap[slb_num] & 0x01) != 0)
            {
                areamap[slb_num] |= 0x02;
                struct SlabMap* slb = get_slabmap_direct(slb_num);
                struct SlabAttr* slbattr = get_slab_attrs(slb);
                if ((slbattr->category == SlbAtCtg_FriableDirt) &&  slab_by_players_land(plyr_idx, slb_x+1, slb_y))
                {
                    unsigned char pretty_type = choose_pretty_type(plyr_idx, slb_x + 1, slb_y);
                    place_slab_type_on_map(pretty_type, slab_subtile(slb_x+1,0), slab_subtile(slb_y,0), plyr_idx, 1u);
                    do_slab_efficiency_alteration(slb_x+1, slb_y);
                    fill_in_reinforced_corners(plyr_idx, slb_x+1, slb_y);
                }
            } else
            if ((areamap[slb_num] & 0x02) == 0)
            {
                areamap[slb_num] |= 0x02;
                slblist[list_len] = slb_num;
                list_len++;
            }
        }
        if (slb_y > 0)
        {
            slb_num = get_slab_number(slb_x, slb_y-1);
            if ((areamap[slb_num] & 0x01) != 0)
            {
                areamap[slb_num] |= 0x02;
                struct SlabMap* slb = get_slabmap_direct(slb_num);
                struct SlabAttr* slbattr = get_slab_attrs(slb);
                if ((slbattr->category == SlbAtCtg_FriableDirt) && slab_by_players_land(plyr_idx, slb_x, slb_y-1))
                {
                    unsigned char pretty_type = choose_pretty_type(plyr_idx, slb_x, slb_y - 1);
                    place_slab_type_on_map(pretty_type, slab_subtile(slb_x,0), slab_subtile(slb_y-1,0), plyr_idx, 1);
                    do_slab_efficiency_alteration(slb_x, slb_y-1);
                    fill_in_reinforced_corners(plyr_idx, slb_x, slb_y-1);
                }
            } else
            if ((areamap[slb_num] & 0x02) == 0)
            {
                areamap[slb_num] |= 0x02;
                slblist[list_len] = slb_num;
                list_len++;
            }
        }
        if (slb_y < gameadd.map_tiles_y-1)
        {
            slb_num = get_slab_number(slb_x, slb_y+1);
            if ((areamap[slb_num] & 0x01) != 0)
            {
                areamap[slb_num] |= 0x02;
                struct SlabMap* slb = get_slabmap_direct(slb_num);
                struct SlabAttr* slbattr = get_slab_attrs(slb);
                if ((slbattr->category == SlbAtCtg_FriableDirt) && slab_by_players_land(plyr_idx, slb_x, slb_y+1))
                {
                    unsigned char pretty_type = choose_pretty_type(plyr_idx, slb_x, slb_y + 1);
                    place_slab_type_on_map(pretty_type, slab_subtile(slb_x,0), slab_subtile(slb_y+1,0), plyr_idx, 1);
                    do_slab_efficiency_alteration(slb_x, slb_y+1);
                    fill_in_reinforced_corners(plyr_idx, slb_x, slb_y+1);
                }
            } else
            if ((areamap[slb_num] & 0x02) == 0)
            {
                areamap[slb_num] |= 0x02;
                slblist[list_len] = slb_num;
                list_len++;
            }
        }

        slb_x = slb_num_decode_x(slblist[list_cur]);
        slb_y = slb_num_decode_y(slblist[list_cur]);
        list_cur++;
    }
    panel_map_update(0, 0, gameadd.map_subtiles_x+1, gameadd.map_subtiles_y+1);
}

void make_unsafe(struct PlayerInfo *player)
{
    MapSlabCoord slb_x;
    MapSlabCoord slb_y;
    SlabCodedCoords slb_num;
    struct SlabMap* slb;
    struct SlabAttr* slbattr;
    struct PowerConfigStats* powerst;
    struct Dungeon* dungeon;
    struct Coord3d pos;
    for (slb_y = 0; slb_y < gameadd.map_tiles_y; slb_y++)
    {
        for (slb_x = 0; slb_x < gameadd.map_tiles_x; slb_x++)
        {
            slb_num = get_slab_number(slb_x, slb_y);
            slb = get_slabmap_direct(slb_num);
            if (slabmap_owner(slb) == player)
            {
                slbattr = get_slab_attrs(slb);
                if ((slbattr->category == SlbAtCtg_FortifiedWall))
                {
                    SlabKind newslab = choose_rock_type(player, slb_x, slb_y);
                    dungeon = get_dungeon(player);
                    dungeon->camera_deviate_jump = dungeon->camera_deviate_jump + 3; //Bigger jump on more slabs changed
                    dungeon->camera_deviate_quake = 30; //30 frames of camera shaking

                    set_coords_to_slab_center(&pos, slb_x, slb_y);
                    powerst = get_power_model_stats(PwrK_DESTRWALLS);
                    play_sound_if_close_to_receiver(&pos, powerst->select_sound_idx);
                    place_slab_type_on_map(newslab, slab_subtile_center(slb_x), slab_subtile_center(slb_y), game.neutral_player_num, 0);
                    do_slab_efficiency_alteration(slb_x, slb_y);
                }
            }
        }
    }
    panel_map_update(0, 0, gameadd.map_subtiles_x + 1, gameadd.map_subtiles_y + 1);
}

void activate_dungeon_special(struct Thing *cratetng, struct PlayerInfo *player)
{
    SYNCDBG(6,"Starting");
    struct Coord3d pos;

    // Gathering data which we'll need if the special is used and disposed.
    struct Dungeon* dungeon = get_dungeon(player->id_number);
    memcpy(&pos,&cratetng->mappos,sizeof(struct Coord3d));
    SpecialKind spkindidx = box_thing_to_special(cratetng);
    struct SpecialConfigStats* specst = get_special_model_stats(spkindidx);
    short used = 0;
    TbBool no_speech = false;
    if (thing_exists(cratetng) && thing_is_special_box(cratetng))
    {
    switch (spkindidx)
    {
        case SpcKind_RevealMap:
            reveal_whole_map(player);
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            break;
        case SpcKind_Resurrect:
            start_resurrect_creature(player, cratetng);
            break;
        case SpcKind_TrnsfrCrtr:
            start_transfer_creature(player, cratetng);
            break;
        case SpcKind_StealHero:
            if (steal_hero(player, &cratetng->mappos))
            {
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            }
            break;
        case SpcKind_MultplCrtr:
            multiply_creatures(player);
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            break;
        case SpcKind_IncrseLvl:
            increase_level(player, 1);
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            break;
        case SpcKind_MakeSafe:
            make_safe(player);
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            break;
        case SpcKind_MakeUnsafe:
            for (long i = 0; i < PLAYERS_COUNT; i++)
            {
                if (players_are_enemies(player->id_number, i))
                {
                    make_unsafe(i);
                }
            }
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            break;
        case SpcKind_HiddnWorld:
            activate_bonus_level(player);
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            break;
        case SpcKind_HealAll:
            do_to_players_all_creatures_of_model(player->id_number, CREATURE_ANY, set_creature_health_to_max_with_heal_effect);
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            break;
        case SpcKind_GetGold:
            throw_out_gold(cratetng, specst->value);
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            break;
        case SpcKind_MakeAngry:
            for (long i = 0; i < PLAYERS_COUNT; i++)
            {
                if (players_are_enemies(player->id_number, i))
                {
                    add_anger_to_all_creatures_of_player(i, specst->value);
                }
            }
            remove_events_thing_is_attached_to(cratetng);
            used = 1;
            delete_thing_structure(cratetng, 0);
            break;
        case SpcKind_Custom:
        default:
            if (thing_is_custom_special_box(cratetng))
            {
                if (gameadd.current_player_turn == game.play_gameturn)
                {
                    WARNLOG("box activation rejected turn:%d", gameadd.current_player_turn);
                    // If two players suddenly activated box at same turn it is not that we want to
                    return;
                }
                gameadd.current_player_turn = game.play_gameturn;
                gameadd.script_current_player = player->id_number;
                memcpy(&gameadd.triggered_object_location, &pos, sizeof(struct Coord3d));
                dungeon->box_info.activated[cratetng->custom_box.box_kind]++;
                no_speech = true;
                remove_events_thing_is_attached_to(cratetng);
                used = 1;
                delete_thing_structure(cratetng, 0);
                break;
            }
            else
            {
                ERRORLOG("Invalid dungeon special (Model %d)", (int)cratetng->model);
            }
            break;
        }
        if ( used )
        {
            if (is_my_player(player) && !no_speech)
            {
                output_message(specst->speech, 0, true);
            }
            create_used_effect_or_element(&pos, specst->effect_id, player->id_number);
        }
    }
}

void resurrect_creature(struct Thing *boxtng, PlayerNumber owner, ThingModel crmodel, unsigned char crlevel)
{
    if (!thing_exists(boxtng) || (box_thing_to_special(boxtng) != SpcKind_Resurrect) ) {
        ERRORMSG("Invalid resurrect box object!");
        return;
    }
    struct Thing* creatng = create_creature(&boxtng->mappos, crmodel, owner);
    if (!thing_is_invalid(creatng))
    {
        init_creature_level(creatng, crlevel);
        if (is_my_player_number(owner))
          output_message(SMsg_CommonAcknowledge, 0, true);
    }
    struct SpecialConfigStats* specst = get_special_model_stats(SpcKind_Resurrect);
    create_used_effect_or_element(&boxtng->mappos, specst->effect_id, owner);
    remove_events_thing_is_attached_to(boxtng);
    force_any_creature_dragging_owned_thing_to_drop_it(boxtng);
    if ((game.conf.rules.game.classic_bugs_flags & ClscBug_ResurrectForever) == 0) {
        remove_item_from_dead_creature_list(get_players_num_dungeon(owner), crmodel, crlevel);
    }
    delete_thing_structure(boxtng, 0);
}

void transfer_creature(struct Thing *boxtng, struct Thing *transftng, unsigned char plyr_idx)
{
    SYNCDBG(7, "Starting");
    TbBool from_script = false;
    struct Dungeon* dungeon = get_players_num_dungeon(plyr_idx);
    if (dungeon->dnheart_idx == boxtng->index)
    {
        from_script = true;
    }

    if (!from_script)
    {
        if (!thing_exists(boxtng) || (box_thing_to_special(boxtng) != SpcKind_TrnsfrCrtr)) {
            ERRORMSG("Invalid transfer box object!");
            return;
        }
    }
    // Check if 'things' are correct
    if (!thing_exists(transftng) || !thing_is_creature(transftng) || (transftng->owner != plyr_idx)) {
        ERRORMSG("Invalid transfer creature thing!");
        return;
    }

    struct CreatureControl* cctrl = creature_control_get_from_thing(transftng);
    if (add_transfered_creature(plyr_idx, transftng->model, cctrl->explevel,cctrl->creature_name))
    {
        dungeon->creatures_transferred++;
    }
    remove_thing_from_power_hand_list(transftng, plyr_idx);
    kill_creature(transftng, INVALID_THING, -1, CrDed_NoEffects|CrDed_NotReallyDying);
    if (!from_script)
    {
        struct SpecialConfigStats* specst = get_special_model_stats(SpcKind_TrnsfrCrtr);
        create_used_effect_or_element(&boxtng->mappos, specst->effect_id, plyr_idx);
        remove_events_thing_is_attached_to(boxtng);
        force_any_creature_dragging_owned_thing_to_drop_it(boxtng);
        delete_thing_structure(boxtng, 0);
    }
    if (is_my_player_number(plyr_idx))
      output_message(SMsg_CommonAcknowledge, 0, true);
}

void start_transfer_creature(struct PlayerInfo *player, struct Thing *thing)
{
    struct Dungeon* dungeon = get_dungeon(player->id_number);
    if (dungeon->num_active_creatrs != 0)
    {
        if (is_my_player(player))
        {
            dungeon_special_selected = thing->index;
            transfer_creature_scroll_offset = 0;
            output_message(SMsg_SpecTransfer, MESSAGE_DELAY_SPECIAL, true);
            turn_off_menu(GMnu_DUNGEON_SPECIAL);
            turn_on_menu(GMnu_TRANSFER_CREATURE);
        }
  }
}

void start_resurrect_creature(struct PlayerInfo *player, struct Thing *thing)
{
    struct Dungeon* dungeon = get_dungeon(player->id_number);
    if (dungeon->dead_creatures_count != 0)
    {
        if (is_my_player(player))
        {
          dungeon_special_selected = thing->index;
          resurrect_creature_scroll_offset = 0;
          output_message(SMsg_SpecResurrect, MESSAGE_DELAY_SPECIAL, true);
          turn_off_menu(GMnu_DUNGEON_SPECIAL);
          turn_on_menu(GMnu_RESURRECT_CREATURE);
        }
    }
}

long create_transferred_creatures_on_level(void)
{
    struct Thing* creatng;
    struct Thing* srcetng;
    struct CreatureControl* cctrl;
    long creature_created = 0;
    PlayerNumber plyr_idx;
    for (int p = 0; p < PLAYERS_COUNT; p++)
    {
        plyr_idx = game.neutral_player_num;
        for (int i = 0; i < TRANSFER_CREATURE_STORAGE_COUNT; i++)
        {
            if (intralvl.transferred_creatures[p][i].model > 0)
            {
                srcetng = get_player_soul_container(p);
                if (p == game.hero_player_num)
                {
                    plyr_idx = p;
                    if (thing_is_invalid(srcetng))
                    {
                        for (long n = 1; n < 16; n++)
                        {
                            srcetng = find_hero_gate_of_number(n);
                            if (!thing_is_invalid(srcetng))
                                break;
                        }
                    }
                }

                struct Coord3d* pos = &(srcetng->mappos);
                creatng = create_creature(pos, intralvl.transferred_creatures[p][i].model, plyr_idx);
                if (thing_is_invalid(creatng))
                {
                    continue;
                }
                init_creature_level(creatng, intralvl.transferred_creatures[p][i].explevel);
                cctrl = creature_control_get_from_thing(creatng);
                strcpy(cctrl->creature_name, intralvl.transferred_creatures[p][i].creature_name);
                creature_created++;
            }
        }
    }
    clear_transfered_creatures();
    return creature_created;
}

SpecialKind box_thing_to_special(const struct Thing *thing)
{
    if (thing_is_invalid(thing))
        return 0;
    if ( (thing->class_id != TCls_Object) || (thing->model >= game.conf.object_conf.object_types_count) )
        return 0;
    return game.conf.object_conf.object_to_special_artifact[thing->model];
}
/******************************************************************************/
