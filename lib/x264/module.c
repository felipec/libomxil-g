#include <core.h>
#include <async_queue.h>
#include <OMX_Component.h>

#include <stdint.h>
#include <x264.h>

#include <glib.h>
#include <stdio.h>

typedef struct CompPrivate CompPrivate;
typedef struct CompPrivatePort CompPrivatePort;

static void *foo_thread (void *cb_data);

struct CompPrivate
{
    OMX_STATETYPE state;
    OMX_CALLBACKTYPE *callbacks;
    OMX_PTR app_data;
    CompPrivatePort *ports;
    gboolean done;
    GMutex *flush_mutex;

    int width;
    int height;
    x264_param_t params;
    x264_t *enc;
};

struct CompPrivatePort
{
    OMX_PARAM_PORTDEFINITIONTYPE port_def;
    AsyncQueue *queue;
};

static OMX_ERRORTYPE
comp_GetState (OMX_HANDLETYPE handle,
               OMX_STATETYPE *state)
{
    OMX_COMPONENTTYPE *comp;
    CompPrivate *private;

    /* printf ("GetState\n"); */

    comp = handle;
    private = comp->pComponentPrivate;

    *state = private->state;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
comp_GetParameter (OMX_HANDLETYPE handle,
                   OMX_INDEXTYPE index,
                   OMX_PTR param)
{
    OMX_COMPONENTTYPE *comp;
    CompPrivate *private;

    /* printf ("GetParameter\n"); */

    comp = handle;
    private = comp->pComponentPrivate;

    switch (index)
    {
        case OMX_IndexParamPortDefinition:
            {
                OMX_PARAM_PORTDEFINITIONTYPE *port_def;
                port_def = param;
                memcpy (port_def, &private->ports[port_def->nPortIndex].port_def, port_def->nSize);
                break;
            }
        default:
            break;
    }

    return OMX_ErrorNone;
}

static void
update_framesize (CompPrivate *private)
{
    private->width = private->ports[0].port_def.format.video.nFrameWidth;
    private->height = private->ports[0].port_def.format.video.nFrameHeight;
    private->ports[1].port_def.nBufferSize = private->width * private->height * 3 / 2;
}

static OMX_ERRORTYPE
comp_SetParameter (OMX_HANDLETYPE handle,
                   OMX_INDEXTYPE index,
                   OMX_PTR param)
{
    OMX_COMPONENTTYPE *comp;
    CompPrivate *private;

    /* printf ("SetParameter\n"); */

    comp = handle;
    private = comp->pComponentPrivate;

    switch (index)
    {
        case OMX_IndexParamPortDefinition:
            {
                OMX_PARAM_PORTDEFINITIONTYPE *port_def;
                port_def = param;
                memcpy (&private->ports[port_def->nPortIndex].port_def, port_def, port_def->nSize);
                update_framesize (private);
                break;
            }
        default:
            break;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
comp_SendCommand (OMX_HANDLETYPE handle,
                  OMX_COMMANDTYPE command,
                  OMX_U32 param_1,
                  OMX_PTR data)
{
    OMX_COMPONENTTYPE *comp;
    CompPrivate *private;

    /* printf ("SendCommand\n"); */

    comp = handle;
    private = comp->pComponentPrivate;

    switch (command)
    {
        case OMX_CommandStateSet:
            {
                if (private->state == OMX_StateLoaded && param_1 == OMX_StateIdle)
                {
                    x264_param_t *params;
                    params = &private->params;

                    x264_param_default (params);

                    params->rc.i_rc_method = X264_RC_CQP;

                    params->i_width = private->width;
                    params->i_height = private->height;

                    private->enc = x264_encoder_open (params);

                    g_thread_create (foo_thread, comp, TRUE, NULL);
                }
                else if (private->state == OMX_StateIdle && param_1 == OMX_StateLoaded)
                {
                    if (private->enc)
                        x264_encoder_close (private->enc);
                }
                private->state = param_1;
                private->callbacks->EventHandler (handle,
                                                  private->app_data, OMX_EventCmdComplete,
                                                  OMX_CommandStateSet, private->state, data);
            }
            break;
        case  OMX_CommandFlush:
            {
                g_mutex_lock (private->flush_mutex);
                {
                    OMX_BUFFERHEADERTYPE *buffer;

                    while (buffer = async_queue_pop_forced (private->ports[0].queue))
                    {
                        private->callbacks->EmptyBufferDone (comp,
                                                             private->app_data, buffer);
                    }

                    while (buffer = async_queue_pop_forced (private->ports[1].queue))
                    {
                        private->callbacks->FillBufferDone (comp,
                                                            private->app_data, buffer);
                    }
                }
                g_mutex_unlock (private->flush_mutex);

                private->callbacks->EventHandler (handle,
                                                  private->app_data, OMX_EventCmdComplete,
                                                  OMX_CommandFlush, param_1, data);
            }
            break;
        default:
            /* printf ("command: %d\n", command); */
            break;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
comp_UseBuffer (OMX_HANDLETYPE handle,
                OMX_BUFFERHEADERTYPE **buffer_header,
                OMX_U32 index,
                OMX_PTR data,
                OMX_U32 size,
                OMX_U8 *buffer)
{
    OMX_BUFFERHEADERTYPE *new;

    new = calloc (1, sizeof (OMX_BUFFERHEADERTYPE));
    new->nSize = sizeof (OMX_BUFFERHEADERTYPE);
    new->nVersion.nVersion = 1;
    new->pBuffer = buffer;
    new->nAllocLen = size;

    switch (index)
    {
        case 0: new->nInputPortIndex = 0; break;
        case 1: new->nOutputPortIndex = 1; break;
        default: break;
    }

    *buffer_header = new;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
comp_FreeBuffer (OMX_HANDLETYPE handle,
                 OMX_U32 index,
                 OMX_BUFFERHEADERTYPE *buffer_header)
{
    free (buffer_header);

    return OMX_ErrorNone;
}

static gpointer
foo_thread (gpointer cb_data)
{
    OMX_COMPONENTTYPE *comp;
    CompPrivate *private;
    x264_picture_t in_pic;
    x264_picture_t out_pic;

    comp = cb_data;
    private = comp->pComponentPrivate;

    in_pic.i_type = X264_TYPE_AUTO;
    in_pic.i_qpplus1 = 0;
    in_pic.img.i_csp = X264_CSP_I420;
    in_pic.img.i_plane = 3;
    in_pic.img.i_stride[0] = private->width;
    in_pic.img.i_stride[1] = private->width / 2;
    in_pic.img.i_stride[2] = private->width / 2;

    while (!private->done)
    {
        OMX_BUFFERHEADERTYPE *in_buffer;
        OMX_BUFFERHEADERTYPE *out_buffer;

        in_buffer = async_queue_pop (private->ports[0].queue);
        if (!in_buffer) continue;

        out_buffer = async_queue_pop (private->ports[1].queue);
        if (!out_buffer) continue;

        out_buffer->nFilledLen = 0;
        out_buffer->nFlags = in_buffer->nFlags;

        if (in_buffer->nFilledLen)
        {
            x264_nal_t *nal;
            int nal_count;
            int i;
            uint8_t *p;

            in_pic.i_pts = in_buffer->nTimeStamp;
            in_pic.img.plane[0] = in_buffer->pBuffer;
            in_pic.img.plane[1] = in_pic.img.plane[0] + private->width * private->height;
            in_pic.img.plane[2] = in_pic.img.plane[1] + private->width * private->height / 4;

            x264_encoder_encode (private->enc, &nal, &nal_count, &in_pic, &out_pic);

            p = out_buffer->pBuffer;
            out_buffer->nTimeStamp = out_pic.i_pts; /** @todo add dts */

            for (i = 0; i < nal_count; i++)
            {
                int size;
                x264_nal_encode (p, &size, 1, &nal[i]);
                p += size;
                out_buffer->nFilledLen += size;
            }

            in_buffer->nFilledLen = 0;

            if (out_pic.i_type == X264_TYPE_IDR)
                out_buffer->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
        }

        g_mutex_lock (private->flush_mutex);

        private->callbacks->FillBufferDone (comp,
                                            private->app_data, out_buffer);
        if (in_buffer->nFilledLen == 0)
        {
            private->callbacks->EmptyBufferDone (comp,
                                                 private->app_data, in_buffer);
        }

        g_mutex_unlock (private->flush_mutex);
    }

    return NULL;
}

static OMX_ERRORTYPE
comp_EmptyThisBuffer (OMX_HANDLETYPE handle,
                      OMX_BUFFERHEADERTYPE *buffer_header)
{
    OMX_COMPONENTTYPE *comp;
    CompPrivate *private;

    /* printf ("EmptyThisBuffer\n"); */

    comp = handle;
    private = comp->pComponentPrivate;

    async_queue_push (private->ports[0].queue, buffer_header);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
comp_FillThisBuffer (OMX_HANDLETYPE handle,
                     OMX_BUFFERHEADERTYPE *buffer_header)
{
    OMX_COMPONENTTYPE *comp;
    CompPrivate *private;

    /* printf ("FillThisBuffer\n"); */

    comp = handle;
    private = comp->pComponentPrivate;

    async_queue_push (private->ports[1].queue, buffer_header);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
comp_SetCallbacks (OMX_HANDLETYPE handle,
                   OMX_CALLBACKTYPE *callbacks,
                   OMX_PTR data)
{
    OMX_COMPONENTTYPE *comp;
    CompPrivate *private;

    comp = handle;
    private = comp->pComponentPrivate;

    private->callbacks = callbacks;
    private->app_data = data;

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
comp_ComponentDeInit (OMX_HANDLETYPE handle)
{
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE
init (OMX_COMPONENTTYPE *comp)
{
    comp->nVersion.nVersion = 1;

    comp->SetCallbacks = comp_SetCallbacks;
    comp->ComponentDeInit = comp_ComponentDeInit;
    comp->GetState = comp_GetState;
    comp->GetParameter = comp_GetParameter;
    comp->SetParameter = comp_SetParameter;
    comp->SendCommand = comp_SendCommand;
    comp->UseBuffer = comp_UseBuffer;
    comp->FreeBuffer = comp_FreeBuffer;
    comp->EmptyThisBuffer = comp_EmptyThisBuffer;
    comp->FillThisBuffer = comp_FillThisBuffer;

    {
        CompPrivate *private;

        private = calloc (1, sizeof (CompPrivate));
        private->state = OMX_StateLoaded;
        private->ports = calloc (2, sizeof (CompPrivatePort));
        private->flush_mutex = g_mutex_new ();

        private->ports[0].queue = async_queue_new ();
        private->ports[1].queue = async_queue_new ();

        private->width = 320;
        private->height = 240;

        {
            OMX_PARAM_PORTDEFINITIONTYPE *port_def;

            port_def = &private->ports[0].port_def;
            port_def->nSize = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
            port_def->nVersion.nVersion = 1;
            port_def->nPortIndex = 0;
            port_def->eDir = OMX_DirInput;
            port_def->nBufferCountActual = 1;
            port_def->nBufferCountMin = 1;
            port_def->nBufferSize = private->width * private->height * 3 / 2;
            port_def->eDomain = OMX_PortDomainVideo;

        }

        {
            OMX_PARAM_PORTDEFINITIONTYPE *port_def;

            port_def = &private->ports[1].port_def;
            port_def->nSize = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
            port_def->nVersion.nVersion = 1;
            port_def->nPortIndex = 1;
            port_def->eDir = OMX_DirOutput;
            port_def->nBufferCountActual = 1;
            port_def->nBufferCountMin = 1;
            port_def->nBufferSize = private->width * private->height / 2;
            port_def->eDomain = OMX_PortDomainVideo;
        }

        comp->pComponentPrivate = private;
    }

    return OMX_ErrorNone;
}

ComponentInfo x264enc_info = {
    .init = init,
};
