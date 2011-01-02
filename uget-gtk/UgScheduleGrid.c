/*
 *
 *   Copyright (C) 2005-2011 by Raymond Huang
 *   plushuang at users.sourceforge.net
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  ---
 *
 *  In addition, as a special exception, the copyright holders give
 *  permission to link the code of portions of this program with the
 *  OpenSSL library under certain conditions as described in each
 *  individual source file, and distribute linked combinations
 *  including the two.
 *  You must obey the GNU Lesser General Public License in all respects
 *  for all of the code used other than OpenSSL.  If you modify
 *  file(s) with this exception, you may extend this exception to your
 *  version of the file(s), but you are not obligated to do so.  If you
 *  do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source
 *  files in the program, then also delete it here.
 *
 */

#include <memory.h>
#include <UgScheduleGrid.h>

#include <glib/gi18n.h>

#define	GRID_FONT_SIZE		10

#define	GRID_WIDTH			14
#define	GRID_HEIGHT			16
#define	GRID_WIDTH_LINE		(GRID_WIDTH  + 1)
#define	GRID_HEIGHT_LINE	(GRID_HEIGHT + 1)
#define	GRID_WIDTH_ALL		(GRID_WIDTH_LINE  * 24 + 1)
#define	GRID_HEIGHT_ALL		(GRID_HEIGHT_LINE *  7 + 1)

#define	COLOR_DISABLE_R		0.5
#define	COLOR_DISABLE_G		0.5
#define	COLOR_DISABLE_B		0.5

static const gdouble	colors[UG_SCHEDULE_N_STATE][3] =
{
	{1.0,   1.0,   1.0},		// turn off
	{1.0,   0.752, 0.752},		// reserve - upload only
	{0.552, 0.807, 0.552},		// limited speed
	{0.0,   0.658, 0.0},		// full speed
};

static const gchar*	week_days[7] =
{
	N_("Mon"),
	N_("Tue"),
	N_("Wed"),
	N_("Thu"),
	N_("Fri"),
	N_("Sat"),
	N_("Sun"),
};

// grid one
static GtkWidget*	ug_grid_one_new (const gdouble* rgb_array);
static gboolean		ug_grid_one_expose (GtkWidget* widget, GdkEventExpose* event, const gdouble* rgb_array);
// signal handler
static void		on_enable_toggled (GtkToggleButton* togglebutton, struct UgScheduleGrid* sgrid);
static gboolean	on_expose_event (GtkWidget* widget, GdkEventExpose* event, struct UgScheduleGrid* sgrid);
static gboolean on_button_press_event (GtkWidget* widget, GdkEventMotion* event, struct UgScheduleGrid* sgrid);
static gboolean on_motion_notify_event (GtkWidget* widget, GdkEventMotion* event, struct UgScheduleGrid* sgrid);


void	ug_schedule_grid_init (struct UgScheduleGrid* sgrid)
{
	GtkWidget*	widget;
	GtkBox*		hbox;
	GtkBox*		vbox;

	sgrid->self = gtk_vbox_new (FALSE, 0);
	vbox = (GtkBox*) sgrid->self;

	// Enable Scheduler
	hbox = (GtkBox*) gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start (vbox, (GtkWidget*)hbox, FALSE, FALSE, 2);
	widget = gtk_check_button_new_with_mnemonic (_("_Enable Scheduler"));
	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 2);
	gtk_box_pack_start (hbox, gtk_hseparator_new (), TRUE, TRUE, 2);
	g_signal_connect (widget, "toggled",
			G_CALLBACK (on_enable_toggled), sgrid);
	sgrid->enable = widget;

	// grid
	widget = gtk_drawing_area_new ();
	gtk_box_pack_start (vbox, widget, FALSE, FALSE, 2);
//	gtk_widget_set_has_window (widget, FALSE);
	gtk_widget_set_size_request (widget,
			GRID_WIDTH_ALL + 32, GRID_HEIGHT_ALL);
	gtk_widget_add_events (widget, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
	g_signal_connect (widget, "expose-event",
			G_CALLBACK (on_expose_event), sgrid);
	g_signal_connect (widget, "button-press-event",
			G_CALLBACK (on_button_press_event), sgrid);
	g_signal_connect (widget, "motion-notify-event",
			G_CALLBACK (on_motion_notify_event), sgrid);
	sgrid->grid = widget;

	// ----------------------------------------------------
	// tips
	sgrid->tips_box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (vbox, (GtkWidget*)sgrid->tips_box, FALSE, FALSE, 2);
	vbox = (GtkBox*) sgrid->tips_box;
	// tips gap
	hbox = (GtkBox*) gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start (vbox, (GtkWidget*)hbox, FALSE, FALSE, 2);
	widget = gtk_label_new ("");
	gtk_widget_set_size_request (widget, 24, 0);
	gtk_box_pack_start (hbox, widget , FALSE, TRUE, 0);
	sgrid->tips_gap = widget;
	// tips
	widget = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);	// left, center
	gtk_widget_set_size_request (widget, 10, 16);
	gtk_box_pack_start (hbox, widget, TRUE, TRUE, 2);
	sgrid->tips = GTK_LABEL (widget);
	// Full speed
	widget = ug_grid_one_new (colors[UG_SCHEDULE_STATE_FULL]);
	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 2);
	widget = gtk_label_new (_("Full speed"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);	// left, center
	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 2);
	// Trun off
	widget = ug_grid_one_new (colors[UG_SCHEDULE_STATE_OFF]);
	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 2);
	widget = gtk_label_new (_("Trun off"));
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);	// left, center
	gtk_box_pack_start (hbox, widget, FALSE, FALSE, 2);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sgrid->enable), FALSE);
	gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON (sgrid->enable));
	gtk_widget_show_all (sgrid->self);
}

