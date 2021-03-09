/* robtk plim gui
 *
 * Copyright 2016 Robin Gareus <robin@gareus.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/uris.h"

#define RTK_USE_HOST_COLORS

#define RTK_URI PLIM_URI
#define RTK_GUI "ui"

#ifndef MAX
#define MAX(A, B) ((A) > (B)) ? (A) : (B)
#endif

#ifndef MIN
#define MIN(A, B) ((A) < (B)) ? (A) : (B)
#endif

#define THRESHOLD_ABBREV "Thld"

struct CtrlRange {
	float       min;
	float       max;
	float       dflt;
	float       step;
	float       mult;
	bool        log;
	const char* name;
};

typedef struct {
	LV2UI_Write_Function write;
	LV2UI_Controller     controller;
	LV2_Atom_Forge       forge;
	LV2_URID_Map*        map;
	LV2UI_Touch*         touch;
	PlimLV2URIs          uris;

	PangoFontDescription* font[3];

	RobWidget* rw;   // top-level container
	RobWidget* ctbl; // control element table

	/* Level + reduction drawing area */
	RobWidget* m0;
	int        m0_width;
	int        m0_height;

	/* history */
	float    _peak;
	float    _min[HISTLEN];
	float    _max[HISTLEN];
	uint32_t _hist;

	RobTkDial* spn_ctrl[3];
	RobTkLbl*  lbl_ctrl[3];
	RobTkCBtn* btn_truepeak;

	cairo_pattern_t* m_fg;
	cairo_pattern_t* m_bg;
	cairo_surface_t* dial_bg[3];

	bool disable_signals;

	int                tt_id;
	int                tt_timeout;
	cairo_rectangle_t* tt_pos;
	cairo_rectangle_t* tt_box;

	const char* nfo;
} PLimUI;

const struct CtrlRange ctrl_range[] = {
	{ -10, 30, 0, 0.2, 5, false, "Input Gain" },
	{ -10, 0, -1, 0.1, 1, false, "Threshold" },
	{ .001, 1, 0.01, 150, 5, true, "Release" },
};

static const char* tooltips[] = {
	"<markup><b>Input Gain.</b> Gain applied\nbefore peak detection or any\nother processing.\n</markup>",

	"<markup><b>Threshold.</b> The maximum sample\nvalue at the output.\n</markup>", // unused

	"<markup><b>Release time.</b> Minimum recovery\ntime. Low frequency content\nmay extend this.\n</markup>",
	/*
	"Note that DPL1 allows short release times even on signals that\n"
	"contain high level low frequency signals. Any gain reduction caused by\n"
	"those will have an automatically extended hold time in order to avoid\n"
	"the limiter following the shape of the waveform and create excessive distortion.\n"
	"Short superimposed peaks will still have the release time as set by this control.\n</markup>",
	*/
	"<markup><b>Threshold (Sample/True Peak).</b> Set\nmaximum alloed value at the output\nas digital or oversampled peak.\n</markup>",
};

static float
ctrl_to_gui (const uint32_t c, const float v)
{
	if (!ctrl_range[c].log) {
		return v;
	}
	const float r = logf (ctrl_range[c].max / ctrl_range[c].min);
	return rintf (ctrl_range[c].step / r * (logf (v / ctrl_range[c].min)));
}

static float
gui_to_ctrl (const uint32_t c, const float v)
{
	if (!ctrl_range[c].log) {
		return v;
	}
	const float r = log (ctrl_range[c].max / ctrl_range[c].min);
	return expf (logf (ctrl_range[c].min) + v * r / ctrl_range[c].step);
}

static float
k_min (const uint32_t c)
{
	if (!ctrl_range[c].log) {
		return ctrl_range[c].min;
	}
	return 0;
}

static float
k_max (const uint32_t c)
{
	if (!ctrl_range[c].log) {
		return ctrl_range[c].max;
	}
	return ctrl_range[c].step;
}

static float
k_step (const uint32_t c)
{
	if (!ctrl_range[c].log) {
		return ctrl_range[c].step;
	}
	return 1;
}

///////////////////////////////////////////////////////////////////////////////

static float c_dlf[4] = { 0.8, 0.8, 0.8, 1.0 }; // dial faceplate fg

///////////////////////////////////////////////////////////////////////////////

/*** knob faceplates ***/
static void
prepare_faceplates (PLimUI* ui)
{
	cairo_t* cr;
	float    xlp, ylp;

/* clang-format off */
#define INIT_DIAL_SF(VAR, W, H)                                             \
  VAR = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 2 * (W), 2 * (H)); \
  cr  = cairo_create (VAR);                                                 \
  cairo_scale (cr, 2.0, 2.0);                                               \
  CairoSetSouerceRGBA (c_trs);                                              \
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);                           \
  cairo_rectangle (cr, 0, 0, W, H);                                         \
  cairo_fill (cr);                                                          \
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

