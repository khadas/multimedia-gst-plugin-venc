/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 */

#ifndef __GST_AMLVENC_H__
#define __GST_AMLVENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>
//#include <list.h>

#include "list.h"
#include "vp_hevc_codec_1_0.h"

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
//#include <vp_multi_codec_1_0.h>

#define PTS_UINT_4_RESET 4294967295

G_BEGIN_DECLS

#define GST_TYPE_AMLH265VENC \
    (gst_amlh265venc_get_type())
#define GST_AMLH265VENC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMLH265VENC,GstAmlH265VEnc))
#define GST_AMLH265VENC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMLH265VENC,GstAmlH265VEncClass))
#define GST_IS_AMLH265VENC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMLH265VENC))
#define GST_IS_AMLH64VENC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMLH265VENC))

typedef struct _GstAmlH265VEnc      GstAmlH265VEnc;
typedef struct _GstAmlH265VEncClass GstAmlH265VEncClass;

typedef struct qp_param_s {
  gint qp_min;
  gint qp_max;
  gint qp_I_base;
  gint qp_P_base;
  gint qp_I_min;
  gint qp_I_max;
  gint qp_P_min;
  gint qp_P_max;
} qp_param_t;

typedef struct _encode_info {
  gint width;
  gint height;
  gint frame_rate;
  gint bit_rate;
  gint gop;
  vl_img_format_t img_format;
} vl_encode_info_t;

typedef enum
{
    VMALLOC_BUFFER = 0,
    CANVAS_BUFFER = 1,
    PHYSICAL_BUFF = 2,
    DMA_BUFF = 3,
    MAX_TYPE = 4,
} AML_ENC_E;


struct _GstAmlH265VEnc
{
  GstVideoEncoder element;

  /*< private >*/
  struct codec_info {
    vl_codec_handle_t handle;
    vl_codec_id_t id;
    guchar *buf;
  } codec;

  struct imgproc_info {
    void *handle;
    gint outbuf_size;
    gint width;
    gint height;
    struct {
      GstMemory *memory;
      gint fd;
    } input, output;
  } imgproc;

  GstAllocator *dmabuf_alloc;

  /* properties */
  gint gop;
  gint framerate;
  guint bitrate;
  guint min_buffers;
  guint max_buffers;
  guint encoder_bufsize;
  guint u4_first_pts_index;
  gboolean b_enable_dmallocator;

  struct roi_info {
    guint srcid;
    gboolean enabled;
    gint id;
    gint block_size;
    struct listnode param_info;
    struct _buffer_info {
      gint width;
      gint height;
      guchar *data;
    } buffer_info;
  } roi;

  /* input description */
  GstVideoCodecState *input_state;

};

struct _GstAmlH265VEncClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_amlh265venc_get_type (void);

G_END_DECLS

#endif /* __GST_AMLVENC_H__ */
