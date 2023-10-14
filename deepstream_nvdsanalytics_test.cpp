/*
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * SPDX-FileCopyrightText: Copyright (c) 2018-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <gst/gst.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include </usr/local/cuda-11.8/include/cuda_runtime_api.h>
#include <sys/timeb.h>
#include <math.h>
#include <iostream>
#include <vector>
#include <sstream>
#include "gstnvdsmeta.h"
#include "nvdsmeta_schema.h"
#include "nvds_yml_parser.h"
#include "gstnvdsmeta.h"
#include "nvds_analytics_meta.h"
#include "deepstream_nvdsanalytics_meta.h"
#include "msg_generate.h"
#include "pad_probe_functions.h"
#ifndef PLATFORM_TEGRA

#include "gst-nvmessage.h"

#endif
#define MAX_DISPLAY_LEN 64
#define MAX_TIME_STAMP_LEN 32
//#define TRACKER_CONFIG_FILE "ds_tracker_config.txt"
#define MAX_TRACKING_ID_LEN 16

#include "analytics.h"
#include "pad_probe_functions.h"
#include "multistream_functionality.h"

#define PGIE_CLASS_ID_VEHICLE 0
#define PGIE_CLASS_ID_PERSON 2

#define PGIE_CONFIG_FILE  "dstest4_pgie_config.txt"
#define MSCONV_CONFIG_FILE "dstest4_msgconv_config.txt"

/* The muxer output resolution must be set if the input streams will be of
 * different resolution. The muxer will scale all the input frames to this
 * resolution. */
#define MUXER_OUTPUT_WIDTH 1920
#define MUXER_OUTPUT_HEIGHT 1080

/* Muxer batch formation timeout, for e.g. 40 millisec. Should ideally be set
 * based on the fastest source's framerate. */
#define MUXER_BATCH_TIMEOUT_USEC 4000000
#define IS_YAML(file) (g_str_has_suffix (file, ".yml") ||   (file, ".yaml"))
#define TILED_OUTPUT_WIDTH 1920
#define TILED_OUTPUT_HEIGHT 1080


static gchar *cfg_file = NULL;
static gchar **input_file = NULL;
static gchar *topic = NULL;
static gchar *conn_str = NULL;
static gchar *proto_lib = NULL;

static gint schema_type = 0;
static gint msg2p_meta = 0;
static gint frame_interval = 30;
static gboolean display_off = FALSE;


//2D array of input files
static gchar *input_files[16][100] = {NULL};



gchar pgie_classes_str[4][32] = {"Person"
};


GOptionEntry entries[] = {
        {"cfg-file",       'c', 0,                     G_OPTION_ARG_FILENAME,     &cfg_file,
                                                                                                "Set the adaptor config file. Optional if connection string has relevant  details.",
                                                                                                                                                                               NULL},
        {"input-file",     'i', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING_ARRAY, &input_file,
                                                                                                "Set the input file. Optional if connection string has relevant details.",
                                                                                                                                                                               NULL},
        {"topic",          't', 0,                     G_OPTION_ARG_STRING,       &topic,
                                                                                                "Name of message topic. Optional if it is part of connection string or config file.",
                                                                                                                                                                               NULL},
        {"conn-str",       0,   0,                     G_OPTION_ARG_STRING,       &conn_str,
                                                                                                "Connection string of backend server. Optional if it is part of config file.", NULL},
        {"proto-lib",      'p', 0,                     G_OPTION_ARG_STRING,       &proto_lib,
                                                                                                "Absolute path of adaptor library",                                            NULL},
        {"schema",         's', 0,                     G_OPTION_ARG_INT,          &schema_type,
                                                                                                "Type of message schema (0=Full, 1=minimal, 2=protobuf), default=0",           NULL},
        {"msg2p-meta",     0,   0,                     G_OPTION_ARG_INT,          &msg2p_meta,
                                                                                                "msg2payload generation metadata type (0=Event Msg meta, 1=nvds meta), default=0",
                                                                                                                                                                               NULL},
        {"frame-interval", 0,   0,                     G_OPTION_ARG_INT,          &frame_interval,
                                                                                                "Frame interval at which payload is generated , default=30",                   NULL},
        {"no-display",     0,   0,                     G_OPTION_ARG_NONE,         &display_off, "Disable display",
                                                                                                                                                                               NULL},

        {NULL}
};