#define DIALDOTS(V, XADD, YADD)                                \
  float ang = (-.75 * M_PI) + (1.5 * M_PI) * (V);              \
  xlp       = GED_CX + XADD + sinf (ang) * (GED_RADIUS + 3.0); \
  ylp       = GED_CY + YADD - cosf (ang) * (GED_RADIUS + 3.0); \
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);               \
  CairoSetSouerceRGBA (c_dlf);                                 \
  cairo_set_line_width (cr, 2.5);                              \
  cairo_move_to (cr, rint (xlp) - .5, rint (ylp) - .5);        \
  cairo_close_path (cr);                                       \
  cairo_stroke (cr);

#define RESPLABLEL(V)                                             \
  {                                                               \
    DIALDOTS (V, 4.5, 15.5)                                       \
    xlp = rint (GED_CX + 4.5 + sinf (ang) * (GED_RADIUS + 9.5));  \
    ylp = rint (GED_CY + 15.5 - cosf (ang) * (GED_RADIUS + 9.5)); \
  }
	/* clang-format on */

	INIT_DIAL_SF (ui->dial_bg[0], GED_WIDTH + 8, GED_HEIGHT + 20);

	RESPLABLEL (0.00);
	write_text_full (cr, "-10", ui->font[0], xlp + 6, ylp, 0, 1, c_dlf);
	RESPLABLEL (0.25);
	RESPLABLEL (0.5);
	write_text_full (cr, "10", ui->font[0], xlp, ylp, 0, 2, c_dlf);
	RESPLABLEL (.75);
	RESPLABLEL (1.0);
	write_text_full (cr, "30", ui->font[0], xlp - 6, ylp, 0, 3, c_dlf);
	cairo_destroy (cr);

	INIT_DIAL_SF (ui->dial_bg[1], GED_WIDTH + 8, GED_HEIGHT + 20);
	RESPLABLEL (0.00);
	write_text_full (cr, "-10", ui->font[0], xlp + 6, ylp, 0, 1, c_dlf);
	RESPLABLEL (.2);
	RESPLABLEL (.4);
	RESPLABLEL (.6);
	RESPLABLEL (.8);
	RESPLABLEL (1.0);
	write_text_full (cr, "0", ui->font[0], xlp - 6, ylp, 0, 3, c_dlf);
	cairo_destroy (cr);

	INIT_DIAL_SF (ui->dial_bg[2], GED_WIDTH + 8, GED_HEIGHT + 20);
	RESPLABLEL (0.00);
	write_text_full (cr, "1ms", ui->font[0], xlp + 2, ylp, 0, 1, c_dlf);
	RESPLABLEL (.16);
	RESPLABLEL (.33);
	write_text_full (cr, "10  ", ui->font[0], xlp, ylp, 0, 2, c_dlf);
	RESPLABLEL (0.5);
	RESPLABLEL (.66);
	write_text_full (cr, "  0.1", ui->font[0], xlp, ylp, 0, 2, c_dlf);
	RESPLABLEL (.83);
	RESPLABLEL (1.0);
	write_text_full (cr, "1s", ui->font[0], xlp - 2, ylp, 0, 3, c_dlf);
	cairo_destroy (cr);

#undef DIALDOTS
#undef INIT_DIAL_SF
#undef RESPLABLEL
}

static void
display_annotation (PLimUI* ui, RobTkDial* d, cairo_t* cr, const char* txt)
{
	int tw, th;
	cairo_save (cr);
	PangoLayout* pl = pango_cairo_create_layout (cr);
	pango_layout_set_font_description (pl, ui->font[0]);
	pango_layout_set_text (pl, txt, -1);
	pango_layout_get_pixel_size (pl, &tw, &th);
	cairo_translate (cr, d->w_width / 2, d->w_height - 2);
	cairo_translate (cr, -tw / 2.0, -th);
	cairo_set_source_rgba (cr, .0, .0, .0, .7);
	rounded_rectangle (cr, -1, -1, tw + 3, th + 1, 3);
	cairo_fill (cr);
	CairoSetSouerceRGBA (c_wht);
	pango_cairo_show_layout (cr, pl);
	g_object_unref (pl);
	cairo_restore (cr);
	cairo_new_path (cr);
}

