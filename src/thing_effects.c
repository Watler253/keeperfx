/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file thing_effects.c
 *     Effect generators and effect elements support functions.
 * @par Purpose:
 *     Functions to create and maintain effect generators and single effect elements.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     01 Jan 2010 - 01 Feb 2012
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "pre_inc.h"
#include "thing_effects.h"
#include "globals.h"

#include "bflib_memory.h"
#include "bflib_math.h"
#include "bflib_sound.h"
#include "bflib_planar.h"

#include "thing_objects.h"
#include "thing_list.h"
#include "thing_stats.h"
#include "thing_physics.h"
#include "thing_factory.h"
#include "thing_navigate.h"
#include "thing_shots.h"
#include "creature_battle.h"
#include "creature_senses.h"
#include "config_creature.h"
#include "config_effects.h"
#include "front_simple.h"
#include "map_data.h"
#include "map_blocks.h"
#include "creature_graphics.h"
#include "gui_topmsg.h"
#include "game_legacy.h"
#include "engine_redraw.h"
#include "keeperfx.hpp"
#include "gui_soundmsgs.h"
#include "room_util.h"
#include "post_inc.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

long const bounce_table[] = { -160, -160, -120, -120, -80, -40, -20, 0, 20, 40, 80, 120, 120, 160, 160, 160 };
/** Effects used when creating new imps. Every player color has different index. */
const int birth_effect_element[] = { TngEffElm_RedPuff, TngEffElm_BluePuff, TngEffElm_GreenPuff, TngEffElm_YellowPuff, TngEffElm_WhitePuff, TngEffElm_WhitePuff,
                                     TngEffElm_PurplePuff,TngEffElm_BlackPuff,TngEffElm_OrangePuff };
/******************************************************************************/
TbBool thing_is_effect(const struct Thing *thing)
{
  if (thing_is_invalid(thing))
    return false;
  if (thing->class_id != TCls_Effect)
    return false;
  return true;
}

struct EffectElementConfigStats *get_effect_element_model_stats(ThingModel tngmodel)
{
    if (tngmodel >= EFFECTSELLEMENTS_TYPES_MAX)
        return &game.conf.effects_conf.effectelement_cfgstats[0];
    return &game.conf.effects_conf.effectelement_cfgstats[tngmodel];
}

struct Thing *create_effect_element(const struct Coord3d *pos, unsigned short eelmodel, PlayerNumber owner)
{
    long i;
    if (!i_can_allocate_free_thing_structure(FTAF_Default)) {
        return INVALID_THING;
    }
    if (!any_player_close_enough_to_see(pos)) {
        return INVALID_THING;
    }
    struct EffectElementConfigStats* eestat = get_effect_element_model_stats(eelmodel);
    struct InitLight ilght;
    LbMemorySet(&ilght, 0, sizeof(struct InitLight));
    struct Thing* thing = allocate_free_thing_structure(FTAF_Default);
    if (thing->index == 0) {
        ERRORDBG(8,"Should be able to allocate effect element %d for player %d, but failed.",(int)eelmodel,(int)owner);
        return INVALID_THING;
    }
    thing->class_id = TCls_EffectElem;
    thing->model = eelmodel;
    thing->mappos.x.val = pos->x.val;
    thing->mappos.y.val = pos->y.val;
    thing->mappos.z.val = pos->z.val;
    thing->next_on_mapblk = 0;
    thing->parent_idx = thing->index;
    thing->owner = owner;
    thing->clipbox_size_xy = 1;
    thing->clipbox_size_z = 1;
    thing->solid_size_xy = 1;
    thing->solid_size_z = 1;

    if (eestat->sprite_idx != -1)
    {
        i = EFFECT_RANDOM(thing, eestat->sprite_size_max  - (int)eestat->sprite_size_min  + 1);
        long n = EFFECT_RANDOM(thing, eestat->sprite_speed_max - (int)eestat->sprite_speed_min + 1);
        set_thing_draw(thing, eestat->sprite_idx, eestat->sprite_speed_min + n, eestat->sprite_size_min + i, 0, 0, eestat->draw_class);
        set_flag_value(thing->rendering_flags, TRF_Unshaded, eestat->unshaded);
        thing->rendering_flags ^= (thing->rendering_flags ^ (TRF_Transpar_8 * eestat->transparant)) & (TRF_Transpar_Flags);
        set_flag_value(thing->rendering_flags, TRF_AnimateOnce, eestat->rendering_flag);
    } else
    {
        set_flag(thing->rendering_flags, TRF_Unknown01);
    }

    thing->fall_acceleration = eestat->fall_acceleration;
    thing->inertia_floor = eestat->inertia_floor;
    thing->inertia_air = eestat->inertia_air;
    thing->movement_flags |= TMvF_Unknown08;
    set_flag_value(thing->movement_flags, TMvF_Unknown10, eestat->movement_flags);
    thing->creation_turn = game.play_gameturn;

    if (eestat->lifespan > 0)
    {
        i = EFFECT_RANDOM(thing, eestat->lifespan_random - (long)eestat->lifespan + 1);
        thing->health = eestat->lifespan + i;
    } else
    {
        if (thing->anim_speed > 0)
        {
            thing->health = get_lifespan_of_animation(thing->anim_sprite, thing->anim_speed);
        }
        else
        {
            thing->health = keepersprite_frames(thing->anim_sprite);
        }
    }

    if (eestat->size_change != TSC_DontChangeSize)
    {
        thing->sprite_size_min = eestat->sprite_size_min;
        thing->sprite_size_max = eestat->sprite_size_max;
        if (eestat->size_change == TSC_ChangeSizeContinuously)
        {
            thing->transformation_speed = 2 * (eestat->sprite_size_max - (long)eestat->sprite_size_min) / thing->health;
            thing->size_change |= TSC_ChangeSizeContinuously;
        }
        else
        {
            thing->transformation_speed = (eestat->sprite_size_max - (long)eestat->sprite_size_min) / thing->health;
            thing->size_change &= ~TSC_ChangeSizeContinuously;
        }
        thing->sprite_size = eestat->sprite_size_min;
    } else
    {
        thing->transformation_speed = 0;
    }

    if (eestat->light_radius != 0)
    {
        ilght.mappos.x.val = thing->mappos.x.val;
        ilght.mappos.y.val = thing->mappos.y.val;
        ilght.mappos.z.val = thing->mappos.z.val;
        ilght.radius = eestat->light_radius;
        ilght.intensity = eestat->light_intensity;
        ilght.is_dynamic = 1;
        ilght.flags = eestat->light_flags;
        thing->light_id = light_create_light(&ilght);
        if (thing->light_id <= 0) {
            SYNCDBG(8,"Cannot allocate dynamic light to %s.",thing_model_name(thing));
        }
    }
    add_thing_to_its_class_list(thing);
    place_thing_in_mapwho(thing);
    return thing;
}

