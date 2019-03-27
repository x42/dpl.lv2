/* plim.lv2
 *
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "peaklim.h"
#include "uris.h"

#include "lv2/lv2plug.in/ns/ext/state/state.h"
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/ext/options/options.h"

#ifdef DISPLAY_INTERFACE
#include "lv2_rgext.h"
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#endif

#ifndef MAX
#define MAX(A, B) ((A) > (B)) ? (A) : (B)
#endif

#ifndef MIN
#define MIN(A, B) ((A) < (B)) ? (A) : (B)
#endif

typedef struct {
	float* _port[PLIM_LAST];

	DPLLV2::Peaklim* peaklim;

	/* history */
	float    _peak;
	float    _min[HISTLEN];
	float    _max[HISTLEN];
	uint32_t _hist;

	uint32_t samplecnt;
	uint32_t sampletme; // 50ms

	/* atom-forge, UI communication */
	const LV2_Atom_Sequence* control;
	LV2_Atom_Sequence*       notify;
	LV2_URID_Map*            map;
	PlimLV2URIs              uris;
	LV2_Atom_Forge           forge;
	LV2_Atom_Forge_Frame     frame;

	/* GUI state */
	bool  ui_active;
	bool  send_state_to_ui;
	float ui_scale;

#ifdef DISPLAY_INTERFACE
	LV2_Inline_Display_Image_Surface surf;
	cairo_surface_t*                 display;
	LV2_Inline_Display*              queue_draw;
	cairo_pattern_t*                 mpat;
	uint32_t                         w, h;
	float                            ui_reduction;
#endif

} Plim;

static LV2_Handle
instantiate (const LV2_Descriptor*     descriptor,
             double                    rate,
             const char*               bundle_path,
             const LV2_Feature* const* features)
{
	Plim* self = (Plim*)calloc (1, sizeof (Plim));

	uint32_t n_channels;

	if (!strcmp (descriptor->URI, PLIM_URI "mono")) {
		n_channels = 1;
	} else if (!strcmp (descriptor->URI, PLIM_URI "stereo")) {
		n_channels = 2;
	} else {
		free (self);
		return NULL;
	}

	const LV2_Options_Option* options = NULL;

	for (int i = 0; features[i]; ++i) {
		if (!strcmp (features[i]->URI, LV2_URID__map)) {
			self->map = (LV2_URID_Map*)features[i]->data;
		} else if (!strcmp(features[i]->URI, LV2_OPTIONS__options)) {
			options = (LV2_Options_Option*)features[i]->data;
		}
#ifdef DISPLAY_INTERFACE
		else if (!strcmp (features[i]->URI, LV2_INLINEDISPLAY__queue_draw)) {
			self->queue_draw = (LV2_Inline_Display*)features[i]->data;
		}
#endif
	}

	if (!self->map) {
		fprintf (stderr, "dpl.lv2 error: Host does not support urid:map\n");
		free (self);
		return NULL;
	}

	lv2_atom_forge_init (&self->forge, self->map);
	map_plim_uris (self->map, &self->uris);

	self->ui_active = false;
	self->ui_scale  = 1.0;
	self->_peak     = -20;

	for (int i = 0; i < HISTLEN; ++i) {
		self->_min[i] = self->_max[i] = 1.0;
	}

	self->peaklim = new DPLLV2::Peaklim ();
	self->peaklim->init (rate, n_channels);

	self->sampletme = ceilf (rate * 0.05); // 50ms

	if (options) {
		LV2_URID atom_Float = self->map->map (self->map->handle, LV2_ATOM__Float);
		LV2_URID ui_scale   = self->map->map (self->map->handle, "http://lv2plug.in/ns/extensions/ui/#scaleFactor");
		for (const LV2_Options_Option* o = options; o->key; ++o) {
			if (o->context == LV2_OPTIONS_INSTANCE && o->key == ui_scale && o->type == atom_Float) {
				float ui_scale = *(const float*)o->value;
				if (ui_scale < 1.0) { ui_scale = 1.0; }
				if (ui_scale > 2.0) { ui_scale = 2.0; }
				self->ui_scale = ui_scale;
			}
		}
	}

	return (LV2_Handle)self;
}

static void
connect_port (LV2_Handle instance,
              uint32_t   port,
              void*      data)
{
	Plim* self = (Plim*)instance;
	if (port == PLIM_ATOM_CONTROL) {
		self->control = (const LV2_Atom_Sequence*)data;
	} else if (port == PLIM_ATOM_NOTIFY) {
		self->notify = (LV2_Atom_Sequence*)data;
	} else if (port < PLIM_LAST) {
		self->_port[port] = (float*)data;
	}
}

