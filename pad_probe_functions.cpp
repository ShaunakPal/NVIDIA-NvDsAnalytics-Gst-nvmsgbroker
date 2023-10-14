//
// Created by kamoliddin on 4/30/23.
//

#include "analytics.h"
#include "gst-nvmessage.h"
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
#include "pad_probe_functions.h"
#define PGIE_CLASS_ID_VEHICLE 0
#define PGIE_CLASS_ID_PERSON 2

gint frame_number = 0;

/* nvdsanalytics_src_pad_buffer_probe  will extract metadata received on tiler sink pad
 * and extract nvanalytics metadata etc. */
extern GstPadProbeReturn
nvdsanalytics_src_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info,
                                   gpointer u_data) {
    GstBuffer *buf = (GstBuffer *) info->data;
    guint num_rects = 0;
    NvDsObjectMeta *obj_meta = NULL;
    guint vehicle_count = 0;
    guint person_count = 0;
    NvDsMetaList *l_frame = NULL;
    NvDsMetaList *l_obj = NULL;
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
         l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *) (l_frame->data);
        std::stringstream out_string;
        vehicle_count = 0;
        num_rects = 0;
        person_count = 0;
        for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;
             l_obj = l_obj->next) {
            obj_meta = (NvDsObjectMeta *) (l_obj->data);
            if (obj_meta->class_id == PGIE_CLASS_ID_VEHICLE) {
                vehicle_count++;
                num_rects++;
            }
            if (obj_meta->class_id == PGIE_CLASS_ID_PERSON) {
                person_count++;
                num_rects++;
            }

            // Access attached user meta for each object
            for (NvDsMetaList *l_user_meta = obj_meta->obj_user_meta_list; l_user_meta != NULL;
                 l_user_meta = l_user_meta->next) {
                NvDsUserMeta *user_meta = (NvDsUserMeta *) (l_user_meta->data);
                if (user_meta->base_meta.meta_type == NVDS_USER_OBJ_META_NVDSANALYTICS) {
                    NvDsAnalyticsObjInfo *user_meta_data = (NvDsAnalyticsObjInfo *) user_meta->user_meta_data;
                    if (user_meta_data->dirStatus.length()) {
                        g_print("object %lu moving in %s\n", obj_meta->object_id, user_meta_data->dirStatus.c_str());
                    }
                }
            }
        }

        /* Iterate user metadata in frames to search analytics metadata */
        for (NvDsMetaList *l_user = frame_meta->frame_user_meta_list;
             l_user != NULL; l_user = l_user->next) {

            /* Check for user metadata of type nvdsanalytics */
            AnalyticsUserMeta *user_data =
                    (AnalyticsUserMeta *) g_malloc0(sizeof(AnalyticsUserMeta));
            if (l_user != NULL) {
                /* Get the user metadata */
                analytics_custom_parse_nvdsanalytics_meta_data(l_user, user_data);
            }
            NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;
            if (user_meta->base_meta.meta_type != NVDS_USER_FRAME_META_NVDSANALYTICS)
                continue;

            /* convert to  metadata */
            NvDsAnalyticsFrameMeta *meta =
                    (NvDsAnalyticsFrameMeta *) user_meta->user_meta_data;
            /* Get the labels from nvdsanalytics config file */
            for (std::pair<std::string, uint32_t> status: meta->objInROIcnt) {
                out_string << "Objs in ROI ";
                out_string << status.first;
                out_string << " = ";
                out_string << status.second;
            }
            for (std::pair<std::string, uint32_t> status: meta->objLCCumCnt) {
                out_string << " LineCrossing Cumulative ";
                out_string << status.first;
                out_string << " = ";
                out_string << status.second;
            }
            for (std::pair<std::string, uint32_t> status: meta->objLCCurrCnt) {
                out_string << " LineCrossing Current Frame ";
                out_string << status.first;
                out_string << " = ";
                out_string << status.second;
            }
            for (std::pair<std::string, bool> status: meta->ocStatus) {
                out_string << " Overcrowding status ";
                out_string << status.first;
                out_string << " = ";
                out_string << status.second;
            }
        }
        g_print("Frame Number = %d of Stream = %d, Number of objects = %d "
                "Vehicle Count = %d Person Count = %d %s\n",
                frame_meta->frame_num, frame_meta->pad_index,
                num_rects, vehicle_count, person_count, out_string.str().c_str());
    }
    return GST_PAD_PROBE_OK;
}

/* osd_sink_pad_buffer_probe  will extract metadata received on OSD sink pad
 * and update params for drawing rectangle, object information etc. */

extern GstPadProbeReturn
osd_sink_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info,
                          gpointer u_data) {
    GstBuffer *buf = (GstBuffer *) info->data;
    NvDsFrameMeta *frame_meta = NULL;
    NvOSD_TextParams *txt_params = NULL;
    guint vehicle_count = 0;
    guint person_count = 0;
    gboolean is_first_object = TRUE;
    NvDsMetaList *l_frame, *l_obj;

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta) {
        // No batch meta attached.
        return GST_PAD_PROBE_OK;
    }

    for (l_frame = batch_meta->frame_meta_list; l_frame; l_frame = l_frame->next) {
        frame_meta = (NvDsFrameMeta *) l_frame->data;
        std::stringstream out_string;
        if (frame_meta == NULL) {
            // Ignore Null frame meta.
            continue;
        }

        is_first_object = TRUE;

        /* Iterate user metadata in frames to search analytics metadata */
        for (NvDsMetaList *l_user = frame_meta->frame_user_meta_list;
             l_user != NULL; l_user = l_user->next) {
            /* Check for user metadata of type nvdsanalytics */
            AnalyticsUserMeta *user_data =
                    (AnalyticsUserMeta *) g_malloc0(sizeof(AnalyticsUserMeta));
            if (l_user != NULL) {
                /* Get the user metadata */
                analytics_custom_parse_nvdsanalytics_meta_data(l_user, user_data);
            }
            NvDsUserMeta *user_meta = (NvDsUserMeta *) l_user->data;
            if (user_meta->base_meta.meta_type != NVDS_USER_FRAME_META_NVDSANALYTICS)
                continue;
            NvDsEventMsgMeta *msg_meta =
                    (NvDsEventMsgMeta *) g_malloc0(sizeof(NvDsEventMsgMeta));
            msg_meta->frameId = frame_number;

            generate_event_msg_meta(msg_meta, user_data);

            NvDsUserMeta *user_event_meta =
                        nvds_acquire_user_meta_from_pool(batch_meta);


                if (user_event_meta) {
                    user_event_meta->user_meta_data = (void *) msg_meta;
                    user_event_meta->base_meta.meta_type = NVDS_EVENT_MSG_META;
                    user_event_meta->base_meta.copy_func =
                            (NvDsMetaCopyFunc) meta_copy_func;
                    user_event_meta->base_meta.release_func =
                            (NvDsMetaReleaseFunc) meta_free_func;
                    //disable sending message to
                    //nvds_add_user_meta_to_frame(frame_meta, user_event_meta);
                } else {
                    g_print("Error in attaching event meta to buffer\n");
                }

        }



    }
    return GST_PAD_PROBE_OK;
}