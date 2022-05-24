/* GStreamer
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 videotestsrc num-buffers=50 ! video/x-raw, framerate='(fraction)'5/1 ! jpegenc ! avimux ! filesink location=mjpeg.avi
 * ]| a pipeline to mux 5 JPEG frames per second into a 10 sec. long motion jpeg
 * avi.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>

#include <gst/gst.h>
//#include "gstjpeg.h"
#include "gstamljpegenc.h"
//#include "gstjpegelements.h"
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/base/base.h>

/* experimental */
/* setting smoothig seems to have no effect in libjepeg
#define ENABLE_SMOOTHING 1
*/

GST_DEBUG_CATEGORY_STATIC (amljpegenc_debug);
#define GST_CAT_DEFAULT amljpegenc_debug

#define JPEG_DEFAULT_QUALITY 85
#define JPEG_DEFAULT_SMOOTHING 0
#define JPEG_DEFAULT_SNAPSHOT		FALSE

/* JpegEnc signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_QUALITY,
  PROP_SMOOTHING,
  PROP_SNAPSHOT
};

static void gst_amljpegenc_finalize (GObject * object);

static void gst_amljpegenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_amljpegenc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_amljpegenc_start (GstVideoEncoder * benc);
static gboolean gst_amljpegenc_stop (GstVideoEncoder * benc);
static gboolean gst_amljpegenc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_amljpegenc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static gboolean gst_amljpegenc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);

/* static guint gst_amljpegenc_signals[LAST_SIGNAL] = { 0 }; */

#define gst_amljpegenc_parent_class parent_class
G_DEFINE_TYPE (GstAmlJpegEnc, gst_amljpegenc, GST_TYPE_VIDEO_ENCODER);

/* *INDENT-OFF* */
static GstStaticPadTemplate gst_amljpegenc_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ I420, YV12, YUY2, UYVY, Y41B, Y42B, YVYU, Y444, NV21, "
         "NV12, RGB, BGR, RGBx, xRGB, BGRx, xBGR, GRAY8 }"))
    );
/* *INDENT-ON* */

static GstStaticPadTemplate gst_amljpegenc_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg, "
        "width = (int) [ 1, 65535 ], "
        "height = (int) [ 1, 65535 ], "
        "framerate = (fraction) [ 0/1, MAX ], "
        "sof-marker = (int) { 0, 1, 2, 4, 9 }")
    );