/** forge atom-vector of raw data */
static void
tx_history (Plim* self)
{
	LV2_Atom_Forge_Frame frame;
	/* forge container object of type 'rawaudio' */
	lv2_atom_forge_frame_time (&self->forge, 0);
	x_forge_object (&self->forge, &frame, 1, self->uris.history);

	/* add integer attribute 'channelid' */
	lv2_atom_forge_property_head (&self->forge, self->uris.position, 0);
	lv2_atom_forge_int (&self->forge, self->_hist);

	/* add vector of floats raw */
	lv2_atom_forge_property_head (&self->forge, self->uris.minvals, 0);
	lv2_atom_forge_vector (&self->forge, sizeof (float), self->uris.atom_Float, HISTLEN, self->_min);

	lv2_atom_forge_property_head (&self->forge, self->uris.maxvals, 0);
	lv2_atom_forge_vector (&self->forge, sizeof (float), self->uris.atom_Float, HISTLEN, self->_max);

	/* close off atom-object */
	lv2_atom_forge_pop (&self->forge, &frame);
}

static void
tx_state (Plim* self)
{
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time (&self->forge, 0);
	x_forge_object (&self->forge, &frame, 1, self->uris.state);

	lv2_atom_forge_property_head (&self->forge, self->uris.s_uiscale, 0);
	lv2_atom_forge_float (&self->forge, self->ui_scale);

	lv2_atom_forge_pop (&self->forge, &frame);
}

static void
run (LV2_Handle instance, uint32_t n_samples)
{
	Plim* self = (Plim*)instance;

	if (!self->control || !self->notify) {
		*self->_port[PLIM_LEVEL]   = -10;
		*self->_port[PLIM_LATENCY] = self->peaklim->get_latency ();
		if (self->_port[PLIM_INPUT0] != self->_port[PLIM_OUTPUT0]) {
			memcpy (self->_port[PLIM_OUTPUT0], self->_port[PLIM_INPUT0], n_samples * sizeof (float));
		}
		if (self->_port[PLIM_INPUT1] != self->_port[PLIM_OUTPUT1]) {
			memcpy (self->_port[PLIM_OUTPUT1], self->_port[PLIM_INPUT1], n_samples * sizeof (float));
		}
		return;
	}

	/* prepare forge buffer and initialize atom-sequence */
	const uint32_t capacity = self->notify->atom.size;
	lv2_atom_forge_set_buffer (&self->forge, (uint8_t*)self->notify, capacity);
	lv2_atom_forge_sequence_head (&self->forge, &self->frame, 0);

	/* process messages from GUI; */
	if (self->control) {
		LV2_Atom_Event* ev = lv2_atom_sequence_begin (&(self->control)->body);
		while (!lv2_atom_sequence_is_end (&(self->control)->body, (self->control)->atom.size, ev)) {
			if (ev->body.type == self->uris.atom_Blank || ev->body.type == self->uris.atom_Object) {
				const LV2_Atom_Object* obj = (LV2_Atom_Object*)&ev->body;
				if (obj->body.otype == self->uris.ui_off) {
					self->ui_active = false;
				} else if (obj->body.otype == self->uris.ui_on) {
					self->ui_active        = true;
					self->send_state_to_ui = true;
				} else if (obj->body.otype == self->uris.state) {
					const LV2_Atom* v = NULL;
					lv2_atom_object_get (obj, self->uris.s_uiscale, &v, 0);
					if (v) {
						self->ui_scale = ((LV2_Atom_Float*)v)->body;
					}
				}
			}
			ev = lv2_atom_sequence_next (ev);
		}
	}

#define BYPASS_THRESH 40 // dBFS

	/* bypass/enable */
	const bool enable = *self->_port[PLIM_ENABLE] > 0;

	if (enable) {
		self->peaklim->set_inpgain (*self->_port[PLIM_GAIN]);
		self->peaklim->set_threshold (*self->_port[PLIM_THRESHOLD]);
		self->peaklim->set_release (*self->_port[PLIM_RELEASE]);
	} else {
		self->peaklim->set_inpgain (0);
		self->peaklim->set_threshold (BYPASS_THRESH);
		self->peaklim->set_release (.05);
	}

	float* ins[2]  = { self->_port[PLIM_INPUT0], self->_port[PLIM_INPUT1] };
	float* outs[2] = { self->_port[PLIM_OUTPUT0], self->_port[PLIM_OUTPUT1] };

	self->peaklim->process (n_samples, ins, outs);

	bool tx = false;

	self->samplecnt += n_samples;
	while (self->samplecnt >= self->sampletme) {
		self->samplecnt -= self->sampletme;
		float pk;
		self->peaklim->get_stats (&pk, &self->_max[self->_hist], &self->_min[self->_hist]);

		pk = pk < 0.1 ? -20 : (20. * log10f (pk));

		if (self->_peak > -20) {
			self->_peak -= .3; // 6dB/sec
		}
		if (pk > self->_peak) {
			self->_peak = pk;
		}

#ifdef DISPLAY_INTERFACE
		const float display_lvl = enable ? fmaxf (-10.f, self->_peak) : -100.f;
		if (self->queue_draw && self->ui_reduction != display_lvl) {
			self->ui_reduction = display_lvl;
			self->queue_draw->queue_draw (self->queue_draw->handle);
		}
#endif

		self->_hist = (self->_hist + 1) % HISTLEN;
		tx          = true;
	}

	*self->_port[PLIM_LEVEL]   = enable ? fmaxf (-10.f, self->_peak) : -10;
	*self->_port[PLIM_LATENCY] = self->peaklim->get_latency ();

	if (self->ui_active && self->send_state_to_ui) {
		self->send_state_to_ui = false;
		tx_state (self);
		tx_history (self);
	} else if (self->ui_active && tx) {
		tx_history (self);
	}

	/* close off atom-sequence */
	lv2_atom_forge_pop (&self->forge, &self->frame);
}

