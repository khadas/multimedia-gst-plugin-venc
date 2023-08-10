/* GStreamer
 */
#ifndef __GST_AMLJPEGENC_H__
#define __GST_AMLJPEGENC_H__
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>

#include "jpegenc_api.h"
#define SUPPORT_SCALE 1

#if SUPPORT_SCALE
#include "aml_ge2d.h"
#endif
G_BEGIN_DECLS

#define GST_TYPE_AMLJPEGENC \
  (gst_amljpegenc_get_type())
#define GST_AMLJPEGENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AMLJPEGENC,GstAmlJpegEnc))
#define GST_AMLJPEGENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AMLJPEGENC,GstAmlJpegEncClass))
#define GST_IS_AMLJPEGENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AMLJPEGENC))
#define GST_IS_AMLJPEGENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AMLJPEGENC))

typedef struct _GstAmlJpegEnc GstAmlJpegEnc;
typedef struct _GstAmlJpegEncClass GstAmlJpegEncClass;
typedef unsigned int uint32_t;

struct _GstAmlJpegEnc
{
  /*< private >*/
  GstVideoEncoder encoder;
  /*GstVideoFrame current_vframe;
  GstVideoCodecFrame *current_frame;
  GstFlowReturn res;
  gboolean input_caps_changed;
  guint channels;
  gint inc[GST_VIDEO_MAX_COMPONENTS];
  gint cwidth[GST_VIDEO_MAX_COMPONENTS];
  gint cheight[GST_VIDEO_MAX_COMPONENTS];
  gint h_samp[GST_VIDEO_MAX_COMPONENTS];
  gint v_samp[GST_VIDEO_MAX_COMPONENTS];
  gint h_max_samp;
  gint v_max_samp;
  gboolean planar;
  gint sof_marker;*/
  /* the video buffer */
  //gint bufsize;
  /* the jpeg line buffer */
  //guchar **line[3];
  /* indirect encoding line buffers */
  //guchar *row[3][4 * DCTSIZE];
  //guchar *row[3][4 * 2];

  /* properties */
  gint quality;
  gint smoothing;
  gboolean snapshot;
  guint width;
  guint height;
  jpegenc_handle_t handle;
  guint min_buffers;
  guint max_buffers;
  guint encoder_bufsize;
  //GstMemory *output_mem;
  //GstMapInfo output_map;
  guchar *outputbuf;

#if SUPPORT_SCALE
  guint ge2d_initial_done;
  aml_ge2d_t amlge2d;
  gboolean INIT_GE2D;
#endif
  gint fd[3];

  /* input description */
  GstVideoCodecState *input_state;
};

struct _GstAmlJpegEncClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_jpegenc_get_type (void);

G_END_DECLS
#endif /* __GST_JPEGENC_H__ */