static void
dial_annotation_db (RobTkDial* d, cairo_t* cr, void* data)
{
	PLimUI* ui = (PLimUI*)(data);
	char    txt[16];
	snprintf (txt, 16, "%5.1f dB", d->cur);
	display_annotation (ui, d, cr, txt);
}

static void
format_msec (char* txt, const float val)
{
	if (val < 0.03) {
		snprintf (txt, 16, "%.1f ms", val * 1000.f);
	} else if (val < 0.3) {
		snprintf (txt, 16, "%.0f ms", val * 1000.f);
	} else {
		snprintf (txt, 16, "%.2f s", val);
	}
}

static void
dial_annotation_tm (RobTkDial* d, cairo_t* cr, void* data)
{
	PLimUI*     ui = (PLimUI*)(data);
	char        txt[16];
	const float val = gui_to_ctrl (2, d->cur);
	format_msec (txt, val);
	display_annotation (ui, d, cr, txt);
}

///////////////////////////////////////////////////////////////////////////////

/*** global tooltip overlay ****/

static bool
tooltip_overlay (RobWidget* rw, cairo_t* cr, cairo_rectangle_t* ev)
{
	PLimUI* ui = (PLimUI*)rw->top;
	assert (ui->tt_id >= 0 && ui->tt_id < 4);

	cairo_save (cr);
	rw->resized = TRUE;
	rcontainer_expose_event (rw, cr, ev);
	cairo_restore (cr);

	const float top = ui->tt_box->y;
	rounded_rectangle (cr, 0, top, rw->area.width, ui->tt_pos->y + 1 - top, 3);
	cairo_set_source_rgba (cr, 0, 0, 0, .7);
	cairo_fill (cr);

	if (ui->tt_id < 3) {
		rounded_rectangle (cr, ui->tt_pos->x + 1, ui->tt_pos->y + 1,
				ui->tt_pos->width + 3, ui->tt_pos->height + 1, 3);
		cairo_set_source_rgba (cr, 1, 1, 1, .5);
		cairo_fill (cr);
	}

	const float*          color = c_wht;
	PangoFontDescription* font  = pango_font_description_from_string ("Sans 11px");

	const float xp = rw->area.width * .5;
	const float yp = rw->area.height * .5;

	cairo_save (cr);
	cairo_scale (cr, rw->widget_scale, rw->widget_scale);
	write_text_full (cr, tooltips[ui->tt_id], font,
	                 xp / rw->widget_scale, yp / rw->widget_scale,
	                 0, 2, color);
	cairo_restore (cr);

	pango_font_description_free (font);
	return TRUE;
}

static bool
tooltip_cnt (RobWidget* rw, cairo_t* cr, cairo_rectangle_t* ev)
{
	PLimUI* ui = (PLimUI*)rw->top;
	if (++ui->tt_timeout < 8) {
		rcontainer_expose_event (rw, cr, ev);
		queue_draw (rw);
	} else {
		rw->expose_event = tooltip_overlay;
		rw->resized      = TRUE;
		tooltip_overlay (rw, cr, ev);
	}
	return TRUE;
}

static void
ttip_handler (RobWidget* rw, bool on, void* handle)
{
	PLimUI* ui     = (PLimUI*)handle;
	ui->tt_id      = -1;
	ui->tt_timeout = 0;

	for (int i = 0; i < 3; ++i) {
		if (rw == ui->lbl_ctrl[i]->rw) {
			ui->tt_id = i;
			break;
		}
	}
	if (rw == ui->btn_truepeak->rw) {
		ui->tt_id = 3;
	}

	if (on && ui->tt_id >= 0) {
		ui->tt_pos             = &rw->area;
		ui->tt_box             = &ui->spn_ctrl[0]->rw->area;
		ui->ctbl->expose_event = tooltip_cnt;
		ui->ctbl->resized      = TRUE;
		queue_draw (ui->ctbl);
	} else {
		ui->ctbl->expose_event    = rcontainer_expose_event;
		ui->ctbl->parent->resized = TRUE; //full re-expose
		queue_draw (ui->rw);
	}
}

///////////////////////////////////////////////////////////////////////////////

/*** knob & button callbacks ****/

static bool
cb_spn_ctrl (RobWidget* w, void* handle)
{
	PLimUI* ui = (PLimUI*)handle;
	if (ui->disable_signals)
		return TRUE;

	for (uint32_t i = 0; i < 3; ++i) {
		if (w != ui->spn_ctrl[i]->rw) {
			continue;
		}
		const float val = gui_to_ctrl (i, robtk_dial_get_value (ui->spn_ctrl[i]));
		ui->write (ui->controller, PLIM_GAIN + i, sizeof (float), 0, (const void*)&val);
		break;
	}
	return TRUE;
}