void process_spells_affected_by_effect_elements(struct Thing *thing)
{
    struct CreatureControl* cctrl = creature_control_get_from_thing(thing);
    GameTurnDelta dturn;
    long angle;
    MapCoordDelta shift_x;
    MapCoordDelta shift_y;
    struct Coord3d pos;
    struct Thing *effeltng;

    if ((cctrl->spell_flags & CSAfF_Rebound) != 0)
    {
        int diamtr = 4 * thing->clipbox_size_xy / 2;
        dturn = game.play_gameturn - thing->creation_turn;
        MapCoord cor_z_max = thing->clipbox_size_z + (thing->clipbox_size_z * game.conf.crtr_conf.exp.size_increase_on_exp * cctrl->explevel) / 80; //effect is 25% larger than unit

        struct EffectElementConfigStats* eestat = get_effect_element_model_stats(TngEffElm_FlashBall1);
        unsigned short nframes = keepersprite_frames(eestat->sprite_idx);
        GameTurnDelta dtadd = 0;
        unsigned short cframe = game.play_gameturn % nframes;
        pos.z.val = thing->mappos.z.val;
        int radius = diamtr / 2;
        while (pos.z.val < cor_z_max + thing->mappos.z.val)
        {
            angle = (abs(dturn + dtadd) & 7) << 8;
            shift_x =  (radius * LbSinL(angle) >> 8) >> 8;
            shift_y = -(radius * LbCosL(angle) >> 8) >> 8;
            pos.x.val = thing->mappos.x.val + shift_x;
            pos.y.val = thing->mappos.y.val + shift_y;
            effeltng = create_thing(&pos, TCls_EffectElem, TngEffElm_FlashBall1, thing->owner, -1);
            if (thing_is_invalid(effeltng))
                break;
            set_thing_draw(effeltng, eestat->sprite_idx, 256, eestat->sprite_size_min, 0, cframe, ODC_Default);
            dtadd++;
            pos.z.val += 64;
            cframe = (cframe + 1) % nframes;
        }
    }

    if ((cctrl->spell_flags & CSAfF_Slow) != 0)
    {
        int diamtr = 4 * thing->clipbox_size_xy / 2;
        MapCoord cor_z_max = thing->clipbox_size_z + (thing->clipbox_size_z * game.conf.crtr_conf.exp.size_increase_on_exp * cctrl->explevel) / 80; //effect is 20% smaller than unit
        int i = cor_z_max / 64; //64 is the vertical speed of the circle.
        if (i <= 1)
          i = 1;
        dturn = game.play_gameturn - thing->creation_turn;
        int vrange = i;
        if (dturn % (2 * i) < vrange)
            pos.z.val = thing->mappos.z.val + cor_z_max / vrange * (dturn % vrange);
        else
            pos.z.val = thing->mappos.z.val + cor_z_max / vrange * (vrange - (dturn % vrange));
        int radius = diamtr / 2;
        for (i=0; i < 16; i++)
        {
            angle = (abs(i) & 0xF) << 7;
            shift_x =  (radius * LbSinL(angle) >> 8) >> 8;
            shift_y = -(radius * LbCosL(angle) >> 8) >> 8;
            pos.x.val = thing->mappos.x.val + shift_x;
            pos.y.val = thing->mappos.y.val + shift_y;
            effeltng = create_thing(&pos, TCls_EffectElem, TngEffElm_RedFlash, thing->owner, -1);
        }
    }

    if ((cctrl->spell_flags & CSAfF_Flying) != 0)
    {
        effeltng = create_thing(&thing->mappos, TCls_EffectElem, TngEffElm_CloudDisperse, thing->owner, -1);
    }

    if ((cctrl->spell_flags & CSAfF_Speed) != 0)
    {
        effeltng = create_effect_element(&thing->mappos, TngEffElm_FlashBall2, thing->owner);
        if (!thing_is_invalid(effeltng))
        {
            //make an afterimage of the speeding unit
            effeltng->anim_time = thing->anim_time;
            effeltng->anim_sprite = thing->anim_sprite;
            effeltng->sprite_size = thing->sprite_size;
            effeltng->current_frame = thing->current_frame;
            effeltng->max_frames = thing->max_frames;
            effeltng->transformation_speed = thing->transformation_speed;
            effeltng->sprite_size_min = thing->sprite_size_min;
            effeltng->sprite_size_max = thing->sprite_size_max;
            effeltng->rendering_flags = thing->rendering_flags;
            effeltng->rendering_flags &= ~TRF_Transpar_8;
            effeltng->rendering_flags |= TRF_Transpar_4;
            effeltng->size_change = thing->size_change;
            effeltng->draw_class = thing->draw_class;
            effeltng->tint_colour = thing->tint_colour;
            effeltng->anim_speed = 0;
            effeltng->move_angle_xy = thing->move_angle_xy;
        }
    }

    if ((cctrl->stateblock_flags & CCSpl_Teleport) != 0)
    {
        dturn = get_spell_duration_left_on_thing(thing, SplK_Teleport);
        const struct SpellConfig* spconf = get_spell_config(SplK_Teleport);
        if (spconf->duration / 2 < dturn)
        {
            effeltng = create_effect_element(&thing->mappos, TngEffElm_FlashBall2, thing->owner);
            if (!thing_is_invalid(effeltng))
            {
                //make an afterimage of the teleporting unit
                effeltng->anim_speed = 0;
                effeltng->anim_time = thing->anim_time;
                effeltng->anim_sprite = thing->anim_sprite;
                effeltng->sprite_size = thing->sprite_size;
                effeltng->current_frame = thing->current_frame;
                effeltng->max_frames = thing->max_frames;
                effeltng->transformation_speed = thing->transformation_speed;
                effeltng->sprite_size_min = thing->sprite_size_min;
                effeltng->sprite_size_max = thing->sprite_size_max;
                effeltng->rendering_flags = thing->rendering_flags;
                effeltng->rendering_flags &= ~TRF_Transpar_8;
                effeltng->rendering_flags |= TRF_Transpar_4;
                effeltng->size_change = thing->size_change;
                effeltng->draw_class = thing->draw_class;
                effeltng->tint_colour = thing->tint_colour;
                effeltng->rendering_flags &= ~TRF_Transpar_8;
                effeltng->rendering_flags |= TRF_Transpar_4;
                effeltng->move_angle_xy = thing->move_angle_xy;
            }
        } else
        if (spconf->duration / 2 > dturn)
        {
            struct CreatureStats* crstat = creature_stats_get_from_thing(thing);
            if ((dturn % 2) == 0) {
                effeltng = create_effect_element(&thing->mappos, birth_effect_element[get_player_color_idx(thing->owner)], thing->owner);
            }
            creature_turn_to_face_angle(thing, thing->move_angle_xy + crstat->max_turning_speed);
        }
    }

    if ((cctrl->spell_flags & CSAfF_MagicFall) != 0)
    {
        dturn = game.play_gameturn - thing->creation_turn;
        if ((dturn & 1) == 0) {
            effeltng = create_effect_element(&thing->mappos, birth_effect_element[get_player_color_idx(thing->owner)], thing->owner);
        }
        struct CreatureStats* crstat = creature_stats_get_from_thing(thing);
        creature_turn_to_face_angle(thing, thing->move_angle_xy + crstat->max_turning_speed);
        if ((dturn > 32) || thing_touching_floor(thing)) {
            cctrl->spell_flags &= ~CSAfF_MagicFall;
        }
    }

    if ((cctrl->spell_flags & CSAfF_Rage) != 0)
    {
        pos.x.val = thing->mappos.x.val;
        pos.y.val = thing->mappos.y.val;
        pos.z.val = thing->mappos.z.val + get_creature_eye_height(thing);
        effeltng = create_thing(&pos, TCls_EffectElem, TngEffElm_RedDot, thing->owner, -1);
    }

    if ((cctrl->spell_flags & CSAfF_DivineShield) != 0)
    {
        int diamtr = 3 * thing->clipbox_size_xy / 2;
        MapCoord cor_z_max = thing->clipbox_size_z + (thing->clipbox_size_z * game.conf.crtr_conf.exp.size_increase_on_exp * cctrl->explevel) / 80; //effect is 20% smaller than unit
        int i = cor_z_max / 16; //128 is the vertical speed of the circle.
        if (i <= 1)
          i = 1;
        dturn = game.play_gameturn - thing->creation_turn;
        int vrange = i;
        if (dturn % (2 * i) < vrange)
            pos.z.val = thing->mappos.z.val + cor_z_max / vrange * (dturn % vrange);
        else
            pos.z.val = thing->mappos.z.val + cor_z_max / vrange * (vrange - (dturn % vrange));
        int radius = diamtr / 2;
        for (i=0; i < 16; i++)
        {
            angle = (abs(i) & 0xF) << 7;
            shift_x =  (radius * LbSinL(angle) >> 8) >> 8;
            shift_y = -(radius * LbCosL(angle) >> 8) >> 8;
            pos.x.val = thing->mappos.x.val + shift_x;
            pos.y.val = thing->mappos.y.val + shift_y;
            effeltng = create_thing(&pos, TCls_EffectElem, TngEffElm_LavaFlameMoving, thing->owner, -1);
        }
    }

    if ((cctrl->spell_flags & CSAfF_MadKilling) != 0)
    {
        pos.x.val = thing->mappos.x.val;
        pos.y.val = thing->mappos.y.val;
        pos.z.val = thing->mappos.z.val;
        effeltng = create_thing(&pos, TCls_EffectElem, TngEffElm_BloodSplat, thing->owner, -1);
    }

    if ((cctrl->spell_flags & CSAfF_MagicMist) != 0)
    {
        int diamtr = thing->clipbox_size_xy;
        dturn = game.play_gameturn - thing->creation_turn;
        MapCoord cor_z_max = thing->clipbox_size_z + (thing->clipbox_size_z * game.conf.crtr_conf.exp.size_increase_on_exp * cctrl->explevel);

        struct EffectElementConfigStats* eestat = get_effect_element_model_stats(TngEffElm_CloudDisperse);
        unsigned short nframes = keepersprite_frames(eestat->sprite_idx);
        GameTurnDelta dtadd = 0;
        unsigned short cframe = game.play_gameturn % nframes;
        pos.z.val = thing->mappos.z.val;
        int radius = diamtr / 2;
        while (pos.z.val < cor_z_max + thing->mappos.z.val)
        {
            angle = (abs(dturn + dtadd) & 7) << 8;
            shift_x =  (radius * LbSinL(angle) >> 8) >> 8;
            shift_y = -(radius * LbCosL(angle) >> 8) >> 8;
            pos.x.val = thing->mappos.x.val + shift_x;
            pos.y.val = thing->mappos.y.val + shift_y;
            effeltng = create_thing(&pos, TCls_EffectElem, TngEffElm_CloudDisperse, thing->owner, -1);
            if (thing_is_invalid(effeltng))
                break;
            set_thing_draw(effeltng, eestat->sprite_idx, 256, eestat->sprite_size_min, 0, cframe, ODC_Default);
            dtadd++;
            pos.z.val += 16;
            cframe = (cframe + 1) % nframes;
        }
    }
}

