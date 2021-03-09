/*
 * Copyright (C) 2013.2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PLIM_URIS_H
#define PLIM_URIS_H

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

#define PLIM_URI "http://gareus.org/oss/lv2/dpl#"

#define HISTLEN 60

#ifdef HAVE_LV2_1_8
#define x_forge_object lv2_atom_forge_object
#else
#define x_forge_object lv2_atom_forge_blank
#endif

typedef struct {
	LV2_URID atom_Blank;
	LV2_URID atom_Object;
	LV2_URID atom_Vector;
	LV2_URID atom_Float;
	LV2_URID atom_Int;
	LV2_URID atom_eventTransfer;
	LV2_URID history;
	LV2_URID position;
	LV2_URID minvals;
	LV2_URID maxvals;
	LV2_URID ui_on;
	LV2_URID ui_off;
	LV2_URID state;
	LV2_URID s_uiscale;
} PlimLV2URIs;

static inline void
map_plim_uris (LV2_URID_Map* map, PlimLV2URIs* uris)
{
	uris->atom_Blank         = map->map (map->handle, LV2_ATOM__Blank);
	uris->atom_Object        = map->map (map->handle, LV2_ATOM__Object);
	uris->atom_Vector        = map->map (map->handle, LV2_ATOM__Vector);
	uris->atom_Float         = map->map (map->handle, LV2_ATOM__Float);
	uris->atom_Int           = map->map (map->handle, LV2_ATOM__Int);
	uris->atom_eventTransfer = map->map (map->handle, LV2_ATOM__eventTransfer);
	uris->history            = map->map (map->handle, PLIM_URI "history");
	uris->position           = map->map (map->handle, PLIM_URI "position");
	uris->minvals            = map->map (map->handle, PLIM_URI "minvals");
	uris->maxvals            = map->map (map->handle, PLIM_URI "maxvals");
	uris->ui_on              = map->map (map->handle, PLIM_URI "ui_on");
	uris->ui_off             = map->map (map->handle, PLIM_URI "ui_off");
	uris->state              = map->map (map->handle, PLIM_URI "state");
	uris->s_uiscale          = map->map (map->handle, PLIM_URI "uiscale");
}

/* common definitions UI and DSP */

typedef enum {
	PLIM_ATOM_CONTROL = 0,
	PLIM_ATOM_NOTIFY,

	PLIM_ENABLE,
	PLIM_GAIN,
	PLIM_THRESHOLD,
	PLIM_RELEASE,
	PLIM_TRUEPEAK,

	PLIM_LEVEL,
	PLIM_LATENCY,

	PLIM_INPUT0,
	PLIM_OUTPUT0,
	PLIM_INPUT1,
	PLIM_OUTPUT1,
	PLIM_LAST
} PortIndex;

#endif
