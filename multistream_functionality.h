//
// Created by kamoliddin on 4/30/23.
//

#ifndef DS_YOLOV5_NVDSA_MULTISTREAM_FUNCTIONALITY_H
#define DS_YOLOV5_NVDSA_MULTISTREAM_FUNCTIONALITY_H

 void
cb_newpad(GstElement *decodebin, GstPad *decoder_src_pad, gpointer data);
 void
decodebin_child_added(GstChildProxy *child_proxy, GObject *object,
                      gchar *name, gpointer user_data);
 GstElement *
create_source_bin(guint index, gchar *uri);

 gboolean
bus_call(GstBus *bus, GstMessage *msg, gpointer data);
#endif //DS_YOLOV5_NVDSA_MULTISTREAM_FUNCTIONALITY_H