void move_effect_blocked(struct Thing *thing, struct Coord3d *prev_pos, struct Coord3d *next_pos)
{
    struct EffectElementConfigStats* eestat = get_effect_element_model_stats(thing->model);
    long blocked_flags = get_thing_blocked_flags_at(thing, next_pos);
    slide_thing_against_wall_at(thing, next_pos, blocked_flags);
    if ( ((blocked_flags & SlbBloF_WalledZ) != 0) && eestat->movable && eestat->impacts )
    {
        struct Thing* efftng = thing;
        long cube_id = get_top_cube_at(next_pos->x.stl.num, next_pos->y.stl.num, NULL);
        unsigned short effmodel;
        if (cube_is_water(cube_id))
        {
          effmodel = eestat->water_effmodel;
          if (effmodel > 0) {
              efftng = create_effect(prev_pos, effmodel, thing->owner);
              TRACE_THING(efftng);
          }
          long sample_id = eestat->water_snd_smpid;
          if (sample_id > 0) {
              thing_play_sample(efftng, sample_id, NORMAL_PITCH, 0, 3, 0, 2, eestat->water_loudness);
          }
          if ( eestat->water_destroy_on_impact )
              thing->health = 0;
        } else
        if (cube_is_lava(cube_id))
        {
            effmodel = eestat->lava_effmodel;
            if (effmodel > 0) {
                efftng = create_effect(prev_pos, effmodel, thing->owner);
                TRACE_THING(efftng);
            }
            long sample_id = eestat->lava_snd_smpid;
            if (sample_id > 0) {
                thing_play_sample(efftng, sample_id, NORMAL_PITCH, 0, 3, 0, 2, eestat->lava_loudness);
            }
            if ( eestat->lava_destroy_on_impact )
                thing->health = 0;
        } else
        {
            effmodel = eestat->solidgnd_effmodel;
            if (effmodel > 0) {
                efftng = create_effect(prev_pos, effmodel, thing->owner);
                TRACE_THING(efftng);
            }
            long sample_id = eestat->solidgnd_snd_smpid;
            if (sample_id > 0) {
                thing_play_sample(efftng, sample_id, NORMAL_PITCH, 0, 3, 0, 2, eestat->solidgnd_loudness);
            }
            if ( eestat->solidgnd_destroy_on_impact )
                thing->health = 0;
        }
    }
    remove_relevant_forces_from_thing_after_slide(thing, next_pos, blocked_flags);
}

TngUpdateRet move_effect_element(struct Thing *thing)
{
    SYNCDBG(18,"Starting");
    TRACE_THING(thing);
    struct Coord3d pos;
    TbBool move_allowed = get_thing_next_position(&pos, thing);
    if ( positions_equivalent(&thing->mappos, &pos) ) {
        return TUFRet_Unchanged;
    }
    if ((thing->movement_flags & TMvF_Unknown10) == 0)
    {
        if (!move_allowed)
        {
            move_effect_blocked(thing, &thing->mappos, &pos);
        } else
        if ( !thing_covers_same_blocks_in_two_positions(thing, &thing->mappos, &pos) )
        {
            if ( thing_in_wall_at(thing, &pos) )
            {
                move_effect_blocked(thing, &thing->mappos, &pos);
            }
        }
    }
    move_thing_in_map(thing, &pos);
    return TUFRet_Modified;
}

void change_effect_element_into_another(struct Thing *thing, long nmodel)
{
    SYNCDBG(18,"Starting");
    struct EffectElementConfigStats* eestat = get_effect_element_model_stats(nmodel);
    int speed = eestat->sprite_speed_min + EFFECT_RANDOM(thing, eestat->sprite_speed_max - eestat->sprite_speed_min + 1);
    int scale = eestat->sprite_size_min + EFFECT_RANDOM(thing, eestat->sprite_size_max - eestat->sprite_size_min + 1);
    thing->model = nmodel;
    set_thing_draw(thing, eestat->sprite_idx, speed, scale, eestat->rendering_flag, 0, ODC_Default);
    thing->rendering_flags ^= (thing->rendering_flags ^ TRF_Unshaded * eestat->unshaded) & TRF_Unshaded;
    thing->rendering_flags ^= (thing->rendering_flags ^ TRF_Transpar_8 * eestat->transparant) & (TRF_Transpar_Flags);
    thing->fall_acceleration = eestat->fall_acceleration;
    thing->inertia_floor = eestat->inertia_floor;
    thing->inertia_air = eestat->inertia_air;
    if (eestat->lifespan <= 0) {
        thing->health = get_lifespan_of_animation(thing->anim_sprite, thing->anim_speed);
    } else {
        thing->health = EFFECT_RANDOM(thing, eestat->lifespan_random - eestat->lifespan + 1) + eestat->lifespan;
    }
    thing->max_frames = keepersprite_frames(thing->anim_sprite);
}

TngUpdateRet update_effect_element(struct Thing *elemtng)
{
    long i;
    SYNCDBG(18,"Starting");
    TRACE_THING(elemtng);
    struct EffectElementConfigStats* eestats = get_effect_element_model_stats(elemtng->model);
    // Check if effect health dropped to zero; delete it, or decrease health for the next check
    long health = elemtng->health;
    if (health <= 0)
    {
        if (eestats->transform_model != 0)
        {
            change_effect_element_into_another(elemtng, eestats->transform_model);
        } else
        {
            delete_thing_structure(elemtng, 0);
        }
        return TUFRet_Deleted;
    }
    elemtng->health = health-1;
    // Set dynamic properties of the effect
    if (!eestats->animate_on_floor)
    {
        if (elemtng->floor_height >= (int)elemtng->mappos.z.val)
          elemtng->anim_speed = 0;
    }
    if (eestats->movable)
    {
        elemtng->movement_flags &= ~TMvF_IsOnWater;
        elemtng->movement_flags &= ~TMvF_IsOnLava;
        if (thing_touching_floor(elemtng))
        {
            i = get_top_cube_at(elemtng->mappos.x.stl.num, elemtng->mappos.y.stl.num, NULL);
            if (cube_is_water(i)) {
                elemtng->movement_flags |= TMvF_IsOnWater;
            } else
            if (cube_is_lava(i)) {
                elemtng->movement_flags |= TMvF_IsOnLava;
            }
        }
    }
    i = eestats->subeffect_delay;
    if (i > 0)
    {
      if (((elemtng->creation_turn - game.play_gameturn) % i) == 0) {
          create_effect_element(&elemtng->mappos, eestats->subeffect_model, elemtng->owner);
      }
    }
    switch (eestats->move_type)
    {
    case 1:
        move_effect_element(elemtng);
        break;
    case 2:
        i = elemtng->veloc_base.x.val;
        elemtng->veloc_base.x.val = 2*i/3;
        i = elemtng->veloc_base.y.val;
        elemtng->veloc_base.y.val = 2*i/3;
        i = elemtng->veloc_base.z.val;
        if (i > 32)
        {
          elemtng->veloc_base.z.val = 2*i/3;
        } else
        if (i > 16)
        {
          i = i-16;
          if (i < 16) i = 16;
          elemtng->veloc_base.z.val = i;
        } else
        if (i < -16)
        {
          elemtng->veloc_base.z.val = 2*i/3;
        } else
        {
            i = i+16;
            if (i > 16) i = 16;
            elemtng->veloc_base.z.val = i;
        }
        move_effect_element(elemtng);
        break;
    case 3:
        elemtng->veloc_base.z.val = 32;
        move_effect_element(elemtng);
        break;
    case 4:
        health = elemtng->health;
        if ((health >= 0) && (health < 16))
        {
            elemtng->veloc_base.z.val = bounce_table[health];
        } else
        {
            ERRORLOG("Illegal effect element bounce life: %d", (int)health);
        }
        move_effect_element(elemtng);
        break;
    case 5:
        break;
    default:
        ERRORLOG("Invalid effect element move type %d!",(int)eestats->move_type);
        JUSTLOG("elemtng->model %d",elemtng->model);
        move_effect_element(elemtng);
        break;
    }

    if (eestats->unanimated != 1)
      return TUFRet_Modified;
    i = get_angle_yz_to_vec(&elemtng->veloc_base);
    if (i > LbFPMath_PI)
      i -= LbFPMath_PI;
    long prop_val = i / (LbFPMath_PI / 8);
    elemtng->move_angle_xy = get_angle_xy_to_vec(&elemtng->veloc_base);
    elemtng->current_frame = prop_val;
    elemtng->anim_speed = 0;
    elemtng->anim_time = (prop_val & 0xff) << 8;
    SYNCDBG(18,"Finished");
    return TUFRet_Modified;
}