#define STATESTORE(URI, TYPE, VALUE)               \
	store (handle, self->uris.URI,             \
	       (void*)&(VALUE), sizeof (uint32_t), \
	       self->uris.atom_##TYPE,             \
	       LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);

static LV2_State_Status
plim_save (LV2_Handle                instance,
           LV2_State_Store_Function  store,
           LV2_State_Handle          handle,
           uint32_t                  flags,
           const LV2_Feature* const* features)
{
	Plim* self = (Plim*)instance;

	STATESTORE (s_uiscale, Float, self->ui_scale)

	return LV2_STATE_SUCCESS;
}

/* clang-format off */
#define STATEREAD(URI, TYPE, CAST, PARAM)                                     \
  value = retrieve (handle, self->uris.URI, &size, &type, &valflags);         \
  if (value && size == sizeof (uint32_t) && type == self->uris.atom_##TYPE) { \
    PARAM = *((const CAST*)value);                                            \
  }
/* clang-format on */

static LV2_State_Status
plim_restore (LV2_Handle                  instance,
              LV2_State_Retrieve_Function retrieve,
              LV2_State_Handle            handle,
              uint32_t                    flags,
              const LV2_Feature* const*   features)
{
	Plim*       self = (Plim*)instance;
	const void* value;
	size_t      size;
	uint32_t    type;
	uint32_t    valflags;

	STATEREAD (s_uiscale, Float, float, self->ui_scale)

	self->send_state_to_ui = true;
	return LV2_STATE_SUCCESS;
}

static void
cleanup (LV2_Handle instance)
{
	Plim* self = (Plim*)instance;
	delete self->peaklim;
#ifdef DISPLAY_INTERFACE
	if (self->mpat) {
		cairo_pattern_destroy (self->mpat);
	}
	if (self->display) {
		cairo_surface_destroy (self->display);
	}
#endif
	free (instance);
}

#ifdef WITH_SIGNATURE
#define RTK_URI PLIM_URI
#include "gpg_init.c"
#include WITH_SIGNATURE
struct license_info license_infos = {
	"x42-Equalizer",
	"http://x42-plugins.com/x42/x42-eq"
};
#include "gpg_lv2ext.c"
#endif

#ifdef DISPLAY_INTERFACE
static void
create_pattern (Plim* self, const double w)
{
	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, w, 0);

	const int x0 = floor (w * 0.05);
	const int x1 = ceil (w * 0.95);
	const int wd = x1 - x0;

#define DEF(x) ((x1 - wd * (x) / 20.) / w)
	cairo_pattern_add_color_stop_rgba (pat, 1.0, .7, .7, .0, 0);
	cairo_pattern_add_color_stop_rgba (pat, DEF (0), .7, .7, .0, 1);
	cairo_pattern_add_color_stop_rgba (pat, DEF (5), .7, .7, .0, 1);
	cairo_pattern_add_color_stop_rgba (pat, DEF (20), .9, .0, .0, 1);
	cairo_pattern_add_color_stop_rgba (pat, 0.0, .9, .0, .0, 0);
#undef DEF

	self->mpat = pat;
}

