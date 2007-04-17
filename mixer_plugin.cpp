/*  This file is part of the KDE project
    Copyright (C) 2007 Matthias Kretz <kretz@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#include <kdemacros.h>
#include <phonon/volumefadereffect.h>
#include <klocale.h>
#include <kdebug.h>
#include <cmath>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <xine.h>
extern "C" {
// xine headers use the reserved keyword this:
#define this this_xine
#include <xine/compat.h>
#include <xine/post.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#undef this

typedef struct
{
    post_class_t post_class;
    xine_t *xine;
} kmixer_class_t;

typedef struct KVolumeFaderPlugin
{
    post_plugin_t post;

    /* private data */
    pthread_mutex_t    lock;
    //xine_post_in_t params_input;

} kmixer_plugin_t;

//X /**************************************************************************
//X  * parameters
//X  *************************************************************************/
//X 
//X typedef struct
//X {
//X     Phonon::VolumeFaderEffect::FadeCurve fadeCurve;
//X     float currentVolume;
//X     float fadeTo;
//X     int fadeTime;
//X } kmixer_parameters_t;
//X 
//X /*
//X  * description of params struct
//X  */
//X static char *enum_fadeCurve[] = { "Fade3Decibel", "Fade6Decibel", "Fade9Decibel", "Fade12Decibel", NULL };
//X START_PARAM_DESCR(kmixer_parameters_t)
//X PARAM_ITEM(POST_PARAM_TYPE_INT, fadeCurve, enum_fadeCurve, 0.0, 0.0, 0, "fade curve")
//X PARAM_ITEM(POST_PARAM_TYPE_DOUBLE, currentVolume, NULL, 0.0, maxVolume, 0, "current volume")
//X PARAM_ITEM(POST_PARAM_TYPE_DOUBLE, fadeTo, NULL, 0.0, maxVolume, 0, "volume to fade to")
//X PARAM_ITEM(POST_PARAM_TYPE_INT, fadeTime, NULL, 0.0, 10000.0, 0, "fade time in milliseconds")
//X END_PARAM_DESCR(param_descr)
//X 
//X static int set_parameters (xine_post_t *this_gen, void *param_gen) 
//X {
//X     kmixer_plugin_t *that = reinterpret_cast<kmixer_plugin_t *>(this_gen);
//X     kmixer_parameters_t *param = static_cast<kmixer_parameters_t *>(param_gen);
//X 
//X     pthread_mutex_lock (&that->lock);
//X     that->fadeCurve = param->fadeCurve;
//X     that->fadeStart = param->currentVolume;
//X     that->fadeDiff = param->fadeTo - that->fadeStart;
//X     that->curvePosition = 0;
//X     that->fadeTime = param->fadeTime;
//X     that->curveLength = static_cast<int>((param->fadeTime * that->rate) / 1000);
//X     that->oneOverCurveLength = 1000.0f / (param->fadeTime * that->rate);
//X     const char *x = "unknown";
//X     switch (that->fadeCurve) {
//X         case Phonon::VolumeFaderEffect::Fade3Decibel:
//X             if (that->fadeDiff > 0) {
//X                 that->curveValue = curveValueFadeIn3dB;
//X             } else {
//X                 that->curveValue = curveValueFadeOut3dB;
//X             }
//X             x = "3dB";
//X             break;
//X         case Phonon::VolumeFaderEffect::Fade6Decibel:
//X             that->curveValue = curveValueFade6dB;
//X             x = "6dB";
//X             break;
//X         case Phonon::VolumeFaderEffect::Fade9Decibel:
//X             if (that->fadeDiff > 0) {
//X                 that->curveValue = curveValueFadeIn9dB;
//X             } else {
//X                 that->curveValue = curveValueFadeOut9dB;
//X             }
//X             x = "9dB";
//X             break;
//X         case Phonon::VolumeFaderEffect::Fade12Decibel:
//X             if (that->fadeDiff > 0) {
//X                 that->curveValue = curveValueFadeIn12dB;
//X             } else {
//X                 that->curveValue = curveValueFadeOut12dB;
//X             }
//X             x = "12dB";
//X             break;
//X     }
//X     kDebug(610) << k_funcinfo << "set parameters to "
//X         << x << ", "
//X         << that->fadeStart << ", "
//X         << that->fadeDiff << ", "
//X         << that->curvePosition << ", "
//X         << that->oneOverCurveLength << ", "
//X         << param->fadeTime
//X         << endl;
//X     pthread_mutex_unlock (&that->lock);
//X 
//X     return 1;
//X }
//X 
//X static int get_parameters (xine_post_t *this_gen, void *param_gen) 
//X {
//X     kmixer_plugin_t *that = reinterpret_cast<kmixer_plugin_t *>(this_gen);
//X     kmixer_parameters_t *param = static_cast<kmixer_parameters_t *>(param_gen);
//X 
//X     pthread_mutex_lock (&that->lock);
//X     param->fadeCurve = that->fadeCurve;
//X     if (that->curvePosition == 0) {
//X         param->currentVolume = that->fadeStart;
//X     } else {
//X         param->currentVolume = that->curveValue(that->fadeStart, that->fadeDiff, that->curvePosition, that->oneOverCurveLength);
//X     }
//X     param->fadeTo = that->fadeDiff + that->fadeStart;
//X     param->fadeTime = (that->curveLength - that->curvePosition) * 1000 / that->rate;
//X     pthread_mutex_unlock (&that->lock);
//X 
//X     return 1;
//X }
//X 
//X static xine_post_api_descr_t *get_param_descr()
//X {
//X     return &param_descr;
//X }
//X 
//X K_GLOBAL_STATIC_WITH_ARGS(QByteArray, helpText, (
//X             i18n("Normalizes audio by maximizing the volume without distorting "
//X              "the sound.\n"
//X              "\n"
//X              "Parameters:\n"
//X              "  method: 1: use a single sample to smooth the variations via "
//X              "the standard weighted mean over past samples (default); 2: use "
//X              "several samples to smooth the variations via the standard "
//X              "weighted mean over past samples.\n"
//X              ).toLocal8Bit()))
//X 
//X static char *get_help ()
//X {
//X     return helpText->data();
//X }
//X 
//X static xine_post_api_t post_api = {
//X     set_parameters,
//X     get_parameters,
//X     get_param_descr,
//X     get_help,
//X };