void	ug_schedule_grid_get (struct UgScheduleGrid* sgrid, UgetGtkSetting* setting)
{
	memcpy (setting->scheduler.state, sgrid->state, sizeof (setting->scheduler.state));
	setting->scheduler.enable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sgrid->enable));
}

void	ug_schedule_grid_set (struct UgScheduleGrid* sgrid, UgetGtkSetting* setting)
{
	memcpy (sgrid->state, setting->scheduler.state, sizeof (sgrid->state));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sgrid->enable), setting->scheduler.enable);
	gtk_toggle_button_toggled (GTK_TOGGLE_BUTTON (sgrid->enable));
}


// ----------------------------------------------------------------------------
// signal handler
//
static void	on_enable_toggled (GtkToggleButton* togglebutton, struct UgScheduleGrid* sgrid)
{
	gboolean	active;

	active = gtk_toggle_button_get_active (togglebutton);
	gtk_widget_set_sensitive (sgrid->grid, active);
	gtk_widget_set_sensitive (sgrid->tips_box, active);
}

static gboolean	on_expose_event (GtkWidget* widget, GdkEventExpose* event, struct UgScheduleGrid* sgrid)
{
	gchar*		string;
	gboolean	sensitive;
	guint		y, x;
	gdouble		cy, cx, ox;
	cairo_t*	cr;
	cairo_text_extents_t	extents;

	cr = gdk_cairo_create (gtk_widget_get_window (widget));
	cairo_set_line_width (cr, 1);
	sensitive = gtk_widget_get_sensitive (widget);

	// week days
	ox = 0.0;		// x offset
	cairo_set_font_size (cr, GRID_FONT_SIZE);
	for (cy = 1.5, y = 0;  y < 7;  y++, cy+=GRID_HEIGHT_LINE) {
		string = gettext (week_days[y]);
		cairo_text_extents (cr, string, &extents);
		cairo_move_to (cr, 1 + 0.5, cy + extents.height);
		cairo_show_text (cr, string);
		if (extents.width + 4 > ox)
			ox = extents.width + 4;
	}
	if (sgrid->grid_offset == 0) {
		sgrid->grid_offset = ox;
		gtk_widget_set_size_request (sgrid->tips_gap, ox, 0);
	}
	// draw grid columns
	for (cx = 0.5;  cx <= GRID_WIDTH_ALL;  cx += GRID_WIDTH_LINE) {
		cairo_move_to (cr, ox + cx, 0 + 0.5);
		cairo_line_to (cr, ox + cx, GRID_HEIGHT_ALL - 1.0 + 0.5);
	}
	// draw grid rows
	for (cy = 0.5;  cy <= GRID_HEIGHT_ALL;  cy += GRID_HEIGHT_LINE) {
		cairo_move_to (cr, ox + 0.5, cy);
		cairo_line_to (cr, ox + GRID_WIDTH_ALL - 1.0 + 0.5, cy);
	}
	cairo_stroke (cr);

	// fill grid
	if (sensitive == FALSE) {
		cairo_set_source_rgb (cr,
				COLOR_DISABLE_R,
				COLOR_DISABLE_G,
				COLOR_DISABLE_B);
	}
	for (cy = 1.5, y = 0;  y < 7;  y++, cy+=GRID_HEIGHT_LINE) {
		for (cx = 1.5+ox, x = 0;  x < 24;  x++, cx+=GRID_WIDTH_LINE) {
			if (sensitive) {
				cairo_set_source_rgb (cr,
						colors [sgrid->state[y][x]][0],
						colors [sgrid->state[y][x]][1],
						colors [sgrid->state[y][x]][2]);
			}
			cairo_rectangle (cr,
					cx,
					cy,
					GRID_WIDTH  - 0.5,
					GRID_HEIGHT - 0.5);
			cairo_fill (cr);
		}
	}

	cairo_destroy (cr);
	return TRUE;
}