struct Thing *create_effect_generator(struct Coord3d *pos, unsigned short model, unsigned short range, unsigned short owner, long parent_idx)
{

    if (!i_can_allocate_free_thing_structure(FTAF_FreeEffectIfNoSlots))
    {
        ERRORDBG(3,"Cannot create effect generator model %d for player %d. There are too many things allocated.",(int)model,(int)owner);
        erstat_inc(ESE_NoFreeThings);
        return INVALID_THING;
    }
    struct Thing* effgentng = allocate_free_thing_structure(FTAF_FreeEffectIfNoSlots);
    if (effgentng->index == 0) {
        ERRORDBG(3,"Should be able to allocate effect generator %d for player %d, but failed.",(int)model,(int)owner);
        erstat_inc(ESE_NoFreeThings);
        return INVALID_THING;
    }

    effgentng->class_id = TCls_EffectGen;
    effgentng->model = model;
    effgentng->parent_idx = parent_idx;
    if ( parent_idx == -1 )
      effgentng->parent_idx = -1;
    effgentng->owner = owner;
    effgentng->effect_generator.range = range;
    effgentng->mappos = *pos;
    effgentng->creation_turn = game.play_gameturn;
    effgentng->health = -1;
    effgentng->rendering_flags |= TRF_Unknown01;
    add_thing_to_its_class_list(effgentng);
    place_thing_in_mapwho(effgentng);
    return effgentng;

}

long move_effect(struct Thing *efftng)
{
    int blocked_flags;
    struct Coord3d pos;

    MapCoordDelta velocity_x = efftng->velocity.x.val;
    MapCoordDelta velocity_y = efftng->velocity.y.val;
    MapCoordDelta velocity_z = efftng->velocity.z.val;

    if ( efftng->velocity.x.val
       || efftng->velocity.y.val
       || efftng->velocity.z.val )
    {
        if ( velocity_x >= -256 )
        {
            if ( velocity_x > 256 )
                velocity_x = 256;
        }
        else
        {
            velocity_x = -256;
        }
        if (velocity_y >= -256 )
        {
            if (velocity_y > 256 )
                velocity_y = 256;
        }
        else
        {
            velocity_y = -256;
        }
        if (velocity_z >= -256 )
        {
            if ( velocity_z > 256 )
                velocity_z = 256;
        }
        else
        {
            velocity_z = -256;
        }
        pos.x.val = velocity_x + efftng->mappos.x.val;
        pos.y.val = velocity_y + efftng->mappos.y.val;
        pos.z.val = velocity_z + efftng->mappos.z.val;
        if ( !positions_equivalent(&efftng->mappos, &pos) && thing_in_wall_at(efftng, &pos) )
        {
            blocked_flags = get_creature_blocked_flags_at(efftng, &pos);
            slide_thing_against_wall_at(efftng, &pos, blocked_flags);
            remove_relevant_forces_from_thing_after_slide(efftng, &pos, blocked_flags);
        }
        if ( efftng->mappos.x.stl.num == pos.x.stl.num && efftng->mappos.y.stl.num == pos.y.stl.num )
        {
            efftng->mappos.x.val = pos.x.val;
            efftng->mappos.y.val = pos.y.val;
            efftng->mappos.z.val = pos.z.val;
        }
        else
        {
            remove_thing_from_mapwho(efftng);
            efftng->mappos.x.val = pos.x.val;
            efftng->mappos.y.val = pos.y.val;
            efftng->mappos.z.val = pos.z.val;
            place_thing_in_mapwho(efftng);
        }
        efftng->floor_height = get_thing_height_at(efftng, &efftng->mappos);
    }
    return 1;
}

TbBool effect_can_affect_thing(struct Thing *efftng, struct Thing *thing)
{
    if (thing_is_invalid(efftng) || thing_is_invalid(thing))
    {
        WARNLOG("Invalid thing tries to interact with other things");
        return false;
    }
    if (thing->index == efftng->index)
    {
        SYNCDBG(18,"Effect tried to shoot itself; suicide not implemented");
        return false;
    }
    if (thing->index == efftng->parent_idx)
    {
        SYNCDBG(18,"Effect tried to shoot its maker; suicide not implemented");
        return false;
    }
    HitTargetFlags hit_targets = hit_type_to_hit_targets(efftng->shot_effect.hit_type);
    return area_effect_can_affect_thing(thing, hit_targets, efftng->owner);
}

void update_effect_light_intensity(struct Thing *thing)
{
  if (thing->light_id != 0)
  {
      if (thing->health < 4)
      {
          long i = light_get_light_intensity(thing->light_id);
          light_set_light_intensity(thing->light_id, (3*i)/4);
      }
  }
}

void effect_generate_effect_elements(const struct Thing *thing)
{
    const struct EffectConfigStats* effcst = get_effect_model_stats(thing->model);
    SYNCDBG(18,"Preparing Effect, Generation Type %d",(int)effcst->generation_type);
    unsigned long arg;
    struct Thing* elemtng;
    switch (effcst->generation_type)
    {
    case 1:
    {
        unsigned long argZ;
        for (long i = 0; i < effcst->elements_count; i++)
        {
            if (effcst->kind_min <= 0)
                continue;
            long n = effcst->kind_min + EFFECT_RANDOM(thing, effcst->kind_max - effcst->kind_min + 1);
            elemtng = create_effect_element(&thing->mappos, n, thing->owner);
            TRACE_THING(elemtng);
            if (thing_is_invalid(elemtng))
                break;
            arg = EFFECT_RANDOM(thing, 0x800);
            argZ = EFFECT_RANDOM(thing, 0x400);
            // Setting XY acceleration
            long k = abs(effcst->accel_xy_max - effcst->accel_xy_min);
            if (k <= 1) k = 1;
            long mag = effcst->accel_xy_min + EFFECT_RANDOM(thing, k);
            elemtng->veloc_push_add.x.val += distance_with_angle_to_coord_x(mag,arg);
            elemtng->veloc_push_add.y.val += distance_with_angle_to_coord_y(mag,arg);
            // Setting Z acceleration
            k = abs(effcst->accel_z_max - effcst->accel_z_min);
            if (k <= 1) k = 1;
            mag = effcst->accel_z_min + EFFECT_RANDOM(thing, k);
            elemtng->veloc_push_add.z.val += distance_with_angle_to_coord_z(mag,argZ);
            elemtng->state_flags |= TF1_PushAdd;
        }
        break;
    }
    case 2:
    {
        long k = 0;
        struct Coord3d pos;
        for (long i=0; i < effcst->elements_count; i++)
        {
            long n = effcst->kind_min + EFFECT_RANDOM(thing, effcst->kind_max - effcst->kind_min + 1);
            long mag = effcst->start_health - thing->health;
            arg = (mag << 7) + k/effcst->elements_count;
            set_coords_to_cylindric_shift(&pos, &thing->mappos, mag, arg, 0);
            elemtng = create_effect_element(&pos, n, thing->owner);
            TRACE_THING(elemtng);
            SYNCDBG(18,"Created %s",thing_model_name(elemtng));
            k += 2048;
        }
        break;
    }
    case 3:
    {
        long k = 0;
        struct Coord3d pos;
        for (long i=0; i < effcst->elements_count; i++)
        {
            long n = effcst->kind_min + EFFECT_RANDOM(thing, effcst->kind_max - effcst->kind_min + 1);
            long mag = thing->health;
            arg = (mag << 7) + k/effcst->elements_count;
            set_coords_to_cylindric_shift(&pos, &thing->mappos, 16*mag, arg, 0);
            elemtng = create_effect_element(&pos, n, thing->owner);
            TRACE_THING(elemtng);
            k += 2048;
        }
        break;
    }
    case 4: // Lightning or CaveIn
    {
        if (thing->model != 48) // CaveIn only
            break;
        long i = effcst->start_health / 2;
        struct PlayerInfo* player;
        if (thing->health == effcst->start_health)
        {
            LbMemorySet(temp_pal, 63, PALETTE_SIZE);
        } else
        if (thing->health > i)
        {
          LbPaletteFade(temp_pal, i, Lb_PALETTE_FADE_OPEN);
        } else
        if (thing->health == i)
        {
          LbPaletteStopOpenFade();
          LbPaletteSet(temp_pal);
        } else
        if (thing->health > 0)
        {
            LbPaletteFade(engine_palette, 8, Lb_PALETTE_FADE_OPEN);
        } else
        {
            player = get_my_player();
            PaletteSetPlayerPalette(player, engine_palette);
            LbPaletteStopOpenFade();
        }
        break;
    }
    default:
        ERRORLOG("Unknown Effect Generation Type %d",(int)effcst->generation_type);
        break;
    }
}