static void
gst_amljpegenc_class_init (GstAmlJpegEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *venc_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  venc_class = (GstVideoEncoderClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_amljpegenc_finalize;
  gobject_class->set_property = gst_amljpegenc_set_property;
  gobject_class->get_property = gst_amljpegenc_get_property;

  g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_int ("quality", "Quality", "Quality of encoding",
          0, 100, JPEG_DEFAULT_QUALITY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

#ifdef ENABLE_SMOOTHING
  /* disabled, since it doesn't seem to work */
  g_object_class_install_property (gobject_class, PROP_SMOOTHING,
      g_param_spec_int ("smoothing", "Smoothing", "Smoothing factor",
          0, 100, JPEG_DEFAULT_SMOOTHING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  /**
   * GstAmlJpegEnc:snapshot:
   *
   * Send EOS after encoding a frame, useful for snapshots.
   *
   * Since: 1.14
   */
  g_object_class_install_property (gobject_class, PROP_SNAPSHOT,
      g_param_spec_boolean ("snapshot", "Snapshot",
          "Send EOS after encoding a frame, useful for snapshots",
          JPEG_DEFAULT_SNAPSHOT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class,
      &gst_amljpegenc_sink_pad_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_amljpegenc_src_pad_template);

  gst_element_class_set_static_metadata (element_class, "Amlogic JPEG image encoder",
      "Codec/Encoder/Image", "Encode images in JPEG format",
      "Xiaobo.Wang@amlogic.com");

  venc_class->start = gst_amljpegenc_start;
  venc_class->stop = gst_amljpegenc_stop;
  venc_class->set_format = gst_amljpegenc_set_format;
  venc_class->handle_frame = gst_amljpegenc_handle_frame;
  venc_class->propose_allocation = gst_amljpegenc_propose_allocation;

  GST_DEBUG_CATEGORY_INIT (amljpegenc_debug, "jpegenc", 0,
      "JPEG encoding element");
}

static void
gst_amljpegenc_init (GstAmlJpegEnc * jpegenc)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_ENCODER_SINK_PAD (jpegenc));

  /* init properties */
  jpegenc->quality = JPEG_DEFAULT_QUALITY;
  jpegenc->smoothing = JPEG_DEFAULT_SMOOTHING;
  jpegenc->snapshot = JPEG_DEFAULT_SNAPSHOT;
}

static void
gst_amljpegenc_finalize (GObject * object)
{
  GstAmlJpegEnc *filter = GST_AMLJPEGENC (object);

  if (filter->input_state)
    gst_video_codec_state_unref (filter->input_state);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_amljpegenc_set_format (GstVideoEncoder * encoder, GstVideoCodecState * state)
{
    return TRUE;
}

static GstFlowReturn
gst_amljpegenc_handle_frame (GstVideoEncoder * encoder, GstVideoCodecFrame * frame)
{
    return GST_FLOW_OK;
}

static gboolean
gst_amljpegenc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder,
      query);
}

static void
gst_amljpegenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAmlJpegEnc *jpegenc = GST_AMLJPEGENC (object);

  GST_OBJECT_LOCK (jpegenc);

  switch (prop_id) {
    case PROP_QUALITY:
      jpegenc->quality = g_value_get_int (value);
      break;
#ifdef ENABLE_SMOOTHING
    case PROP_SMOOTHING:
      jpegenc->smoothing = g_value_get_int (value);
      break;
#endif
    case PROP_SNAPSHOT:
      jpegenc->snapshot = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (jpegenc);
}

static void
gst_amljpegenc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstAmlJpegEnc *jpegenc = GST_AMLJPEGENC (object);

  GST_OBJECT_LOCK (jpegenc);

  switch (prop_id) {
    case PROP_QUALITY:
      g_value_set_int (value, jpegenc->quality);
      break;
#ifdef ENABLE_SMOOTHING
    case PROP_SMOOTHING:
      g_value_set_int (value, jpegenc->smoothing);
      break;
#endif
    case PROP_SNAPSHOT:
      g_value_set_boolean (value, jpegenc->snapshot);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (jpegenc);
}

static gboolean
gst_amljpegenc_start (GstVideoEncoder * benc)
{
  GstAmlJpegEnc *enc = (GstAmlJpegEnc *) benc;

  enc->line[0] = NULL;
  enc->line[1] = NULL;
  enc->line[2] = NULL;
  enc->sof_marker = -1;

  return TRUE;
}

static gboolean
gst_amljpegenc_stop (GstVideoEncoder * benc)
{
  GstAmlJpegEnc *enc = (GstAmlJpegEnc *) benc;
  g_free (enc->line[0]);
  g_free (enc->line[1]);
  g_free (enc->line[2]);
  enc->line[0] = NULL;
  enc->line[1] = NULL;
  enc->line[2] = NULL;

  return TRUE;
}

#ifndef VERSION
#define VERSION "1.0.0"
#endif

#ifndef PACKAGE
#define PACKAGE "aml_package"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "aml_package"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://amlogic.com/"
#endif

static gboolean
amljpeg_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (amljpegenc_debug, "amljpegenc", 0,
      "amlogic JPEG encoding element");

  //Fix me,current rank none.
  return gst_element_register (plugin, "amljpegenc", GST_RANK_NONE,
      GST_TYPE_AMLJPEGENC);
}

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    amljpegenc,
    "Amlogic JPEG encoder plugins",
    amljpeg_init,
    VERSION,
    "LGPL",
    "amlogic JPEG ecoding",
    "http://openlinux.amlogic.com"
)
