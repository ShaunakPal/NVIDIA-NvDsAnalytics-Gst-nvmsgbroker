//
// Created by kamoliddin on 4/30/23.
//


#ifndef DS_YOLOV5_NVDSA_PAD_PROBE_FUNCTIONS_H
#define DS_YOLOV5_NVDSA_PAD_PROBE_FUNCTIONS_H
#include "gstnvdsmeta.h"
#include "nvds_analytics_meta.h"
#include "deepstream_nvdsanalytics_meta.h"


extern  GstPadProbeReturn
        nvdsanalytics_src_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data);

extern GstPadProbeReturn
        osd_sink_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data);

#endif //DS_YOLOV5_NVDSA_PAD_PROBE_FUNCTIONS_H

