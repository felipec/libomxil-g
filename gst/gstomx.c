/*
 * Copyright (C) 2007-2008 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "gstomx.h"
#include "gstomx_avcenc.h"

#include <stdbool.h>

GST_DEBUG_CATEGORY (gstomx_debug);

#define DEFAULT_RANK GST_RANK_PRIMARY

static gboolean
plugin_init (GstPlugin *plugin)
{
    GST_DEBUG_CATEGORY_INIT (gstomx_debug, "omx", 0, "OpenMAX");

    g_omx_init ();

    if (!gst_element_register (plugin, "omx_avcenc", DEFAULT_RANK, GST_OMX_AVCENC_TYPE))
        return false;

    return true;
}

GstPluginDesc gst_plugin_desc = {
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "omx",
    (gchar *) "OpenMAX IL",
    plugin_init,
    VERSION,
    "LGPL",
    "source",
    "package",
    "origin",
    GST_PADDING_INIT
};
