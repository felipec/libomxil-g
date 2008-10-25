/*
 * Copyright (C) 2008 Nokia Corporation.
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

#include <OMX_Core.h>
#include <OMX_Component.h>

#include "core.h"

#include <glib.h>
#include <stdlib.h> /* For calloc, free */

static GHashTable *components;

#define register(x) { \
          extern ComponentInfo x##_info; \
          g_hash_table_insert (components, "OMX.g." #x, &x##_info); }

OMX_ERRORTYPE
OMX_Init (void)
{
    if (!g_thread_supported ())
    {
        g_thread_init (NULL);
    }

    components = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

    register (dummy);
    register (x264enc);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE
OMX_Deinit (void)
{
    g_hash_table_destroy (components);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE
OMX_GetHandle (OMX_HANDLETYPE *handle,
               OMX_STRING component_name,
               OMX_PTR data,
               OMX_CALLBACKTYPE *callbacks)
{
    ComponentInfo *info;
    info = g_hash_table_lookup (components, component_name);
    if (info)
    {
        OMX_COMPONENTTYPE *comp;
        OMX_ERRORTYPE error;

        comp = calloc (1, sizeof (OMX_COMPONENTTYPE));
        comp->nSize = sizeof (OMX_COMPONENTTYPE);

        error = info->init (comp);
        if (error != OMX_ErrorNone)
        {
            free (comp);
            return error;
        }

        comp->SetCallbacks ((OMX_HANDLETYPE) comp, callbacks, data);
        *handle = comp;

        return OMX_ErrorNone;
    }
    return OMX_ErrorInvalidComponentName;
}

OMX_ERRORTYPE
OMX_FreeHandle (OMX_HANDLETYPE handle)
{
    OMX_COMPONENTTYPE *comp;

    comp = handle;
    if (comp->ComponentDeInit)
        return comp->ComponentDeInit (handle);

    return OMX_ErrorNone;
}