static bool
cb_btn_truepeak (RobWidget* w, void* handle)
{
	PLimUI* ui = (PLimUI*)handle;
	if (ui->disable_signals)
		return TRUE;

	const float val = robtk_cbtn_get_active (ui->btn_truepeak) ? 1.f : 0.f;
	robtk_cbtn_set_text (ui->btn_truepeak, val > 0 ? THRESHOLD_ABBREV " dBTP" : THRESHOLD_ABBREV " dBFS");
	ui->write (ui->controller, PLIM_TRUEPEAK, sizeof (float), 0, (const void*)&val);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////

static void
m0_size_request (RobWidget* handle, int* w, int* h)
{
	*w = 320;
	*h = 80;
}

static void
m0_size_allocate (RobWidget* handle, int w, int h)
{
	PLimUI* ui    = (PLimUI*)GET_HANDLE (handle);
	ui->m0_width  = w;
	ui->m0_height = h;
	robwidget_set_size (ui->m0, w, h);

	if (ui->m_fg) {
		cairo_pattern_destroy (ui->m_fg);
	}
	if (ui->m_bg) {
		cairo_pattern_destroy (ui->m_bg);
	}
	ui->m_fg = ui->m_bg = NULL;

	if (1) {
		int scale = MIN (w / 180, h / 80);
		pango_font_description_free (ui->font[1]);
		pango_font_description_free (ui->font[2]);
		char fnt[32];
		snprintf (fnt, 32, "Mono %.0fpx\n", 10 * sqrtf (scale));
		ui->font[1] = pango_font_description_from_string (fnt);
		snprintf (fnt, 32, "Mono Bold %.0fpx\n", 12 * sqrtf (scale));
		ui->font[2] = pango_font_description_from_string (fnt);
	}

	queue_draw (ui->m0);
}

static bool
m0_expose_event (RobWidget* handle, cairo_t* cr, cairo_rectangle_t* ev)
{
	PLimUI* ui = (PLimUI*)GET_HANDLE (handle);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_rectangle (cr, ev->x, ev->y, ev->width, ev->height);
	cairo_clip_preserve (cr);

	float c[4];
	get_color_from_theme (1, c);
	cairo_set_source_rgb (cr, c[0], c[1], c[2]);
	cairo_fill (cr);

	const uint32_t yscale = ui->m0_height / 80;
	const uint32_t top    = (ui->m0_height - 80 * yscale) * .5;
	const uint32_t disp_w = ui->m0_width - 20; // deafult: 300
#define YPOS(y) (top + yscale * (y))
#define HGHT(y) (yscale * (y))

	CairoSetSouerceRGBA (c_blk);
	rounded_rectangle (cr, 0, top, ui->m0_width, HGHT (80), 6);
	cairo_fill_preserve (cr);
	cairo_clip (cr);

#define DEF(x) MAX (0, MIN (1., ((10. + (x)) / 30.)))
#define DEFLECT(x) (disp_w * DEF (x))
#define PX (1.0 / (disp_w - 10.))

	if (!ui->m_fg) {
		cairo_pattern_t* pat = cairo_pattern_create_linear (10, 0.0, disp_w, 0.0);
		cairo_pattern_add_color_stop_rgb (pat, DEF (-10), .0, .8, .0);
		cairo_pattern_add_color_stop_rgb (pat, DEF (0), .0, .8, .0);
		cairo_pattern_add_color_stop_rgb (pat, DEF (0) + PX, .7, .7, .0);
		cairo_pattern_add_color_stop_rgb (pat, DEF (5), .7, .7, .0);
		cairo_pattern_add_color_stop_rgb (pat, DEF (20), .9, .0, .0);
		ui->m_fg = pat;
	}

	if (!ui->m_bg) {
		const float      alpha = 0.5;
		cairo_pattern_t* pat   = cairo_pattern_create_linear (10, 0.0, disp_w, 0.0);
		cairo_pattern_add_color_stop_rgba (pat, DEF (-10), .0, .8, .0, alpha);
		cairo_pattern_add_color_stop_rgba (pat, DEF (0), .0, .8, .0, alpha);
		cairo_pattern_add_color_stop_rgba (pat, DEF (0) + PX, .7, .7, .0, alpha);
		cairo_pattern_add_color_stop_rgba (pat, DEF (5), .7, .7, .0, alpha);
		cairo_pattern_add_color_stop_rgba (pat, DEF (20), .9, .0, .0, alpha);
		ui->m_bg = pat;
	}

	if (ui->nfo) {
		write_text_full (cr, ui->nfo, ui->font[0], ui->m0_width - 1, top + 3, 1.5 * M_PI, 4, c_g30);
	}

	/* meter background */
	cairo_set_source (cr, ui->m_bg);
	cairo_rectangle (cr, 5, YPOS (68), disp_w + 10, HGHT (8));
	cairo_fill (cr);

	cairo_set_line_width (cr, yscale);
	cairo_set_source (cr, ui->m_fg);

	/* reduction history */
	for (int i = 0; i < HISTLEN; ++i) {
		int p = (i + ui->_hist) % HISTLEN;

		const int x0 = DEFLECT (-20.0f * log10f (ui->_max[p]));
		const int x1 = DEFLECT (-20.0f * log10f (ui->_min[p]));

		cairo_move_to (cr, 9 + x0, YPOS (i + .5));
		cairo_line_to (cr, 10 + x1, YPOS (i + .5));

		cairo_stroke (cr);
	}

	/* current reduction */
	if (ui->_peak > -10) {
		cairo_rectangle (cr, 5, YPOS (68), 5 + DEFLECT (ui->_peak), HGHT (8));
		cairo_fill (cr);
	}

	// TODO: cache this background

	/* meter ticks */
	cairo_set_line_width (cr, 1);
	CairoSetSouerceRGBA (c_wht);
	for (int i = 0; i < 7; ++i) {
		int dbx = DEFLECT (-10 + i * 5);
		cairo_move_to (cr, 9.5 + dbx, YPOS (68));
		cairo_line_to (cr, 9.5 + dbx, YPOS (76));
		cairo_stroke (cr);

		if (i > 0) {
			int          tw, th;
			PangoLayout* pl = pango_cairo_create_layout (cr);
			pango_layout_set_font_description (pl, ui->font[1]);
			if (i > 1) {
				char txt[16];
				snprintf (txt, 16, "-%d ", (i - 2) * 5);
				pango_layout_set_text (pl, txt, -1);
			} else {
				pango_layout_set_text (pl, "Gain Reduction:", -1);
			}
			CairoSetSouerceRGBA (c_dlf);
			pango_layout_get_pixel_size (pl, &tw, &th);
			cairo_move_to (cr, 9.5 + dbx - tw * .5, YPOS (68) - th);
			pango_cairo_show_layout (cr, pl);
			g_object_unref (pl);
		}
	}

	/* numeric display */
	if (1) {
		int          tw, th;
		char         txt[16];
		int          y0 = top;
		PangoLayout* pl = pango_cairo_create_layout (cr);
		pango_layout_set_font_description (pl, ui->font[2]);

		snprintf (txt, 16, "%5.1f dB", robtk_dial_get_value (ui->spn_ctrl[0]));
		cairo_set_source_rgb (cr, .6, .6, .1);
		pango_layout_set_text (pl, txt, -1);
		pango_layout_get_pixel_size (pl, &tw, &th);
		cairo_move_to (cr, DEFLECT (-1) - tw, y0 + th * .5);
		pango_cairo_show_layout (cr, pl);
		y0 += th;

		snprintf (txt, 16, "%5.1f dB", robtk_dial_get_value (ui->spn_ctrl[1]));
		cairo_set_source_rgb (cr, .7, .2, .2);
		pango_layout_set_text (pl, txt, -1);
		pango_layout_get_pixel_size (pl, &tw, &th);
		cairo_move_to (cr, DEFLECT (-1) - tw, y0 + th * .5);
		pango_cairo_show_layout (cr, pl);
		y0 += th;

		const float val = gui_to_ctrl (2, robtk_dial_get_value (ui->spn_ctrl[2]));
		format_msec (txt, val);

		cairo_set_source_rgb (cr, .2, .2, .7);
		pango_layout_set_text (pl, txt, -1);
		pango_layout_get_pixel_size (pl, &tw, &th);
		cairo_move_to (cr, DEFLECT (-1) - tw, y0 + th * .5);
		pango_cairo_show_layout (cr, pl);

		g_object_unref (pl);
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////

static RobWidget*
toplevel (PLimUI* ui, void* const top)
{
	/* main widget: layout */
	ui->rw = rob_hbox_new (FALSE, 2);
	robwidget_make_toplevel (ui->rw, top);
	robwidget_toplevel_enable_scaling (ui->rw);

	ui->font[0] = pango_font_description_from_string ("Mono 9px");
	ui->font[1] = pango_font_description_from_string ("Mono 10px");
	ui->font[2] = pango_font_description_from_string ("Mono Bold 12px");

	prepare_faceplates (ui);

	/* graph display */
	ui->m0 = robwidget_new (ui);
	robwidget_set_alignment (ui->m0, .5, .5);
	robwidget_set_expose_event (ui->m0, m0_expose_event);
	robwidget_set_size_request (ui->m0, m0_size_request);
	robwidget_set_size_allocate (ui->m0, m0_size_allocate);

	ui->ctbl      = rob_table_new (/*rows*/ 2, /*cols*/ 3, FALSE);
	ui->ctbl->top = (void*)ui;

#define GSP_W(PTR) robtk_dial_widget (PTR)
#define GLB_W(PTR) robtk_lbl_widget (PTR)
#define GSL_W(PTR) robtk_select_widget (PTR)
#define GBT_W(PTR) robtk_cbtn_widget (PTR)

	for (uint32_t i = 0; i < 3; ++i) {
		ui->lbl_ctrl[i] = robtk_lbl_new (ctrl_range[i].name);
		ui->spn_ctrl[i] = robtk_dial_new_with_size (
		    k_min (i), k_max (i), k_step (i),
		    GED_WIDTH + 8, GED_HEIGHT + 20, GED_CX + 4, GED_CY + 15, GED_RADIUS);
		ui->spn_ctrl[i]->with_scroll_accel = false;

		robtk_dial_set_callback (ui->spn_ctrl[i], cb_spn_ctrl, ui);
		robtk_dial_set_value (ui->spn_ctrl[i], ctrl_to_gui (i, ctrl_range[i].dflt));
		robtk_dial_set_default (ui->spn_ctrl[i], ctrl_to_gui (i, ctrl_range[i].dflt));
		robtk_dial_set_scroll_mult (ui->spn_ctrl[i], ctrl_range[i].mult);

		if (ui->touch) {
			robtk_dial_set_touch (ui->spn_ctrl[i], ui->touch->touch, ui->touch->handle, PLIM_GAIN + i);
		}

		robtk_dial_set_scaled_surface_scale (ui->spn_ctrl[i], ui->dial_bg[i], 2.0);

		rob_table_attach (ui->ctbl, GSP_W (ui->spn_ctrl[i]), i, i + 1, 0, 1, 4, 0, RTK_EXANDF, RTK_SHRINK);

		if (i != 1) {
			robtk_lbl_annotation_callback (ui->lbl_ctrl[i], ttip_handler, ui);
			rob_table_attach (ui->ctbl, GLB_W (ui->lbl_ctrl[i]), i, i + 1, 1, 2, 4, 0, RTK_EXANDF, RTK_SHRINK);
		}
	}

	ui->spn_ctrl[2]->displaymode = 3; // use dot
	robtk_dial_set_detent_default (ui->spn_ctrl[0], true);

	/* these numerics are meaningful */
	robtk_dial_annotation_callback (ui->spn_ctrl[0], dial_annotation_db, ui);
	robtk_dial_annotation_callback (ui->spn_ctrl[1], dial_annotation_db, ui);
	robtk_dial_annotation_callback (ui->spn_ctrl[2], dial_annotation_tm, ui);

	/* add true-peak button/label */
	ui->btn_truepeak = robtk_cbtn_new (THRESHOLD_ABBREV " dBFS", GBT_LED_RIGHT, false);
	robtk_cbtn_set_callback (ui->btn_truepeak, cb_btn_truepeak, ui);
	robtk_cbtn_set_color_on (ui->btn_truepeak, .1, .3, .8);
	robtk_cbtn_set_color_off (ui->btn_truepeak, .1, .1, .3);
	robtk_cbtn_annotation_callback (ui->btn_truepeak, ttip_handler, ui);
	rob_table_attach (ui->ctbl, GBT_W (ui->btn_truepeak), 1, 2, 1, 2, 4, 0, RTK_EXANDF, RTK_SHRINK);

	/* some custom colors */
	{
		const float c_bg[4] = { .6, .6, .1, 1.0 };
		create_dial_pattern (ui->spn_ctrl[0], c_bg);
		ui->spn_ctrl[0]->dcol[0][0] =
		    ui->spn_ctrl[0]->dcol[0][1] =
		        ui->spn_ctrl[0]->dcol[0][2] = .05;
	}
	{
		const float c_bg[4] = { .7, .2, .2, 1.0 };
		create_dial_pattern (ui->spn_ctrl[1], c_bg);
		ui->spn_ctrl[1]->dcol[0][0] =
		    ui->spn_ctrl[1]->dcol[0][1] =
		        ui->spn_ctrl[1]->dcol[0][2] = .05;
	}
	{
		const float c_bg[4] = { .2, .2, .7, 1.0 };
		create_dial_pattern (ui->spn_ctrl[2], c_bg);
		ui->spn_ctrl[2]->dcol[0][0] =
		    ui->spn_ctrl[2]->dcol[0][1] =
		        ui->spn_ctrl[2]->dcol[0][2] = .05;
	}

	/* top-level packing */
	rob_hbox_child_pack (ui->rw, ui->ctbl, FALSE, TRUE);
	rob_hbox_child_pack (ui->rw, ui->m0, TRUE, TRUE);
	return ui->rw;
}

static void
gui_cleanup (PLimUI* ui)
{
	for (int i = 0; i < 3; ++i) {
		robtk_dial_destroy (ui->spn_ctrl[i]);
		robtk_lbl_destroy (ui->lbl_ctrl[i]);
		cairo_surface_destroy (ui->dial_bg[i]);
	}
	robtk_cbtn_destroy (ui->btn_truepeak);

	pango_font_description_free (ui->font[0]);
	pango_font_description_free (ui->font[1]);
	pango_font_description_free (ui->font[2]);

	if (ui->m_fg) {
		cairo_pattern_destroy (ui->m_fg);
	}
	if (ui->m_bg) {
		cairo_pattern_destroy (ui->m_bg);
	}

	robwidget_destroy (ui->m0);
	rob_table_destroy (ui->ctbl);
	rob_box_destroy (ui->rw);
}

/******************************************************************************
 * RobTk + LV2
 */

static void
tx_state (PLimUI* ui)
{
	uint8_t obj_buf[1024];
	lv2_atom_forge_set_buffer (&ui->forge, obj_buf, 1024);
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time (&ui->forge, 0);
	LV2_Atom* msg = (LV2_Atom*)x_forge_object (&ui->forge, &frame, 1, ui->uris.state);

	lv2_atom_forge_property_head (&ui->forge, ui->uris.s_uiscale, 0);
	lv2_atom_forge_float (&ui->forge, ui->rw->widget_scale);

	lv2_atom_forge_pop (&ui->forge, &frame);
	ui->write (ui->controller, PLIM_ATOM_CONTROL, lv2_atom_total_size (msg), ui->uris.atom_eventTransfer, msg);
}

#define LVGL_RESIZEABLE

static void
ui_enable (LV2UI_Handle handle)
{
	PLimUI* ui = (PLimUI*)handle;

	uint8_t obj_buf[64];
	lv2_atom_forge_set_buffer (&ui->forge, obj_buf, 64);
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time (&ui->forge, 0);
	LV2_Atom* msg = (LV2_Atom*)x_forge_object (&ui->forge, &frame, 1, ui->uris.ui_on);
	lv2_atom_forge_pop (&ui->forge, &frame);
	ui->write (ui->controller, PLIM_ATOM_CONTROL, lv2_atom_total_size (msg), ui->uris.atom_eventTransfer, msg);
}

static void
ui_disable (LV2UI_Handle handle)
{
	PLimUI* ui = (PLimUI*)handle;

	tx_state (ui); // too late?

	uint8_t obj_buf[64];
	lv2_atom_forge_set_buffer (&ui->forge, obj_buf, 64);
	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_frame_time (&ui->forge, 0);
	LV2_Atom* msg = (LV2_Atom*)x_forge_object (&ui->forge, &frame, 1, ui->uris.ui_off);
	lv2_atom_forge_pop (&ui->forge, &frame);
	ui->write (ui->controller, PLIM_ATOM_CONTROL, lv2_atom_total_size (msg), ui->uris.atom_eventTransfer, msg);
}

static enum LVGLResize
plugin_scale_mode (LV2UI_Handle handle)
{
	return LVGL_LAYOUT_TO_FIT;
}

static LV2UI_Handle
instantiate (
    void* const               ui_toplevel,
    const LV2UI_Descriptor*   descriptor,
    const char*               plugin_uri,
    const char*               bundle_path,
    LV2UI_Write_Function      write_function,
    LV2UI_Controller          controller,
    RobWidget**               widget,
    const LV2_Feature* const* features)
{
	PLimUI* ui = (PLimUI*)calloc (1, sizeof (PLimUI));
	if (!ui) {
		return NULL;
	}

	if (strcmp (plugin_uri, RTK_URI "mono") && strcmp (plugin_uri, RTK_URI "stereo")) {
		free (ui);
		return NULL;
	}

	for (int i = 0; features[i]; ++i) {
		if (!strcmp (features[i]->URI, LV2_URID_URI "#map")) {
			ui->map = (LV2_URID_Map*)features[i]->data;
		} else if (!strcmp(features[i]->URI, LV2_UI__touch)) {
			ui->touch = (LV2UI_Touch*)features[i]->data;
		}
	}

	if (!ui->map) {
		fprintf (stderr, "dpl.lv2 UI: Host does not support urid:map\n");
		free (ui);
		return NULL;
	}

	ui->nfo             = robtk_info (ui_toplevel);
	ui->write           = write_function;
	ui->controller      = controller;
	ui->disable_signals = true;

	map_plim_uris (ui->map, &ui->uris);
	lv2_atom_forge_init (&ui->forge, ui->map);
	interpolate_fg_bg (c_dlf, .2);

	*widget             = toplevel (ui, ui_toplevel);
	ui->disable_signals = false;
	return ui;
}

static void
cleanup (LV2UI_Handle handle)
{
	PLimUI* ui = (PLimUI*)handle;
	gui_cleanup (ui);
	free (ui);
}

/* receive information from DSP */
static void
port_event (LV2UI_Handle handle,
            uint32_t     port_index,
            uint32_t     buffer_size,
            uint32_t     format,
            const void*  buffer)
{
	PLimUI* ui = (PLimUI*)handle;

	if (format == ui->uris.atom_eventTransfer && port_index == PLIM_ATOM_NOTIFY) {
		LV2_Atom* atom = (LV2_Atom*)buffer;
		if (atom->type != ui->uris.atom_Blank && atom->type != ui->uris.atom_Object) {
			return;
		}
		LV2_Atom_Object* obj = (LV2_Atom_Object*)atom;

		if (obj->body.otype == ui->uris.state) {
			const LV2_Atom* a0 = NULL;
			if (1 == lv2_atom_object_get (obj, ui->uris.s_uiscale, &a0, NULL) && a0) {
				const float sc = ((LV2_Atom_Float*)a0)->body;
				if (sc != ui->rw->widget_scale && sc >= 1.0 && sc <= 2.0) {
					robtk_queue_scale_change (ui->rw, sc);
				}
			}
		} else if (obj->body.otype == ui->uris.history) {
			const LV2_Atom* a0 = NULL;
			const LV2_Atom* a1 = NULL;
			const LV2_Atom* a2 = NULL;
			if (3 == lv2_atom_object_get (obj, ui->uris.position, &a0, ui->uris.minvals, &a1, ui->uris.maxvals, &a2, NULL) && a0 && a1 && a2 && a0->type == ui->uris.atom_Int && a1->type == ui->uris.atom_Vector && a2->type == ui->uris.atom_Vector) {
				ui->_hist = ((LV2_Atom_Int*)a0)->body;

				LV2_Atom_Vector* mins = (LV2_Atom_Vector*)LV2_ATOM_BODY (a1);
				assert (mins->atom.type == ui->uris.atom_Float);
				assert (HISTLEN == (a2->size - sizeof (LV2_Atom_Vector_Body)) / mins->atom.size);

				LV2_Atom_Vector* maxs = (LV2_Atom_Vector*)LV2_ATOM_BODY (a2);
				assert (maxs->atom.type == ui->uris.atom_Float);
				assert (HISTLEN == (a2->size - sizeof (LV2_Atom_Vector_Body)) / maxs->atom.size);

				memcpy (ui->_min, (float*)LV2_ATOM_BODY (&mins->atom), sizeof (float) * HISTLEN);
				memcpy (ui->_max, (float*)LV2_ATOM_BODY (&maxs->atom), sizeof (float) * HISTLEN);
				queue_draw (ui->m0);
			}
		}
		return;
	}

	if (format != 0) {
		return;
	}

	if (port_index == PLIM_LEVEL) {
		ui->_peak = *(float*)buffer;
		queue_draw (ui->m0);
	} else if (port_index == PLIM_TRUEPEAK) {
		ui->disable_signals = true;
		robtk_cbtn_set_active (ui->btn_truepeak, (*(float*)buffer) > 0);
		robtk_cbtn_set_text (ui->btn_truepeak, (*(float*)buffer) > 0 ? THRESHOLD_ABBREV " dBTP" : THRESHOLD_ABBREV " dBFS");
		ui->disable_signals = false;
	} else if (port_index >= PLIM_GAIN && port_index <= PLIM_RELEASE) {
		const float v       = *(float*)buffer;
		ui->disable_signals = true;
		uint32_t ctrl       = port_index - PLIM_GAIN;
		robtk_dial_set_value (ui->spn_ctrl[ctrl], ctrl_to_gui (ctrl, v));
		ui->disable_signals = false;
	}
}

static const void*
extension_data (const char* uri)
{
	return NULL;
}
