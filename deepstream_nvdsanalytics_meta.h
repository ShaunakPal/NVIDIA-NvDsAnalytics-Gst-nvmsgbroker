//
// Created by kamoliddin on 4/30/23.
//

#ifndef DS_YOLOV5_NVDSA_DEEPSTREAM_NVDSANALYTICS_META_H
#define DS_YOLOV5_NVDSA_DEEPSTREAM_NVDSANALYTICS_META_H



#include <gstnvdsmeta.h>
#include "nvds_analytics_meta.h"
#include "analytics.h"

void analytics_custom_parse_nvdsanalytics_meta_data(NvDsMetaList *l_user, AnalyticsUserMeta *data);

#endif //DS_YOLOV5_NVDSA_DEEPSTREAM_NVDSANALYTICS_META_H