static LV2_Inline_Display_Image_Surface*
dpl_render (LV2_Handle handle, uint32_t w, uint32_t max_h)
{
#ifdef WITH_SIGNATURE
	if (!is_licensed (handle)) {
		return NULL;
	}
#endif
	uint32_t h = MAX (11, MIN (1 | (uint32_t)ceilf (w / 10.f), max_h));

	Plim* self = (Plim*)handle;

	if (!self->display || self->w != w || self->h != h) {
		if (self->display)
			cairo_surface_destroy (self->display);
		self->display = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
		self->w       = w;
		self->h       = h;
		if (self->mpat) {
			cairo_pattern_destroy (self->mpat);
			self->mpat = NULL;
		}
	}

	if (!self->mpat) {
		create_pattern (self, w);
	}

	cairo_t* cr = cairo_create (self->display);
	cairo_rectangle (cr, 0, 0, w, h);
	cairo_set_source_rgba (cr, .2, .2, .2, 1.0);
	cairo_fill (cr);

	const int x0 = floor (w * 0.05);
	const int x1 = ceil (w * 0.95);
	const int wd = x1 - x0;

	cairo_set_line_width (cr, 1);
	cairo_set_source_rgba (cr, 0.8, 0.8, 0.8, 1.0);

#define DEF(x) (rint (x1 - wd * (x) / 20.) - .5)
	cairo_move_to (cr, DEF (0), 0);
	cairo_rel_line_to (cr, 0, h);
	cairo_stroke (cr);
	cairo_move_to (cr, DEF (5), 0);
	cairo_rel_line_to (cr, 0, h);
	cairo_stroke (cr);
	cairo_move_to (cr, DEF (10), 0);
	cairo_rel_line_to (cr, 0, h);
	cairo_stroke (cr);
	cairo_move_to (cr, DEF (15), 0);
	cairo_rel_line_to (cr, 0, h);
	cairo_stroke (cr);
	cairo_move_to (cr, DEF (20), 0);
	cairo_rel_line_to (cr, 0, h);
	cairo_stroke (cr);
#undef DEF

	cairo_rectangle (cr, x0, 2, wd, h - 5);
	cairo_set_source_rgba (cr, .5, .5, .5, 0.6);
	cairo_fill (cr);

#define CLAMP01(x) (((x) > 1.f) ? 1.f : (((x) < 0.f) ? 0.f : (x)))
	if (self->ui_reduction >= -10.f) {
		const int xw = wd * CLAMP01 (self->ui_reduction / 20.f);
		cairo_rectangle (cr, x1 - xw, 2, xw, h - 5);
		cairo_set_source (cr, self->mpat);
		cairo_fill (cr);
	} else {
		cairo_rectangle (cr, 0, 0, w, h);
		cairo_set_source_rgba (cr, .2, .2, .2, 0.8);
		cairo_fill (cr);
	}

	/* finish surface */
	cairo_destroy (cr);
	cairo_surface_flush (self->display);
	self->surf.width  = cairo_image_surface_get_width (self->display);
	self->surf.height = cairo_image_surface_get_height (self->display);
	self->surf.stride = cairo_image_surface_get_stride (self->display);
	self->surf.data   = cairo_image_surface_get_data (self->display);

	return &self->surf;
}
#endif

const void*
extension_data (const char* uri)
{
	static const LV2_State_Interface state = { plim_save, plim_restore };
	if (!strcmp (uri, LV2_STATE__interface)) {
		return &state;
	}
#ifdef DISPLAY_INTERFACE
	static const LV2_Inline_Display_Interface display = { dpl_render };
	if (!strcmp (uri, LV2_INLINEDISPLAY__interface)) {
#if (defined _WIN32 && defined RTK_STATIC_INIT)
		static int once = 0;
		if (!once) {
			once = 1;
			gobject_init_ctor ();
		}
#endif
		return &display;
	}
#endif
#ifdef WITH_SIGNATURE
	LV2_LICENSE_EXT_C
#endif
	return NULL;
}

static const LV2_Descriptor descriptor_mono = {
	PLIM_URI "mono",
	instantiate,
	connect_port,
	NULL,
	run,
	NULL,
	cleanup,
	extension_data
};

static const LV2_Descriptor descriptor_stereo = {
	PLIM_URI "stereo",
	instantiate,
	connect_port,
	NULL,
	run,
	NULL,
	cleanup,
	extension_data
};

/* clang-format off */
#undef LV2_SYMBOL_EXPORT
#ifdef _WIN32
# define LV2_SYMBOL_EXPORT __declspec(dllexport)
#else
# define LV2_SYMBOL_EXPORT __attribute__ ((visibility ("default")))
#endif
/* clang-format on */
LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor (uint32_t index)
{
	switch (index) {
		case 0:
			return &descriptor_mono;
		case 1:
			return &descriptor_stereo;
		default:
			return NULL;
	}
}