/* NVIDIA Decoder source pad memory feature. This feature signifies that source
 * pads having this capability will push GstBuffers containing cuda buffers. */

int
main(int argc, char *argv[]) {
    GMainLoop *loop = NULL;
    GstElement *pipeline = NULL, *source = NULL, *h264parser = NULL,
            *decoder = NULL, *sink = NULL, *tiler = NULL, *pgie = NULL, *tracker = NULL, *nvvidconv = NULL,
            *nvdsanalytics = NULL,
            *nvosd = NULL, *nvstreammux;
    GstElement *msgconv = NULL, *msgbroker = NULL, *tee = NULL;
    GstElement *queue1 = NULL, *queue2 = NULL;
    GstBus *bus = NULL;
    guint bus_watch_id;
    GstPad *nvdsanalytics_src_pad = NULL;
    GstPad *osd_sink_pad = NULL;
    GstPad *tee_render_pad = NULL;
    GstPad *tee_msg_pad = NULL;
    GstPad *sink_pad = NULL;
    GstPad *src_pad = NULL;
    GOptionContext *ctx = NULL;
    GOptionGroup *group = NULL;
    GError *error = NULL;
    NvDsGieType pgie_type = NVDS_GIE_PLUGIN_INFER;
    guint i = 0, num_sources = 0;
    guint tiler_rows, tiler_columns;
    int current_device = -1;
    cudaGetDevice(&current_device);
    struct cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, current_device);

    ctx = g_option_context_new("Nvidia DeepStream Test4");
    group = g_option_group_new("test4", NULL, NULL, NULL, NULL);
    g_option_group_add_entries(group, entries);

    g_option_context_set_main_group(ctx, group);
    g_option_context_add_group(ctx, gst_init_get_option_group());


    if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
        g_option_context_free(ctx);
        g_printerr("%s", error->message);
        return -1;
    }
    g_option_context_free(ctx);

    if (!proto_lib || !input_file) {

        g_printerr("missing arguments\n");
        g_printerr("Usage: %s <yml file>\n", argv[0]);
        g_printerr
                ("Usage: %s -i <H264 filename> -p <Proto adaptor library> --conn-str=<Connection string>\n",
                 argv[0]);
        return -1;
    }

    loop = g_main_loop_new(NULL, FALSE);



    /* Create gstreamer elements */
    /* Create Pipeline element that will form a connection of other elements */
    pipeline = gst_pipeline_new("dstest4-pipeline");

    /* Create nvstreammux instance to form batches from one or more sources. */
    nvstreammux = gst_element_factory_make("nvstreammux", "stream-muxer");

    if (!pipeline || !nvstreammux) {
        g_printerr("One of 2 element could not be created. Exiting.\n");
        return -1;
    }
    gst_bin_add(GST_BIN (pipeline), nvstreammux);

    num_sources = g_strv_length(input_file);
    g_print("num_sources = %d", num_sources);
    for (i = 0; i < num_sources; i++) {
        GstPad *sinkpad, *srcpad;
        gchar pad_name[16] = {};
        GstElement *source_bin = NULL;
        source_bin = create_source_bin(i, input_file[i]);
        g_print("input_file = %s", input_file[i]);
        if (!source_bin) {
            g_printerr("Failed to create source bin. Exiting.\n");
            return -1;
        }

        gst_bin_add(GST_BIN (pipeline), source_bin);

        g_snprintf(pad_name, 15, "sink_%u", i);
        sinkpad = gst_element_get_request_pad(nvstreammux, pad_name);
        if (!sinkpad) {
            g_printerr("Streammux request sink pad failed. Exiting.\n");
            return -1;
        }

        srcpad = gst_element_get_static_pad(source_bin, "src");
        if (!srcpad) {
            g_printerr("Failed to get src pad of source bin. Exiting.\n");
            return -1;
        }

        if (gst_pad_link(srcpad, sinkpad) != GST_PAD_LINK_OK) {
            g_printerr("Failed to link source bin to stream muxer. Exiting.\n");
            return -1;
        }
        gst_object_unref(srcpad);
        gst_object_unref(sinkpad);
    }

    /* Use nvinfer or nvinferserver to run inferencing on decoder's output,
     * behaviour of inferencing is set through config file */
    if (pgie_type == NVDS_GIE_PLUGIN_INFER_SERVER) {
        pgie = gst_element_factory_make("nvinferserver", "primary-nvinference-engine");
    } else {
        pgie = gst_element_factory_make("nvinfer", "primary-nvinference-engine");
    };

    tracker = gst_element_factory_make("nvtracker", "tracker");

    /* Use nvdsanalytics to perform analytics on object */
    nvdsanalytics = gst_element_factory_make("nvdsanalytics", "nvdsanalytics");

    /* Use convertor to convert from NV12 to RGBA as required by nvosd */
    nvvidconv = gst_element_factory_make("nvvideoconvert", "nvvideo-converter");

    /* Create OSD to draw on the converted RGBA buffer */
    nvosd = gst_element_factory_make("nvdsosd", "nv-onscreendisplay");


    /* Create msg converter to generate payload from buffer metadata */
    msgconv = gst_element_factory_make("nvmsgconv", "nvmsg-converter");

    /* Create msg broker to send payload to server */
    msgbroker = gst_element_factory_make("nvmsgbroker", "nvmsg-broker");

    /* Create tee to render buffer and send message simultaneously */
    tee = gst_element_factory_make("tee", "nvsink-tee");

    /* Create queues */
    queue1 = gst_element_factory_make("queue", "nvtee-que1");
    queue2 = gst_element_factory_make("queue", "nvtee-que2");

    tiler = gst_element_factory_make("nvmultistreamtiler", "nvtiler");

    /* Finally render the osd output */
    if (display_off) {
        sink = gst_element_factory_make("fakesink", "nvvideo-renderer");
    } else if (prop.integrated) {
        sink = gst_element_factory_make("nv3dsink", "nv3d-sink");
    } else {
        sink = gst_element_factory_make("nveglglessink", "nvvideo-renderer");
    }

    if (!pgie || !tracker || !nvdsanalytics || !tiler
        || !nvvidconv || !nvosd || !msgconv || !msgbroker || !tee
        || !queue1 || !queue2 || !sink) {
        g_printerr("One element could not be created. Exiting.\n");
        return -1;
    }


    g_object_set(G_OBJECT (nvstreammux), "batch-size", num_sources, NULL);

    g_object_set(G_OBJECT (nvstreammux), "width", MUXER_OUTPUT_WIDTH, "height",
                 MUXER_OUTPUT_HEIGHT,
                 "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, NULL);


    /* Set all the necessary properties of the nvinfer element,
     * the necessary ones are : */
    g_object_set(G_OBJECT (pgie), "config-file-path", PGIE_CONFIG_FILE, NULL);
    g_object_get(G_OBJECT (pgie), "batch-size", &num_sources, NULL);
    g_object_set(G_OBJECT (nvdsanalytics),
                 "config-file", "config_nvdsanalytics.txt",
                 NULL);

    tiler_rows = (guint) sqrt(num_sources);
    tiler_columns = (guint) ceil(1.0 * num_sources / tiler_rows);
    /* we set the tiler properties here */
    g_object_set(G_OBJECT (tiler), "rows", tiler_rows, "columns", tiler_columns,
                 "width", TILED_OUTPUT_WIDTH, "height", TILED_OUTPUT_HEIGHT, NULL);

    g_object_set(G_OBJECT (sink), "qos", 0, NULL);

    g_object_set(G_OBJECT (tracker),
                 "ll-lib-file", "/opt/nvidia/deepstream/deepstream-6.2/lib/libnvds_nvmultiobjecttracker.so",
                 "ll-config-file", "../../../../samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml",
                 "tracker-width", 640, "tracker-height", 480,
                 NULL);
    g_object_set(G_OBJECT (msgconv), "config", MSCONV_CONFIG_FILE, NULL);
    g_object_set(G_OBJECT (msgconv), "payload-type", schema_type, NULL);
    g_object_set(G_OBJECT (msgconv), "msg2p-newapi", msg2p_meta, NULL);
    g_object_set(G_OBJECT (msgconv), "frame-interval", frame_interval, NULL);

    g_object_set(G_OBJECT (msgbroker), "proto-lib", proto_lib,
                 "conn-str", conn_str, "sync", FALSE, NULL);

    if (topic) {
        g_object_set(G_OBJECT (msgbroker), "topic", topic, NULL);
    }

    if (cfg_file) {
        g_object_set(G_OBJECT (msgbroker), "config", cfg_file, NULL);
    }

    g_object_set(G_OBJECT (sink), "sync", TRUE, NULL);

    bus = gst_pipeline_get_bus(GST_PIPELINE (pipeline));
    bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);
    gst_bin_add_many(GST_BIN (pipeline),
                     pgie, tracker, nvdsanalytics, tiler,
                     nvvidconv, nvosd, tee, queue1, queue2, msgconv, msgbroker, sink, NULL);

    /* we link the elements together */
    /* file-source -> h264-parser -> nvh264-decoder -> nvstreammux ->
     * pgie -> tracker -> nvdsanalytics ->tiler-> nvvidconv  -> nvosd -> tee -> video-renderer
     *                                                                      |
     *                                                                      |-> msgconv -> msgbroker  */

    if (!gst_element_link_many(nvstreammux, pgie, tracker, nvdsanalytics, tiler, nvvidconv, nvosd, tee, NULL)) {
        g_printerr("Elements could not be linked. Exiting.\n");
        return -1;
    }

    if (!gst_element_link_many(queue1, msgconv, msgbroker, NULL)) {
        g_printerr("Elements could not be linked. Exiting.\n");
        return -1;
    }

    if (!gst_element_link(queue2, sink)) {
        g_printerr("Elements could not be linked. Exiting.\n");
        return -1;
    }

    sink_pad = gst_element_get_static_pad(queue1, "sink");
    tee_msg_pad = gst_element_get_request_pad(tee, "src_%u");
    tee_render_pad = gst_element_get_request_pad(tee, "src_%u");
    if (!tee_msg_pad || !tee_render_pad) {
        g_printerr("Unable to get request pads\n");
        return -1;
    }

    if (gst_pad_link(tee_msg_pad, sink_pad) != GST_PAD_LINK_OK) {
        g_printerr("Unable to link tee and message converter\n");
        gst_object_unref(sink_pad);
        return -1;
    }

    gst_object_unref(sink_pad);

    sink_pad = gst_element_get_static_pad(queue2, "sink");
    if (gst_pad_link(tee_render_pad, sink_pad) != GST_PAD_LINK_OK) {
        g_printerr("Unable to link tee and render\n");
        gst_object_unref(sink_pad);
        return -1;
    }

    gst_object_unref(sink_pad);