static gboolean on_button_press_event (GtkWidget *widget, GdkEventMotion *event, struct UgScheduleGrid* sgrid)
{
	gint			x, y;
	cairo_t*		cr;
	UgScheduleState	state;

	x  = (event->x - sgrid->grid_offset) / GRID_WIDTH_LINE;
	y  =  event->y / GRID_HEIGHT_LINE;
	if (x < 0 || y < 0 || x >= 24 || y >= 7)
		return FALSE;

	state = (sgrid->state[y][x]) ? UG_SCHEDULE_STATE_OFF : UG_SCHEDULE_STATE_FULL;
//	state = sgrid->state[y][x] + 1;
//	if (state > UG_SCHEDULE_STATE_FULL)
//		state = UG_SCHEDULE_STATE_OFF;
	sgrid->state[y][x] = state;
	sgrid->last_state = state;
	// cairo
	cr = gdk_cairo_create (gtk_widget_get_window (widget));
	cairo_set_source_rgb (cr,
			colors [state][0],
			colors [state][1],
			colors [state][2]);
	cairo_rectangle (cr,
			(gdouble)x * GRID_WIDTH_LINE  + 1.0 + 0.5 + sgrid->grid_offset,
			(gdouble)y * GRID_HEIGHT_LINE + 1.0 + 0.5,
			GRID_WIDTH  - 0.5,
			GRID_HEIGHT - 0.5);
	cairo_fill (cr);
	cairo_destroy (cr);

	return TRUE;
}

static gboolean on_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, struct UgScheduleGrid* sgrid)
{
	gint			x, y;
	gchar*			string;
	cairo_t*		cr;
	GdkWindow*		gdkwin;
	GdkModifierType	mod;
	UgScheduleState	state;

	gdkwin = gtk_widget_get_window (widget);
	gdk_window_get_pointer (gdkwin, &x, &y, &mod);
	x -= sgrid->grid_offset;
	x /= GRID_WIDTH_LINE;
	y /= GRID_HEIGHT_LINE;
	if (x < 0 || y < 0 || x >= 24 || y >= 7) {
		// clear tips
		gtk_label_set_text (sgrid->tips, "");
		return FALSE;
	}
	// update tips
	string = g_strdup_printf ("%s, %.2d:00 - %.2d:59",
			gettext (week_days[y]), x, x);
	gtk_label_set_text (sgrid->tips, string);
	g_free (string);
	// if no button press
	if ((mod & GDK_BUTTON1_MASK) == 0)
		return FALSE;

	state = sgrid->last_state;
	sgrid->state[y][x] = state;
	// cairo
	cr = gdk_cairo_create (gdkwin);
	cairo_rectangle (cr,
			sgrid->grid_offset, 0,
			GRID_WIDTH_ALL, GRID_HEIGHT_ALL);
	cairo_clip (cr);
	cairo_set_source_rgb (cr,
			colors [state][0],
			colors [state][1],
			colors [state][2]);
	cairo_rectangle (cr,
			(gdouble)x * GRID_WIDTH_LINE  + 1.0 + 0.5 + sgrid->grid_offset,
			(gdouble)y * GRID_HEIGHT_LINE + 1.0 + 0.5,
			GRID_WIDTH  - 0.5,
			GRID_HEIGHT - 0.5);
	cairo_fill (cr);
	cairo_destroy (cr);

	return TRUE;
}


// ----------------------------------------------------------------------------
// grid one
//
static GtkWidget*	ug_grid_one_new (const gdouble* rgb_array)
{
	GtkWidget*	widget;

	widget = gtk_drawing_area_new ();
	gtk_widget_set_has_window (widget, FALSE);
	gtk_widget_set_size_request (widget, GRID_WIDTH + 2, GRID_HEIGHT + 2);
	gtk_widget_add_events (widget, GDK_POINTER_MOTION_MASK);
	g_signal_connect (widget, "expose-event",
			G_CALLBACK (ug_grid_one_expose), (gpointer) rgb_array);

	return widget;
}

static gboolean		ug_grid_one_expose (GtkWidget* widget, GdkEventExpose* event, const gdouble* rgb_array)
{
	cairo_t*		cr;
	GtkAllocation	allocation;
	gdouble			x, y, width, height;

	cr = gdk_cairo_create (gtk_widget_get_window (widget));
	gtk_widget_get_allocation (widget, &allocation);
	x = (gdouble) allocation.x + 0.5;
	y = (gdouble) allocation.y + 0.5;
	width  = (gdouble) allocation.width;
	height = (gdouble) allocation.height;
	cairo_set_line_width (cr, 1);
	cairo_rectangle (cr, x, y, width, height);
	cairo_stroke (cr);
	if (gtk_widget_get_sensitive (widget)) {
		cairo_set_source_rgb (cr,
				rgb_array [0],
				rgb_array [1],
				rgb_array [2]);
	}
	else {
		cairo_set_source_rgb (cr,
				COLOR_DISABLE_R,
				COLOR_DISABLE_G,
				COLOR_DISABLE_B);
	}
	cairo_rectangle (cr, x + 1.0, y + 1.0, width - 2.0, height - 2.0);
	cairo_fill (cr);

	cairo_destroy (cr);
	return TRUE;
}

