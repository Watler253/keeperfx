/******************************************************************************/
// Free implementation of Bullfrog's Dungeon Keeper strategy game.
/******************************************************************************/
/** @file thing_physics.h
 *     Header file for thing_physics.c.
 * @par Purpose:
 *     Implementation of physics functions used for things.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     25 Mar 2009 - 02 Mar 2011
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DK_TNGPHYSICS_H
#define DK_TNGPHYSICS_H

#include "bflib_basics.h"
#include "globals.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#define MAX_VELOCITY 256
/******************************************************************************/
#pragma pack(1)

enum MagicElementFlags {
    DTF_Neutral       = 0x000000,
    DTF_Physical      = 0x000001,
    DTF_Biological    = 0x000002,
    DTF_Electrical    = 0x000004,
    DTF_Mechanical    = 0x000008,
    DTF_Temporal      = 0x000010,
    DTF_Explosive     = 0x000020,
    DTF_Protective    = 0x000040,
    DTF_Poisonous     = 0x000080,
    DTF_Respiratory   = 0x000100,
    DTF_Heatburn      = 0x000200,
    DTF_Soaked        = 0x000400,
    DTF_Frostbite     = 0x000800,
    DTF_Hoarfrost     = 0x001000,
    DTF_Arcane        = 0x002000,
    DTF_Earth         = 0x004000, 
    DTF_Fire          = 0x008000,
    DTF_Water         = 0x010000,
    DTF_Wind          = 0x020000,
    DTF_Holy          = 0x040000,
    DTF_Darkness      = 0x080000,
    DTF_Unstable      = 0x100000,
};

struct Thing;
struct Dungeon;
struct ComponentVector;

#pragma pack()
/******************************************************************************/
TbBool thing_touching_floor(const struct Thing *thing);
TbBool thing_touching_flight_altitude(const struct Thing *thing);
TbBool thing_above_flight_altitude(const struct Thing* thing);

TbBool thing_on_thing_at(const struct Thing *firstng, const struct Coord3d *pos, const struct Thing *sectng);
TbBool things_collide_while_first_moves_to(const struct Thing *firstng, const struct Coord3d *dstpos, const struct Thing *sectng);
TbBool cross_x_boundary_first(const struct Coord3d *pos1, const struct Coord3d *pos2);
TbBool cross_y_boundary_first(const struct Coord3d *pos1, const struct Coord3d *pos2);

void slide_thing_against_wall_at(struct Thing *thing, struct Coord3d *pos, long blocked_flags);
void bounce_thing_off_wall_at(struct Thing *thing, struct Coord3d *pos, long blocked_flags);
TbBool get_thing_next_position(struct Coord3d *pos, const struct Thing *thing);
void remove_relevant_forces_from_thing_after_slide(struct Thing *thing, struct Coord3d *pos, long a3);
void apply_transitive_velocity_to_thing(struct Thing *thing, struct ComponentVector *veloc);
TbBool positions_equivalent(const struct Coord3d *pos_a, const struct Coord3d *pos_b);
TbBool creature_cannot_move_directly_to(struct Thing *thing, struct Coord3d *pos);
void creature_set_speed(struct Thing *thing, long speed);

long get_thing_height_at(const struct Thing *thing, const struct Coord3d *pos);
long get_thing_height_at_with_radius(const struct Thing *thing, const struct Coord3d *pos, unsigned long radius);
long thing_in_wall_at(const struct Thing *thing, const struct Coord3d *pos);
long thing_in_wall_at_with_radius(const struct Thing *thing, const struct Coord3d *pos, unsigned long radius);
TbBool creature_can_pass_through_wall_at(const struct Thing *thing, const struct Coord3d *pos);
long get_floor_height_under_thing_at(const struct Thing *thing, const struct Coord3d *pos);
long get_ceiling_height_above_thing_at(const struct Thing *thing, const struct Coord3d *pos);
void get_floor_and_ceiling_height_under_thing_at(const struct Thing *thing,
    const struct Coord3d *pos, MapCoord *floor_height_cor, MapCoord *ceiling_height_cor);
TbBool thing_is_exempt_from_z_axis_clipping(const struct Thing *thing);
unsigned short push_thingz_against_wall_at(const struct Thing *thing, const struct Coord3d *pos);
TbBool position_over_floor_level(const struct Thing* thing, const struct Coord3d* pos);
TbBool map_is_solid_at_height(MapSubtlCoord stl_x, MapSubtlCoord stl_y, MapCoord height_beg, MapCoord height_end);
TbBool move_object_to_nearest_free_position(struct Thing *thing);
void clear_thing_acceleration(struct Thing* thing);
void clear_thing_velocity(struct Thing* thing);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