//    nvdsanalytics_src_pad = gst_element_get_static_pad(nvdsanalytics, "src");
//    if (!nvdsanalytics_src_pad)
//        g_print("Unable to get src pad\n");
//    else {
//        if (msg2p_meta == 0)        //generate payload using eventMsgMeta
//            gst_pad_add_probe(nvdsanalytics_src_pad, GST_PAD_PROBE_TYPE_BUFFER,
//                              nvdsanalytics_src_pad_buffer_probe, NULL, NULL);
//    }

    /* Lets add probe to get informed of the meta data generated, we add probe to
     * the sink pad of the osd element, since by that time, the buffer would have
     * had got all the metadata. */
    osd_sink_pad = gst_element_get_static_pad(nvosd, "sink");
    if (!osd_sink_pad)
        g_print("Unable to get sink pad\n");
    else {
        if (msg2p_meta == 0)        //generate payload using eventMsgMeta
            gst_pad_add_probe(osd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER,
                              osd_sink_pad_buffer_probe, NULL, NULL);
    }
    gst_object_unref(osd_sink_pad);

    /* Set the pipeline to "playing" state */
    if (argc > 1 && IS_YAML (argv[1])) {
        g_print("Using file: %s\n", argv[1]);
    } else {
        g_print("Now playing:");
        for (i = 0; i < num_sources; i++) {
            g_print(" %s,", input_file[i]);
        }
        g_print("\n");
    }
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* Wait till pipeline encounters an error or EOS */
    g_print("Running...\n");
    g_main_loop_run(loop);

    /* Out of the main loop, clean up nicely */
    g_print("Returned, stopping playback\n");

    g_free(cfg_file);
    g_free(input_file);
    g_free(topic);
    g_free(conn_str);
    g_free(proto_lib);

    /* Release the request pads from the tee, and unref them */
    gst_element_release_request_pad(tee, tee_msg_pad);
    gst_element_release_request_pad(tee, tee_render_pad);
    gst_object_unref(tee_msg_pad);
    gst_object_unref(tee_render_pad);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_print("Deleting pipeline\n");
    gst_object_unref(GST_OBJECT (pipeline));
    g_source_remove(bus_watch_id);
    g_main_loop_unref(loop);
    return 0;
}