TngUpdateRet process_effect_generator(struct Thing *thing)
{
    SYNCDBG(18,"Starting");
    TRACE_THING(thing);
    if (thing->health > 0)
        thing->health--;
    if (thing->health == 0)
    {
        delete_thing_structure(thing, 0);
        return TUFRet_Deleted;
    }
    if ( !any_player_close_enough_to_see(&thing->mappos) )
    {
        SYNCDBG(18,"No player sees %s at (%d,%d,%d)",thing_model_name(thing),(int)thing->mappos.x.stl.num,(int)thing->mappos.y.stl.num,(int)thing->mappos.z.stl.num);
        return TUFRet_Modified;
    }
    if (thing->effect_generator.generation_delay > 0)
        thing->effect_generator.generation_delay--;
    if (thing->effect_generator.generation_delay > 0)
    {
        return TUFRet_Modified;
    }
    struct EffectGeneratorConfigStats* egenstat = get_effectgenerator_model_stats(thing->model);
    for (long i = 0; i < egenstat->generation_amount; i++)
    {
        long deviation_angle = EFFECT_RANDOM(thing, 0x800);
        long deviation_mag = EFFECT_RANDOM(thing, thing->effect_generator.range + 1);
        struct Coord3d pos;
        set_coords_to_cylindric_shift(&pos, &thing->mappos, deviation_mag, deviation_angle, 0);
        SYNCDBG(18,"The %s creates effect %d/%d at (%d,%d,%d)",thing_model_name(thing),(int)pos.x.val,(int)pos.y.val,(int)pos.z.val);
        struct Thing* elemtng = create_used_effect_or_element(&pos, egenstat->effect_model , thing->owner);
        TRACE_THING(elemtng);
        if (thing_is_invalid(elemtng))
            break;
        elemtng->clipbox_size_xy = 20;
        elemtng->clipbox_size_z = 20;
        long k;
        if (egenstat->ignore_terrain)
        {
            k = egenstat->spawn_height;
        } else
        if (egenstat->spawn_height == -1)
        {
            elemtng->mappos.z.val = subtile_coord(8,0);
            k = get_next_gap_creature_can_fit_in_below_point(elemtng, &elemtng->mappos);
        } else
        {
            k = egenstat->spawn_height + get_thing_height_at(elemtng, &elemtng->mappos);
        }
        elemtng->mappos.z.val = k;
        if ( thing_in_wall_at(elemtng, &elemtng->mappos) )
        {
            SYNCDBG(18,"The %s created effect %d/%d in wall, removing",thing_model_name(thing),(int)i,(int)egenstat->generation_amount);
            delete_thing_structure(elemtng, 0);
        } else
        {
            SYNCDBG(18,"The %s created effect %d/%d, index %d",thing_model_name(thing),(int)i,(int)egenstat->generation_amount,(int)elemtng->index);
            long acc_x = egenstat->acc_x_min + EFFECT_RANDOM(thing, egenstat->acc_x_max - egenstat->acc_x_min + 1);
            long acc_y = egenstat->acc_y_min + EFFECT_RANDOM(thing, egenstat->acc_y_max - egenstat->acc_y_min + 1);
            long acc_z = egenstat->acc_z_min + EFFECT_RANDOM(thing, egenstat->acc_z_max - egenstat->acc_z_min + 1);
            elemtng->veloc_push_add.x.val += acc_x;
            elemtng->veloc_push_add.y.val += acc_y;
            elemtng->veloc_push_add.z.val += acc_z;
            elemtng->state_flags |= TF1_PushAdd;
            if (egenstat->sound_sample_idx > 0)
            {
                struct Thing* sectng = create_effect(&elemtng->mappos, TngEff_DamageBlood, thing->owner);
                TRACE_THING(sectng);
                if (!thing_is_invalid(sectng)) {
                    thing_play_sample(sectng, egenstat->sound_sample_idx + EFFECT_RANDOM(thing, egenstat->sound_sample_rng), NORMAL_PITCH, 0, 3, 0, 2, FULL_LOUDNESS);
                }
            }
        }
    }
    thing->effect_generator.generation_delay = egenstat->generation_delay_min + EFFECT_RANDOM(thing, egenstat->generation_delay_max - egenstat->generation_delay_min + 1);
    return TUFRet_Modified;
}

struct Thing *create_effect(const struct Coord3d *pos, ThingModel effmodel, PlayerNumber owner)
{
    struct EffectConfigStats* effcst = get_effect_model_stats(effmodel);
    if (!i_can_allocate_free_thing_structure(FTAF_FreeEffectIfNoSlots)) {
        return INVALID_THING;
    }
    struct Thing* thing = allocate_free_thing_structure(1);
    if (thing->index == 0) {
        ERRORDBG(8,"Should be able to allocate effect %d (%s) for player %d, but failed.",(int)effmodel,effect_code_name(effmodel),(int)owner);
        return INVALID_THING;
    }
    thing->creation_turn = game.play_gameturn;
    thing->class_id = TCls_Effect;
    thing->model = effmodel;
    thing->mappos.x.val = pos->x.val;
    thing->mappos.y.val = pos->y.val;
    thing->mappos.z.val = pos->z.val;
    thing->next_on_mapblk = 0;
    thing->owner = owner;
    thing->parent_idx = thing->index;
    thing->fall_acceleration = 0;
    thing->inertia_floor = 0;
    thing->inertia_air = 0;
    thing->rendering_flags |= TRF_Unknown01;
    thing->health = effcst->start_health;
    if (effcst->ilght.radius != 0)
    {
        struct InitLight ilght;
        memcpy(&ilght, &effcst->ilght, sizeof(struct InitLight));
        ilght.is_dynamic = 1;
        ilght.mappos.x.val = thing->mappos.x.val;
        ilght.mappos.y.val = thing->mappos.y.val;
        ilght.mappos.z.val = thing->mappos.z.val;
        thing->light_id = light_create_light(&ilght);
        if (thing->light_id == 0) {
            // Note that there's an error here in original DK, and it makes unusable Thing entries if cannot allocate light.
            SYNCDBG(8,"Cannot allocate dynamic light to %s.",thing_model_name(thing));
        }
    }
    add_thing_to_its_class_list(thing);
    place_thing_in_mapwho(thing);
    if (effcst->effect_sound != 0)
    {
        thing_play_sample(thing, effcst->effect_sound, NORMAL_PITCH, 0, 3, 0, 3, FULL_LOUDNESS);
    }
    return thing;
}

struct Thing *create_used_effect_or_element(const struct Coord3d *pos, EffectOrEffElModel effect, PlayerNumber plyr_idx)
{
    if (effect == 0)
        return INVALID_THING;
     
    struct Thing* efftng;
    if (effect > 0)
    {
        efftng = create_effect(pos, effect, plyr_idx);
    }
    else
    {
        efftng = create_effect_element(pos, ~(effect) + 1, plyr_idx);
    }
    TRACE_THING(efftng);
    return efftng;
}

TbBool destroy_effect_thing(struct Thing *efftng)
{
    if (efftng->model == TngEff_Eruption)
    {
        MapSubtlCoord stl_x = efftng->mappos.x.stl.num;
        MapSubtlCoord stl_y = efftng->mappos.y.stl.num;
        struct SlabMap* slb = get_slabmap_block(subtile_slab(stl_x), subtile_slab(stl_y));
        if (slab_kind_is_indestructible(slb->kind) == false)
        {
            if (subtile_is_room(stl_x, stl_y))
            {
                delete_room_slab(subtile_slab(stl_x), subtile_slab(stl_y), true);
            }
            neutralise_enemy_block(efftng->mappos.x.stl.num, efftng->mappos.y.stl.num, efftng->owner);
            if (slb->kind == SlbT_PATH) //Do not turn water into lava
            {
                place_slab_type_on_map(SlbT_LAVA, efftng->mappos.x.stl.num, efftng->mappos.y.stl.num, game.neutral_player_num, 0);
            }
        }
    }
    if (efftng->snd_emitter_id != 0)
    {
        // In case of effect, don't stop any sound samples which are still playing
        S3DDestroySoundEmitter(efftng->snd_emitter_id);
        efftng->snd_emitter_id = 0;
    }
    delete_thing_structure(efftng, 0);
    return true;
}

