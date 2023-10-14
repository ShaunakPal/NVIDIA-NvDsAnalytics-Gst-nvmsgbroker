//
// Created by kamoliddin on 4/30/23.
//

#ifndef DS_YOLOV5_NVDSA_MSG_GENERATE_H
#define DS_YOLOV5_NVDSA_MSG_GENERATE_H

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
#include <glib-2.0/glib/gstrfuncs.h>
#include <glib-2.0/glib/gmem.h>
#include <glib-2.0/glib/gutils.h>
#include "gstnvdsmeta.h"
#include "nvdsmeta_schema.h"
#include "nvds_yml_parser.h"
#include "nvds_analytics_meta.h"
#include "deepstream_nvdsanalytics_meta.h"
#include "msg_generate.h"
#include "gst-nvmessage.h"
#include "analytics.h"

void  generate_event_msg_meta(gpointer data, AnalyticsUserMeta *user_meta);

extern gpointer  meta_copy_func(gpointer data, gpointer user_data);

extern void meta_free_func(gpointer data, gpointer user_data);

#endif //DS_YOLOV5_NVDSA_MSG_GENERATE_H