/**************************************************************************
 * xine audio post plugin functions
 *************************************************************************/

static int kmixer_port_open(xine_audio_port_t *port_gen, xine_stream_t *stream,
                             uint32_t bits, uint32_t rate, int mode)
{
    post_audio_port_t *port = reinterpret_cast<post_audio_port_t *>(port_gen);
    kmixer_plugin_t *that = reinterpret_cast<kmixer_plugin_t *>(port->post);

    _x_post_rewire(&that->post);
    _x_post_inc_usage(port);

    port->stream = stream;
    port->bits = bits;
    port->rate = rate;
    port->mode = mode;

    return port->original_port->open(port->original_port, stream, bits, rate, mode);
}

static void kmixer_port_close(xine_audio_port_t *port_gen, xine_stream_t *stream)
{
    post_audio_port_t *port = reinterpret_cast<post_audio_port_t *>(port_gen);

    port->stream = NULL;
    port->original_port->close(port->original_port, stream);
    _x_post_dec_usage(port);
}

static void kmixer_port_put_buffer(xine_audio_port_t *port_gen,
        audio_buffer_t *buf, xine_stream_t *stream)
{
    post_audio_port_t *port = reinterpret_cast<post_audio_port_t *>(port_gen);
    //kmixer_plugin_t *that = reinterpret_cast<kmixer_plugin_t *>(port->post);

    // modify the buffer data
    //that->fadeBuffer(buf);

    // and send the modified buffer to the original port
    port->original_port->put_buffer(port->original_port, buf, stream);
    return;
}

static void kmixer_dispose(post_plugin_t *this_gen)
{
    kmixer_plugin_t *that = reinterpret_cast<kmixer_plugin_t *>(this_gen);

    if (_x_post_dispose(this_gen)) {
        pthread_mutex_destroy(&that->lock);
        free(that);
    }
}

/* plugin class functions */
static post_plugin_t *kmixer_open_plugin(post_class_t *class_gen, int inputs,
                                          xine_audio_port_t **audio_target,
                                          xine_video_port_t **video_target)
{
    Q_UNUSED(class_gen);
    Q_UNUSED(video_target);

    kmixer_plugin_t *that = static_cast<kmixer_plugin_t *>(xine_xmalloc(sizeof(kmixer_plugin_t)));

    // refuse to work without an audio port to decorate or if there's nothing or too much to mix
    if (!that || !audio_target || !audio_target[0] || inputs < 2 || inputs > 32) {
        free(that);
        return NULL;
    }

    // creates 1 audio I/O, 0 video I/O
    _x_post_init(&that->post, inputs, 0);

    // init private data
    pthread_mutex_init (&that->lock, NULL);

    // the following call wires our plugin in front of the given audio_target
    post_audio_port_t *port;
    post_in_t *input;
    post_out_t *output;
    port = _x_post_intercept_audio_port(&that->post, audio_target[0], &input, &output);
    input->xine_in.name = "in0";

    // the methods of new_port are all forwarded to audio_target, overwrite a few of them here:
    port->new_port.open       = kmixer_port_open;
    port->new_port.close      = kmixer_port_close;
    port->new_port.put_buffer = kmixer_port_put_buffer;

    for (int i = 1; i < inputs; ++i) {
        // additional inputs
        port = _x_post_intercept_audio_port(&that->post, audio_target[0], &input, NULL);
        char *name = strdup("in0");
        name[2] += i;
        input->xine_in.name = name;

        // the methods of new_port are all forwarded to audio_target, overwrite a few of them here:
        port->new_port.open       = kmixer_port_open;
        port->new_port.close      = kmixer_port_close;
        port->new_port.put_buffer = kmixer_port_put_buffer;
    }

    // add a parameter input to the plugin
    //xine_post_in_t *input_api;
    //input_api = &that->params_input;
    //input_api->name = "parameters";
    //input_api->type = XINE_POST_DATA_PARAMETERS;
    //input_api->data = &post_api;
    //xine_list_push_back(that->post.input, input_api);

    that->post.xine_post.audio_input[0] = &port->new_port;

    // our own cleanup function
    that->post.dispose = kmixer_dispose;

    return &that->post;
}

static char *kmixer_get_identifier(post_class_t *class_gen)
{
    Q_UNUSED(class_gen);
    return "KMixer";
}

static char *kmixer_get_description(post_class_t *class_gen)
{
    Q_UNUSED(class_gen);
    return "mix several audio streams together";
}

static void kmixer_class_dispose(post_class_t *class_gen)
{
    free(class_gen);
}

/* plugin class initialization function */
void *init_kmixer_plugin (xine_t *xine, void *)
{
    kmixer_class_t *_class = static_cast<kmixer_class_t *>(malloc(sizeof(kmixer_class_t)));

    if (!_class) {
        return NULL;
    }

    _class->post_class.open_plugin     = kmixer_open_plugin;
    _class->post_class.get_identifier  = kmixer_get_identifier;
    _class->post_class.get_description = kmixer_get_description;
    _class->post_class.dispose         = kmixer_class_dispose;

    _class->xine                       = xine;

    return _class;
}

} // extern "C"