/**
 * Affects a thing with explosion effect.
 *
 * @param tngsrc The thing which caused the affect.
 * @param tngdst The thing being affected by the effect.
 * @param pos Position of the effect epicenter.
 * @param max_dist Max distance at which creatures are affected, in map coordinates.
 * @param max_damage Damage at epicenter of the explosion.
 * @param blow_strength The strength of hitwave blowing creatures out of affected area.
 * @param damage_type Type of the damage inflicted.
 * @param owner The owner of the explosion.
 * @return Gives true if the target thing was affected by the spell, false otherwise.
 * @note If the function returns true, the effect might have caused death of the target.
 */
TbBool explosion_affecting_thing(struct Thing *tngsrc, struct Thing *tngdst, const struct Coord3d *pos,
    MapCoordDelta max_dist, HitPoints max_damage, long blow_strength, DamageType damage_type, PlayerNumber owner, unsigned long shot_model_flags)
{
    if (thing_is_deployed_door(tngdst))
    {
        return explosion_affecting_door(tngsrc, tngdst, pos, max_dist, max_damage, blow_strength, damage_type, owner);
    }
    TbBool affected = false;
    SYNCDBG(17,"Starting for %s, max damage %d, max blow %d, owner %d",thing_model_name(tngdst),(int)max_damage,(int)blow_strength,(int)owner);
    if (nowibble_line_of_sight_3d(pos, &tngdst->mappos))
    {
        // Friendly fire usually causes less damage and at smaller distance
        if ((tngdst->class_id == TCls_Creature) && (tngdst->owner == owner)) {
            max_dist = max_dist * game.conf.rules.magic.friendly_fight_area_range_permil / 1000;
            max_damage = max_damage * game.conf.rules.magic.friendly_fight_area_damage_permil / 1000;
        }
        MapCoordDelta distance = get_2d_distance(pos, &tngdst->mappos);
        if (distance <= max_dist)
        {
            if (tngdst->class_id == TCls_Creature)
            {
                HitPoints damage = get_radially_decaying_value(max_damage, max_dist / 4, 3 * max_dist / 4, distance) + 1;
                SYNCDBG(7,"Causing %d damage to %s at distance %d",(int)damage,thing_model_name(tngdst),(int)distance);
                apply_damage_to_thing_and_display_health(tngdst, damage, damage_type, owner);
                affected = true;
                if (tngdst->health < 0)
                {
                    struct Thing *origtng = thing_get(tngsrc->parent_idx); //parent of the tngsrc(shot) is the shooting creature.
                    struct CreatureBattle* battle = creature_battle_get_from_thing(origtng);
                    CrDeathFlags dieflags = (!creature_battle_invalid(battle)) ? CrDed_DiedInBattle : CrDed_Default;
                    // Explosions kill rather than only stun friendly creatures when imprison is on
                    if (((tngsrc->owner == tngdst->owner) &! (game.conf.rules.game.classic_bugs_flags & ClscBug_FriendlyFaint)) || (shot_model_flags & ShMF_NoStun) )
                    {
                        dieflags |= CrDed_NoUnconscious;
                    }
                    if ((shot_model_flags & ShMF_BlocksRebirth))
                    {
                        dieflags |= CrDed_NoRebirth;
                    }
                    kill_creature(tngdst, origtng, -1, dieflags);
                    affected = true;
                }
            }
            if (thing_is_dungeon_heart(tngdst))
            {
                HitPoints damage = get_radially_decaying_value(max_damage, max_dist / 4, 3 * max_dist / 4, distance) + 1;
                SYNCDBG(7,"Causing %d damage to %s at distance %d",(int)damage,thing_model_name(tngdst),(int)distance);
                apply_damage_to_thing(tngdst, damage, damage_type, -1);
                affected = true;
                event_create_event_or_update_nearby_existing_event(tngdst->mappos.x.val, tngdst->mappos.y.val,EvKind_HeartAttacked, tngdst->owner, 0);
                if (is_my_player_number(tngdst->owner))
                {
                    output_message(SMsg_HeartUnderAttack, 400, true);
                }
            } else // Explosions move creatures and other things
            {
                long move_angle = get_angle_xy_to(pos, &tngdst->mappos);
                long move_dist = 0;
                if (blow_strength > 0)
                {
                    move_dist = get_radially_decaying_value(blow_strength, max_dist / 4, max_dist * 3 / 4, distance);
                }
                else
                {
                    move_dist = get_radially_growing_value(blow_strength, max_dist / 4, max_dist * 3 / 4, distance, tngdst->inertia_floor);
                }
                if (move_dist != 0)
                {
                    tngdst->veloc_push_add.x.val += distance_with_angle_to_coord_x(move_dist, move_angle);
                    tngdst->veloc_push_add.y.val += distance_with_angle_to_coord_y(move_dist, move_angle);
                    tngdst->state_flags |= TF1_PushAdd;
                    affected = true;
                }
            }
        }
    }
    return affected;
}

TbBool explosion_affecting_door(struct Thing *tngsrc, struct Thing *tngdst, const struct Coord3d *pos,
    MapCoordDelta max_dist, HitPoints max_damage, long blow_strength, DamageType damage_type, PlayerNumber owner)
{
    TbBool affected = false;
    SYNCDBG(17,"Starting for %s, max damage %d, max blow %d, owner %d",thing_model_name(tngdst),(int)max_damage,(int)blow_strength,(int)owner);
    if (tngdst->class_id != TCls_Door)
    {
        ERRORLOG("%s is trying to damage %s which is not a door",thing_model_name(tngsrc),thing_model_name(tngdst));
        return false;
    }
    if (line_of_sight_3d_ignoring_specific_door(&tngsrc->mappos, &tngdst->mappos,tngdst))
    {
        MapCoordDelta distance = get_2d_distance(pos, &tngdst->mappos);
        if (distance < max_dist)
        {
            HitPoints damage = get_radially_decaying_value(max_damage, max_dist / 4, 3 * max_dist / 4, distance) + 1;
            SYNCDBG(7,"Causing %d damage to %s at distance %d",(int)damage,thing_model_name(tngdst),(int)distance);
            apply_damage_to_thing(tngdst, damage, damage_type, -1);
            affected = true;
        }
    }
    return affected;
}

/**
 * Computes and applies damage the effect associated to a spell makes to things at given map block.
 * @param efftng The effect thing which represents the spell.
 * @param tngsrc The thing being source of the spell.
 * @param mapblk Map block on which all targets are to be affected by the spell.
 * @param max_dist Range of the spell on map, used to compute damage decaying with distance; in map coordinates.
 * @param max_damage Damage at epicenter of the explosion.
 * @param blow_strength The strength of hitwave blowing creatures out of affected area.
 * @param damage_type Type of the damage inflicted.
 */
long explosion_effect_affecting_map_block(struct Thing *efftng, struct Thing *tngsrc, struct Map *mapblk,
    MapCoordDelta max_dist, HitPoints max_damage, long blow_strength, DamageType damage_type, unsigned long shot_model_flags)
{
    PlayerNumber owner;
    if (!thing_is_invalid(tngsrc))
        owner = tngsrc->owner;
    else
        owner = -1;
    long num_affected = 0;
    unsigned long k = 0;
    long i = get_mapwho_thing_index(mapblk);
    while (i != 0)
    {
        struct Thing* thing = thing_get(i);
        TRACE_THING(thing);
        if (thing_is_invalid(thing))
        {
            ERRORLOG("Jump to invalid thing detected");
            break;
        }
        i = thing->next_on_mapblk;
        // Per thing processing block
        if ((thing->class_id == TCls_Door) && (efftng->shot_effect.hit_type != THit_CrtrsOnlyNotOwn)) //TODO: Find pretty way to say that WoP traps should not destroy doors. And make it configurable through configs.
        {
            if (explosion_affecting_door(tngsrc, thing, &efftng->mappos, max_dist, max_damage, blow_strength, damage_type, owner))
            {
                num_affected++;
            }
        } else
        if (effect_can_affect_thing(efftng, thing))
        {
            if (explosion_affecting_thing(tngsrc, thing, &efftng->mappos, max_dist, max_damage, blow_strength, damage_type, owner, shot_model_flags))
            {
                num_affected++;
            }
        }
        // Per thing processing block ends
        k++;
        if (k > THINGS_COUNT)
        {
            ERRORLOG("Infinite loop detected when sweeping things list");
            break_mapwho_infinite_chain(mapblk);
            break;
        }
    }
    return num_affected;
}

