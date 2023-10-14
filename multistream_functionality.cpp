//
// Created by kamoliddin on 4/30/23.
//

#include "analytics.h"
#include "gst-nvmessage.h"
#include "pad_probe_functions.h"
#include "msg_generate.h"
#include "deepstream_nvdsanalytics_meta.h"
#include "nvds_analytics_meta.h"
#include "nvds_yml_parser.h"
#include "nvdsmeta_schema.h"
#include "gstnvdsmeta.h"
#include <sstream>
#include <vector>
#include <iostream>
#include <math.h>
#include <sys/timeb.h>
#include </usr/local/cuda-11.8/include/cuda_runtime_api.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gst/gst.h>
#include "multistream_functionality.h"

#define GST_CAPS_FEATURES_NVMM "memory:NVMM"
gboolean PERF_MODE = FALSE;
extern void
cb_newpad(GstElement *decodebin, GstPad *decoder_src_pad, gpointer data) {
    GstCaps *caps = gst_pad_get_current_caps(decoder_src_pad);
    if (!caps) {
        caps = gst_pad_query_caps(decoder_src_pad, NULL);
    }
    const GstStructure *str = gst_caps_get_structure(caps, 0);
    const gchar *name = gst_structure_get_name(str);
    GstElement *source_bin = (GstElement *) data;
    GstCapsFeatures *features = gst_caps_get_features(caps, 0);

    /* Need to check if the pad created by the decodebin is for video and not
     * audio. */
    if (!strncmp(name, "video", 5)) {
        /* Link the decodebin pad only if decodebin has picked nvidia
         * decoder plugin nvdec_*. We do this by checking if the pad caps contain
         * NVMM memory features. */
        if (gst_caps_features_contains(features, GST_CAPS_FEATURES_NVMM)) {
            /* Get the source bin ghost pad */
            GstPad *bin_ghost_pad = gst_element_get_static_pad(source_bin, "src");
            if (!gst_ghost_pad_set_target(GST_GHOST_PAD (bin_ghost_pad),
                                          decoder_src_pad)) {
                g_printerr("Failed to link decoder src pad to source bin ghost pad\n");
            }
            gst_object_unref(bin_ghost_pad);
        } else {
            g_printerr("Error: Decodebin did not pick nvidia decoder plugin.\n");
        }
    }
}

extern void
decodebin_child_added(GstChildProxy *child_proxy, GObject *object,
                      gchar *name, gpointer user_data) {
    g_print("Decodebin child added: %s\n", name);
    if (g_strrstr(name, "decodebin") == name) {
        g_signal_connect (G_OBJECT(object), "child-added",
                          G_CALLBACK(decodebin_child_added), user_data);
    }
    if (g_strrstr(name, "source") == name) {
        g_object_set(G_OBJECT(object), "drop-on-latency", true, NULL);
    }

}

extern GstElement *
create_source_bin(guint index, gchar *uri) {
    GstElement *bin = NULL, *uri_decode_bin = NULL;
    gchar bin_name[16] = {};

    g_snprintf(bin_name, 15, "source-bin-%02d", index);
    /* Create a source GstBin to abstract this bin's content from the rest of the
     * pipeline */
    bin = gst_bin_new(bin_name);

    /* Source element for reading from the uri.
     * We will use decodebin and let it figure out the container format of the
     * stream and the codec and plug the appropriate demux and decode plugins. */
    if (PERF_MODE) {
        uri_decode_bin = gst_element_factory_make("nvurisrcbin", "uri-decode-bin");
        g_object_set(G_OBJECT (uri_decode_bin), "file-loop", TRUE, NULL);
        g_object_set(G_OBJECT (uri_decode_bin), "cudadec-memtype", 0, NULL);
    } else {
        uri_decode_bin = gst_element_factory_make("uridecodebin", "uri-decode-bin");
    }

    if (!bin || !uri_decode_bin) {
        g_printerr("One element in source bin could not be created.\n");
        return NULL;
    }

    /* We set the input uri to the source element */
    g_object_set(G_OBJECT (uri_decode_bin), "uri", uri, NULL);

    /* Connect to the "pad-added" signal of the decodebin which generates a
     * callback once a new pad for raw data has beed created by the decodebin */
    g_signal_connect (G_OBJECT(uri_decode_bin), "pad-added",
                      G_CALLBACK(cb_newpad), bin);
    g_signal_connect (G_OBJECT(uri_decode_bin), "child-added",
                      G_CALLBACK(decodebin_child_added), bin);

    gst_bin_add(GST_BIN (bin), uri_decode_bin);

    /* We need to create a ghost pad for the source bin which will act as a proxy
     * for the video decoder src pad. The ghost pad will not have a target right
     * now. Once the decode bin creates the video decoder and generates the
     * cb_newpad callback, we will set the ghost pad target to the video decoder
     * src pad. */
    if (!gst_element_add_pad(bin, gst_ghost_pad_new_no_target("src",
                                                              GST_PAD_SRC))) {
        g_printerr("Failed to add ghost pad in source bin\n");
        return NULL;
    }

    return bin;
}

extern gboolean
bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
    GMainLoop *loop = (GMainLoop *) data;
    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_ERROR: {
            gchar *debug;
            GError *error;
            gst_message_parse_error(msg, &error, &debug);
            g_printerr("ERROR from element %s: %s\n",
                       GST_OBJECT_NAME (msg->src), error->message);
            if (debug)
                g_printerr("Error details: %s\n", debug);
            g_free(debug);
            g_error_free(error);
            g_main_loop_quit(loop);
            break;
        }
        default:
            break;
    }
    return TRUE;
}