/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2012 Red Hat, Inc.
 */

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <nm-setting-connection.h>
#include <nm-setting-wimax.h>
#include <nm-device-wimax.h>
#include <nm-utils.h>

#include "page-wimax.h"

G_DEFINE_TYPE (CEPageWimax, ce_page_wimax, CE_TYPE_PAGE)

#define CE_PAGE_WIMAX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CE_TYPE_PAGE_WIMAX, CEPageWimaxPrivate))

typedef struct {
	NMSettingWimax *setting;

	GtkEntry *name;
	GtkComboBoxText *device_mac;  /* Permanent MAC of the device */

	gboolean disposed;
} CEPageWimaxPrivate;

static void
wimax_private_init (CEPageWimax *self)
{
	CEPageWimaxPrivate *priv = CE_PAGE_WIMAX_GET_PRIVATE (self);
	GtkBuilder *builder;
	GtkWidget *align;
	GtkLabel *label;

	builder = CE_PAGE (self)->builder;

	priv->name = GTK_ENTRY (gtk_builder_get_object (builder, "wimax_name"));

	priv->device_mac = GTK_COMBO_BOX_TEXT (gtk_combo_box_text_new_with_entry ());
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->device_mac), 0);
	gtk_widget_set_tooltip_text (GTK_WIDGET (priv->device_mac),
	                             _("This option locks this connection to the network device specified by its permanent MAC address entered here.  Example: 00:11:22:33:44:55"));

	align = GTK_WIDGET (gtk_builder_get_object (builder, "wimax_device_mac_alignment"));
	gtk_container_add (GTK_CONTAINER (align), GTK_WIDGET (priv->device_mac));
	gtk_widget_show_all (GTK_WIDGET (priv->device_mac));

	/* Set mnemonic widget for device MAC label */
	label = GTK_LABEL (gtk_builder_get_object (builder, "wimax_device_mac_label"));
	gtk_label_set_mnemonic_widget (label, GTK_WIDGET (priv->device_mac));
}

static void
populate_ui (CEPageWimax *self)
{
	CEPageWimaxPrivate *priv = CE_PAGE_WIMAX_GET_PRIVATE (self);
	NMSettingWimax *setting = priv->setting;
	char **mac_list;
	const GByteArray *s_mac;
	char *s_mac_str;

	gtk_entry_set_text (priv->name, nm_setting_wimax_get_network_name (setting));
	g_signal_connect_swapped (priv->name, "changed", G_CALLBACK (ce_page_changed), self);

	/* Device MAC address */
	mac_list = ce_page_get_mac_list (CE_PAGE (self), NM_TYPE_DEVICE_WIMAX,
	                                 NM_DEVICE_WIMAX_HW_ADDRESS);
	s_mac = nm_setting_wimax_get_mac_address (setting);
	s_mac_str = s_mac ? nm_utils_hwaddr_ntoa (s_mac->data, ARPHRD_ETHER) : NULL;
	ce_page_setup_mac_combo (CE_PAGE (self), GTK_COMBO_BOX (priv->device_mac),
	                         s_mac_str, mac_list);
	g_free (s_mac_str);
	g_strfreev (mac_list);
	g_signal_connect_swapped (priv->device_mac, "changed", G_CALLBACK (ce_page_changed), self);
}

static void
finish_setup (CEPageWimax *self, gpointer unused, GError *error, gpointer user_data)
{
	if (error)
		return;

	populate_ui (self);
}

CEPage *
ce_page_wimax_new (NMConnection *connection,
                   GtkWindow *parent_window,
                   NMClient *client,
                   NMRemoteSettings *settings,
                   const char **out_secrets_setting_name,
                   GError **error)
{
	CEPageWimax *self;
	CEPageWimaxPrivate *priv;

	g_return_val_if_fail (NM_IS_CONNECTION (connection), NULL);

	self = CE_PAGE_WIMAX (ce_page_new (CE_TYPE_PAGE_WIMAX,
	                                   connection,
	                                   parent_window,
	                                   client,
	                                   settings,
	                                   UIDIR "/ce-page-wimax.ui",
	                                   "WimaxPage",
	                                   _("WiMAX")));
	if (!self) {
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC,
		                     _("Could not load WiMAX user interface."));
		return NULL;
	}

	wimax_private_init (self);
	priv = CE_PAGE_WIMAX_GET_PRIVATE (self);

	priv->setting = nm_connection_get_setting_wimax (connection);
	if (!priv->setting) {
		priv->setting = NM_SETTING_WIMAX (nm_setting_wimax_new ());
		nm_connection_add_setting (connection, NM_SETTING (priv->setting));
	}

	g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

	return CE_PAGE (self);
}

static void
ui_to_setting (CEPageWimax *self)
{
	CEPageWimaxPrivate *priv = CE_PAGE_WIMAX_GET_PRIVATE (self);
	const char *name;
	GByteArray *device_mac = NULL;
	GtkWidget *entry;

	name = gtk_entry_get_text (priv->name);

	entry = gtk_bin_get_child (GTK_BIN (priv->device_mac));
	if (entry)
		device_mac = ce_page_entry_to_mac (GTK_ENTRY (entry), ARPHRD_ETHER, NULL);

	g_object_set (priv->setting,
	              NM_SETTING_WIMAX_NETWORK_NAME, name,
	              NM_SETTING_WIMAX_MAC_ADDRESS, device_mac,
	              NULL);

	if (device_mac)
		g_byte_array_free (device_mac, TRUE);
}

static gboolean
validate (CEPage *page, NMConnection *connection, GError **error)
{
	CEPageWimax *self = CE_PAGE_WIMAX (page);
	CEPageWimaxPrivate *priv = CE_PAGE_WIMAX_GET_PRIVATE (self);
	const char *name;
	gboolean invalid = FALSE;
	GByteArray *ignore;
	GtkWidget *entry;

	name = gtk_entry_get_text (priv->name);
	if (!*name)
		return FALSE;

	entry = gtk_bin_get_child (GTK_BIN (priv->device_mac));
	if (entry) {
		ignore = ce_page_entry_to_mac (GTK_ENTRY (entry), ARPHRD_ETHER, &invalid);
		if (invalid)
			return FALSE;
		if (ignore)
			g_byte_array_free (ignore, TRUE);
	}

	ui_to_setting (self);
	return TRUE;
}

static void
ce_page_wimax_init (CEPageWimax *self)
{
}

static void
ce_page_wimax_class_init (CEPageWimaxClass *wimax_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (wimax_class);
	CEPageClass *parent_class = CE_PAGE_CLASS (wimax_class);

	g_type_class_add_private (object_class, sizeof (CEPageWimaxPrivate));

	/* virtual methods */
	parent_class->validate = validate;
}


void
wimax_connection_new (GtkWindow *parent,
                      const char *detail,
                      NMRemoteSettings *settings,
                      PageNewConnectionResultFunc result_func,
                      gpointer user_data)
{
	NMConnection *connection;
	NMSetting *s_wimax;

	connection = ce_page_new_connection (_("WiMAX connection %d"),
	                                     NM_SETTING_WIMAX_SETTING_NAME,
	                                     TRUE,
	                                     settings,
	                                     user_data);
	s_wimax = nm_setting_wimax_new ();
	nm_connection_add_setting (connection, s_wimax);

	(*result_func) (connection, FALSE, NULL, user_data);
}