/**
 * Applies damage the Word Of Power spell makes to all things in the area surrounding given position.
 * @param efftng The thing which is WOP effect originator.
 * @param tngsrc The thing being source of the spell.
 * @param pos Position where the WOP effect center is.
 * @param max_dist Range of the WOP spell effect, in map coordinates.
 */
void word_of_power_affecting_area(struct Thing *efftng, struct Thing *tngsrc, struct Coord3d *pos)
{
    long stl_xmin;
    long stl_xmax;
    long stl_ymin;
    long stl_ymax;
    // Effect causes area damage only on its birth turn
    if (efftng->creation_turn != game.play_gameturn) {
        return;
    }
    struct ShotConfigStats* shotst;
    if (thing_is_deployed_trap(tngsrc))
    {
        shotst = get_shot_model_stats(ShM_TrapWordOfPower);
    }
    else
    {
        shotst = get_shot_model_stats(ShM_WordOfPower);
    }
    if ((shotst->area_range <= 0) || ((shotst->area_damage == 0) && (shotst->area_blow == 0))) {
        ERRORLOG("Word of power shot configuration does not include area influence.");
        return;
    }
    SYNCDBG(8,"Starting for %s index %d owner %d",thing_model_name(efftng),(int)efftng->index,(int)efftng->owner);
    MapCoordDelta max_dist = shotst->area_range * COORD_PER_STL;
    {
        // Make sure the subtile is rounded up, unless the range is really close to lower value
        long stl_range = coord_subtile(max_dist + COORD_PER_STL * 9 / 10);
        // Position on subtile is not at its start, so add 1 to max values while ignoring the position
        stl_xmin = pos->x.stl.num - stl_range;
        stl_xmax = pos->x.stl.num + stl_range + 1;
        stl_ymin = pos->y.stl.num - stl_range;
        stl_ymax = pos->y.stl.num + stl_range + 1;
    }
    if (stl_xmin < 0) {
        stl_xmin = 0;
    } else
    if (stl_xmin > gameadd.map_subtiles_x) {
        stl_xmin = gameadd.map_subtiles_x;
    }
    if (stl_ymin < 0) {
      stl_ymin = 0;
    } else
    if (stl_ymin > gameadd.map_subtiles_y) {
      stl_ymin = gameadd.map_subtiles_y;
    }
    if (stl_xmax < 0) {
      stl_xmax = 0;
    } else
    if (stl_xmax > gameadd.map_subtiles_x) {
      stl_xmax = gameadd.map_subtiles_x;
    }
    if (stl_ymax < 0) {
      stl_ymax = 0;
    } else
    if (stl_ymax > gameadd.map_subtiles_y) {
      stl_ymax = gameadd.map_subtiles_y;
    }
    for (long stl_y = stl_ymin; stl_y <= stl_ymax; stl_y++)
    {
        for (long stl_x = stl_xmin; stl_x <= stl_xmax; stl_x++)
        {
            struct Map* mapblk = get_map_block_at(stl_x, stl_y);
            explosion_effect_affecting_map_block(efftng, tngsrc, mapblk, max_dist,
                shotst->area_damage, shotst->area_blow, shotst->damage_type, shotst->model_flags);
        }
    }
}

/**
 * Determines if an explosion or other area effect, of given hit thing type and owner, can affect given thing.
 * Explosions can affect a lot more things than shots. If only the thing isn't invalid,
 * it is by default affected by explosions.
 */
TbBool area_effect_can_affect_thing(const struct Thing *thing, HitTargetFlags hit_targets, PlayerNumber shot_owner)
{
    if (thing_is_invalid(thing))
    {
        WARNLOG("Invalid thing tries to interact with explosion");
        return false;
    }
    return thing_is_shootable(thing, shot_owner, hit_targets);
}

/**
 * Affects things on a map blocks with explosion effect, if only they should be affected with given hit type.
 *
 * @param tngsrc The thing which caused the affect.
 * @param mapblk Map blocks on which are creatures should be affected.
 * @param pos Position of the effect epicenter.
 * @param max_dist Max distance at which creatures are affected, in map coordinates.
 * @param max_damage Damage at epicenter of the explosion.
 * @param blow_strength The strength of hitwave blowing creatures out of affected area.
 * @param hit_targets Defines which things are affected.
 * @param damage_type Type of the damage inflicted.
 */
long explosion_affecting_map_block(struct Thing *tngsrc, const struct Map *mapblk, const struct Coord3d *pos,
    MapCoord max_dist, HitPoints max_damage, long blow_strength, HitTargetFlags hit_targets, DamageType damage_type)
{
    PlayerNumber owner;
    if (!thing_is_invalid(tngsrc))
        owner = tngsrc->owner;
    else
        owner = -1;
    long num_affected = 0;
    unsigned long k = 0;
    long i = get_mapwho_thing_index(mapblk);
    while (i != 0)
    {
        struct Thing* thing = thing_get(i);
        TRACE_THING(thing);
        if (thing_is_invalid(thing))
        {
            WARNLOG("Jump out of things array");
            break;
        }
        i = thing->next_on_mapblk;
        // Should never happen - only existing thing shall be in list
        if (!thing_exists(thing))
        {
            WARNLOG("Jump to non-existing thing");
            break;
        }
        // Per thing processing block
        if (area_effect_can_affect_thing(thing, hit_targets, owner))
        {
            struct ShotConfigStats* shotst = get_shot_model_stats(tngsrc->model);
            if (explosion_affecting_thing(tngsrc, thing, pos, max_dist, max_damage, blow_strength, damage_type, owner, shotst->model_flags))
                num_affected++;
        }
        // Per thing processing block ends
        k++;
        if (k > THINGS_COUNT)
        {
            ERRORLOG("Infinite loop detected when sweeping things list");
            break_mapwho_infinite_chain(mapblk);
            break;
        }
    }
    return num_affected;
}

/**
 * Affects things on an area with explosion effect, if only they should be affected with given hit type.
 *
 * @param tngsrc The thing which caused the effect, usually spell caster.
 * @param pos Position of the effect epicenter.
 * @param max_dist Range of the effect.
 * @param max_damage Damage at epicenter of the effect.
 * @param blow_strength The strength of hitwave blowing creatures out of affected area.
 * @param hit_targets Defines which things are affected.
 * @return Gives number of things which were affected by the explosion.
 */
long explosion_affecting_area(struct Thing *tngsrc, const struct Coord3d *pos, MapCoord max_dist,
    HitPoints max_damage, long blow_strength, HitTargetFlags hit_targets, DamageType damage_type)
{
    MapSubtlCoord start_x;
    MapSubtlCoord start_y;
    if (hit_targets == HitTF_None)
    {
        ERRORLOG("The %s tries to affect area up to distance %d with invalid hit type %d",thing_model_name(tngsrc),(int)max_dist,(int)hit_targets);
        return 0;
    }
    MapSubtlCoord range_stl = (max_dist + 5 * COORD_PER_STL / 6) / COORD_PER_STL;
    if (pos->x.stl.num > range_stl)
      start_x = pos->x.stl.num - range_stl;
    else
      start_x = 0;
    if (pos->y.stl.num > range_stl)
      start_y = pos->y.stl.num - range_stl;
    else
      start_y = 0;
    MapSubtlCoord end_x = range_stl + pos->x.stl.num;
    if (end_x >= gameadd.map_subtiles_x)
      end_x = gameadd.map_subtiles_x;
    MapSubtlCoord end_y = range_stl + pos->y.stl.num;
    if (end_y > gameadd.map_subtiles_y)
      end_y = gameadd.map_subtiles_y;
#if (BFDEBUG_LEVEL > 0)
    if ((start_params.debug_flags & DFlg_ShotsDamage) != 0)
        create_price_effect(pos, my_player_number, max_damage);
#endif
    long num_affected = 0;
    for (MapSubtlCoord stl_y = start_y; stl_y <= end_y; stl_y++)
    {
        for (MapSubtlCoord stl_x = start_x; stl_x <= end_x; stl_x++)
        {
            const struct Map* mapblk = get_map_block_at(stl_x, stl_y);
            num_affected += explosion_affecting_map_block(tngsrc, mapblk, pos, max_dist, max_damage, blow_strength, hit_targets, damage_type);
        }
    }
    return num_affected;
}

TbBool poison_cloud_affecting_thing(struct Thing *tngsrc, struct Thing *tngdst, const struct Coord3d *pos,
    MapCoordDelta max_dist, HitPoints max_damage, long blow_strength, unsigned char area_affect_type, DamageType damage_type, PlayerNumber owner)
{
    TbBool affected = false;
    SYNCDBG(17,"Starting for %s, max damage %d, max blow %d, owner %d",thing_model_name(tngdst),(int)max_damage,(int)blow_strength,(int)owner);
    if (thing_is_creature(tngdst))
    {
        const struct CreatureStats* crstat = creature_stats_get_from_thing(tngdst);
        if (crstat->immune_to_gas) {
            return affected;
        }
    } else {
        return affected;
    }
    if (line_of_sight_3d(pos, &tngdst->mappos))
    {
        // Note that gas has no distinction over friendly and enemy fire
        MapCoordDelta distance = get_2d_distance(pos, &tngdst->mappos);
        if (distance < max_dist)
        {
            struct CreatureControl* cctrl = creature_control_get_from_thing(tngdst);
            cctrl->spell_flags |= CSAfF_PoisonCloud;
            switch (area_affect_type)
            {
            case AAffT_GasDamage:
                if (max_damage > 0) {
                    HitPoints damage;
                    damage = get_radially_decaying_value(max_damage,3*max_dist/4,max_dist/4,distance)+1;
                    SYNCDBG(7,"Causing %d damage to %s at distance %d",(int)damage,thing_model_name(tngdst),(int)distance);
                    apply_damage_to_thing_and_display_health(tngdst, damage, damage_type, tngsrc->owner);
                }
                break;
            case AAffT_GasSlow:
                if (!creature_affected_by_spell(tngdst,SplK_Slow)) {
                    struct CreatureControl *srcctrl;
                    srcctrl = creature_control_get_from_thing(tngsrc);
                    apply_spell_effect_to_thing(tngdst, SplK_Slow, srcctrl->explevel);
                }
                break;
            case AAffT_GasSlowDamage:
                if (max_damage > 0) {
                    HitPoints damage;
                    damage = get_radially_decaying_value(max_damage, 3 * max_dist / 4, max_dist / 4, distance) + 1;
                    SYNCDBG(7, "Causing %d damage to %s at distance %d", (int)damage, thing_model_name(tngdst), (int)distance);
                    apply_damage_to_thing_and_display_health(tngdst, damage, damage_type, tngsrc->owner);
                }
                if (!creature_affected_by_spell(tngdst, SplK_Slow)) {
                    struct CreatureControl* srcctrl;
                    srcctrl = creature_control_get_from_thing(tngsrc);
                    apply_spell_effect_to_thing(tngdst, SplK_Slow, srcctrl->explevel);
                }
                break;
            case AAffT_GasDisease:
                if (!creature_affected_by_spell(tngdst, SplK_Disease)) {
                    struct CreatureControl* srcctrl;
                    srcctrl = creature_control_get_from_thing(tngsrc);
                    apply_spell_effect_to_thing(tngdst, SplK_Disease, srcctrl->explevel);
                }
            }
            affected = true;
        }
    }
    return affected;
}

long poison_cloud_affecting_map_block(struct Thing *tngsrc, const struct Map *mapblk, const struct Coord3d *pos,
    MapCoord max_dist, HitPoints max_damage, long blow_strength, HitTargetFlags hit_targets, unsigned char area_affect_type, DamageType damage_type)
{
    PlayerNumber owner;
    if (!thing_is_invalid(tngsrc))
        owner = tngsrc->owner;
    else
        owner = -1;
    long num_affected = 0;
    unsigned long k = 0;
    long i = get_mapwho_thing_index(mapblk);
    while (i != 0)
    {
        struct Thing* thing = thing_get(i);
        TRACE_THING(thing);
        if (thing_is_invalid(thing))
        {
            WARNLOG("Jump out of things array");
            break;
        }
        i = thing->next_on_mapblk;
        // Should never happen - only existing thing shall be in list
        if (!thing_exists(thing))
        {
            WARNLOG("Jump to non-existing thing");
            break;
        }
        // Per thing processing block
        if (area_effect_can_affect_thing(thing, hit_targets, owner))
        {
            if (poison_cloud_affecting_thing(tngsrc, thing, pos, max_dist, max_damage, blow_strength, area_affect_type, damage_type, owner))
                num_affected++;
        }
        // Per thing processing block ends
        k++;
        if (k > THINGS_COUNT)
        {
            ERRORLOG("Infinite loop detected when sweeping things list");
            break_mapwho_infinite_chain(mapblk);
            break;
        }
    }
    return num_affected;
}

long poison_cloud_affecting_area(struct Thing *tngsrc, struct Coord3d *pos, long max_dist, long max_damage, unsigned char area_affect_type)
{
    int dmg_divider = 10;
    if (thing_is_effect(tngsrc)) {
        const struct EffectConfigStats* effcst = get_effect_model_stats(tngsrc->model);
        dmg_divider = max(effcst->start_health,1);
    }
    MapSubtlCoord start_x = coord_subtile(pos->x.val - max_dist);
    MapSubtlCoord start_y = coord_subtile(pos->y.val - max_dist);
    MapSubtlCoord end_x = coord_subtile(pos->x.val + max_dist) + 1;
    MapSubtlCoord end_y = coord_subtile(pos->y.val + max_dist) + 1;
    if (start_x < 0) {
        start_x = 0;
    } else
    if (start_x > gameadd.map_subtiles_x) {
        start_x = gameadd.map_subtiles_x;
    }
    if (start_y < 0) {
        start_y = 0;
    } else
    if (start_y > gameadd.map_subtiles_y) {
        start_y = gameadd.map_subtiles_y;
    }
    if (end_x < 0) {
        end_x = 0;
    } else
    if (end_x > gameadd.map_subtiles_x) {
        end_x = gameadd.map_subtiles_x;
    }
    if (end_y < 0) {
        end_y = 0;
    } else
    if (end_y > gameadd.map_subtiles_y) {
        end_y = gameadd.map_subtiles_y;
    }
    long num_affected = 0;
    for (MapSubtlCoord stl_y = start_y; stl_y <= end_y; stl_y++)
    {
        for (MapSubtlCoord stl_x = start_x; stl_x <= end_x; stl_x++)
        {
            HitTargetFlags hit_targets = hit_type_to_hit_targets(tngsrc->shot_effect.hit_type);
            struct Map* mapblk = get_map_block_at(stl_x, stl_y);
            num_affected += poison_cloud_affecting_map_block(tngsrc, mapblk, pos, max_dist, max_damage/dmg_divider, 0, hit_targets, area_affect_type, DmgT_Respiratory);
        }
    }
    return num_affected;
}

TngUpdateRet update_effect(struct Thing *efftng)
{
    SYNCDBG(18,"Starting for %s",thing_model_name(efftng));
    TRACE_THING(efftng);
    struct Thing* subtng = NULL;
    const struct EffectConfigStats* effcst = get_effect_model_stats(efftng->model);
    if (efftng->parent_idx > 0) {
        subtng = thing_get(efftng->parent_idx);
        TRACE_THING(subtng);
    }
    if (efftng->health <= 0) {
        destroy_effect_thing(efftng);
        return TUFRet_Deleted;
    }
    update_effect_light_intensity(efftng);
    // Effect generators can be used to generate effect elements
    if ( (effcst->always_generate) || any_player_close_enough_to_see(&efftng->mappos) )
    {
        effect_generate_effect_elements(efftng);
    }
    // Let the effect affect area
    switch (effcst->area_affect_type)
    {
    case AAffT_GasDamage:
    case AAffT_GasSlow:
    case AAffT_GasSlowDamage:
    case AAffT_GasDisease:
        poison_cloud_affecting_area(efftng, &efftng->mappos, 5*COORD_PER_STL, 120, effcst->area_affect_type);
        break;
    case AAffT_WOPDamage:
        word_of_power_affecting_area(efftng, subtng, &efftng->mappos);
        break;
    }
    efftng->health--;
    return move_effect(efftng);
}

struct Thing *create_price_effect(const struct Coord3d *pos, long plyr_idx, long price)
{
    struct Thing* elemtng = create_effect_element(pos, TngEffElm_Price, plyr_idx);
    TRACE_THING(elemtng);
    if (!thing_is_invalid(elemtng)) {
        elemtng->price_effect.number = abs(price);
    }
    return elemtng;
}

/******************************************************************************/
#ifdef __cplusplus
}
#endif
