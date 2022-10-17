/* GStreamer
 *
 * ## Example launch line
 * |[
 *  gst-launch-1.0 -v v4l2src device=/dev/video0 io-mode=mmap num-buffers=1 ! video/x-raw,format=YUY2,width=640,height=480 ! amljpegenc ! filesink location=/media/1.jpg
 * ]|
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <gst/allocators/gstdmabuf.h>
#include <gst/gst.h>
#include "gstamljpegenc.h"
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/base/base.h>
#include <gst/allocators/gstdmabuf.h>
#include "../common/gstamlionallocator.h"
#include "imgproc.h"

#define PROP_IDR_PERIOD_DEFAULT 30
#define PROP_FRAMERATE_DEFAULT 30
#define PROP_BITRATE_DEFAULT 2000
#define PROP_BITRATE_MAX 12000
#define PROP_MIN_BUFFERS_DEFAULT 2
#define PROP_MAX_BUFFERS_DEFAULT 6
#define PROP_ENCODER_BUFFER_SIZE_DEFAULT 2048
#define PROP_ENCODER_BUFFER_SIZE_MIN 1024
#define PROP_ENCODER_BUFFER_SIZE_MAX 4096
#define PROP_ROI_ID_DEFAULT 0
#define PROP_ROI_ENABLED_DEFAULT TRUE
#define PROP_ROI_WIDTH_DEFAULT 0.00
#define PROP_ROI_HEIGHT_DEFAULT 0.00
#define PROP_ROI_X_DEFAULT 0.00
#define PROP_ROI_Y_DEFAULT 0.00
#define PROP_ROI_QUALITY_DEFAULT 50

enum
{
  PROP_0,
  PROP_GOP,
  PROP_FRAMERATE,
  PROP_BITRATE,
  PROP_MIN_BUFFERS,
  PROP_MAX_BUFFERS,
  PROP_ENCODER_BUFSIZE,
  PROP_ROI_ID,
  PROP_ROI_ENABLED,
  PROP_ROI_WIDTH,
  PROP_ROI_HEIGHT,
  PROP_ROI_X,
  PROP_ROI_Y,
  PROP_ROI_QUALITY,
  PROD_ENABLE_DMALLOCATOR
};

GST_DEBUG_CATEGORY_STATIC (amljpegenc_debug);
#define GST_CAT_DEFAULT amljpegenc_debug
#define JPEG_DEFAULT_QUALITY 50
#define JPEG_DEFAULT_SMOOTHING 0
#define JPEG_DEFAULT_SNAPSHOT		FALSE
#define ALIGNE_64(a) (((a + 63) >> 6) << 6)

/* JpegEnc signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  /*PROP_0,*/
  PROP_QUALITY,
  PROP_SMOOTHING,
  PROP_SNAPSHOT
};

static void gst_amljpegenc_finalize (GObject * object);
static void gst_amljpegenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_amljpegenc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_amljpegenc_close_encoder (GstAmlJpegEnc * encoder);
static gboolean gst_amljpegenc_start (GstAmlJpegEnc * encoder);
static gboolean gst_amljpegenc_stop (GstAmlJpegEnc * encoder);
static gboolean gst_amljpegenc_init_encoder (GstAmlJpegEnc * encoder);
static gboolean gst_amljpegenc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static gboolean gst_amljpegenc_set_src_caps (GstAmlJpegEnc * encoder, GstCaps * caps);
static GstFlowReturn gst_amljpegenc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_amljpegenc_encode_frame (GstAmlJpegEnc * encoder,
    GstVideoCodecFrame * frame);/* add */
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

/* initialize parameter */
static void
gst_amljpegenc_init (GstAmlJpegEnc * encoder)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_ENCODER_SINK_PAD (encoder));
  encoder->width = PROP_ROI_WIDTH_DEFAULT;
  encoder->height = PROP_ROI_HEIGHT_DEFAULT;
  encoder->quality = 50;
  encoder->smoothing = JPEG_DEFAULT_SMOOTHING;
  encoder->snapshot = JPEG_DEFAULT_SNAPSHOT;
  encoder->handle = 0;
  encoder->encoder_bufsize = PROP_ENCODER_BUFFER_SIZE_DEFAULT * 1024;
}

/* gst_amljpegenc_close_encoder
 * @encoder:  Encoder which should close.
 *
 * Close jpegenc encoder.
 */
static void
gst_amljpegenc_close_encoder (GstAmlJpegEnc * encoder)
{
  GST_DEBUG_OBJECT (encoder, "enter close encoder");
  if (encoder->handle != 0) {
    jpegenc_destroy(encoder->handle);
    encoder->handle = 0;
  }

  if (encoder->imgproc.handle) {
    imgproc_deinit(encoder->imgproc.handle);
    encoder->imgproc.handle = NULL;
  }

}

/*
 * gst_amljpegenc_init_encoder
 * @encoder:  Encoder which should be initialized.
 *
 * Initialize jpegenc encoder.
 *
 */
static gboolean
gst_amljpegenc_init_encoder (GstAmlJpegEnc * encoder)
{
  GstVideoInfo *info;
  if (!encoder->input_state) {
    return FALSE;
  }

  info = &encoder->input_state->info;
  gst_amljpegenc_close_encoder (encoder);
  encoder->width = info->width;
  encoder->height = info->height;
  encoder->handle = jpegenc_init();

  if (encoder->handle == 0) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Can not initialize v encoder."), (NULL));
    return FALSE;
  }
  return TRUE;
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
gst_amljpegenc_set_format (GstVideoEncoder * video_enc,
    GstVideoCodecState * state)
{
  GstAmlJpegEnc *encoder = GST_AMLJPEGENC (video_enc);
  GstVideoInfo *info = &state->info;
  GstCaps *template_caps;
  GstCaps *allowed_caps = NULL;
  const gchar* allowed_mime_name = NULL;

  if (encoder->handle != 0) {
    GstVideoInfo *old = &encoder->input_state->info;
    if (info->finfo->format == old->finfo->format
        && info->width == old->width && info->height == old->height
        && info->fps_n == old->fps_n && info->fps_d == old->fps_d
        && info->par_n == old->par_n && info->par_d == old->par_d) {
      gst_video_codec_state_unref (encoder->input_state);
      encoder->input_state = gst_video_codec_state_ref (state);
      return TRUE;
    }
  }

  if (encoder->input_state)
     gst_video_codec_state_unref (encoder->input_state);
   encoder->input_state = gst_video_codec_state_ref (state);

  if (!gst_amljpegenc_set_src_caps (encoder, state->caps)) {
     gst_amljpegenc_close_encoder (encoder);
     return FALSE;
   }

  /* If the encoder is initialized, do not reinitialize it again if not
   * necessary */
  if (!gst_amljpegenc_init_encoder (encoder))  {
     GST_DEBUG_OBJECT (encoder, "not init encoder");
     return FALSE;
   }
     return TRUE;
}

/* gst_amljpegenc_set_src_caps
 * Returns: TRUE on success.
 */
static gboolean
gst_amljpegenc_set_src_caps (GstAmlJpegEnc * encoder, GstCaps * caps)
{
  GstCaps *outcaps;
  GstStructure *structure;
  GstVideoCodecState *state;
  GstTagList *tags;
  const gchar* mime_str = "image/jpeg";
  outcaps = gst_caps_new_empty_simple (mime_str);
  state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (encoder),
      outcaps, encoder->input_state);
  GST_DEBUG_OBJECT (encoder, "output caps: %" GST_PTR_FORMAT, state->caps);
  gst_video_codec_state_unref (state);
  return TRUE;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_amljpegenc_handle_frame (GstVideoEncoder * video_enc,
    GstVideoCodecFrame * frame)
{
  GstAmlJpegEnc *encoder = GST_AMLJPEGENC (video_enc);
  GstFlowReturn ret;

  if (G_UNLIKELY (encoder->handle == 0))
    goto not_inited;

  ret = gst_amljpegenc_encode_frame (encoder, frame);
  return ret;

not_inited:
  {
    GST_WARNING_OBJECT (encoder, "Got buffer before set_caps was called");
    return GST_FLOW_OK;
  }
}

static GstFlowReturn
gst_amljpegenc_encode_frame (GstAmlJpegEnc * encoder,
    GstVideoCodecFrame * frame)
{
  GstVideoInfo *info = &encoder->input_state->info;
  GstMapInfo map;
  gint encode_data_len = -1;
  int inbuf_info;
  int retbuf_info;
  int w_stride = (info->width % 8) == 0 ? info->width : (((info->width / 8)+1)*8);
  int h_stride = (info->height % 8) == 0 ? info->height : (((info->height / 8)+1)*8);

  if (G_UNLIKELY (encoder->handle == 0)) {
    if (frame)
      gst_video_codec_frame_unref (frame);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (frame) {
    if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
      GST_INFO_OBJECT (encoder, "Forcing key frame");
    }
  }

  GstMemory *memory = gst_buffer_get_memory(frame->input_buffer, 0);
  gboolean is_dmabuf = gst_is_dmabuf_memory(memory);
  gint fd = -1;
  GstMapInfo minfo;
  GstVideoFrame video_frame;
  guint8 *pixel = 1;
  gint mem_type = JPEGENC_DMA_BUFF;
  gint iformat = FMT_YUV420 ;
  iformat = GST_VIDEO_INFO_FORMAT(info);
  GST_DEBUG_OBJECT (encoder,"iformat=%d",iformat);
 /* match the iformat */
  switch ( GST_VIDEO_INFO_FORMAT(info) ) {
    case GST_VIDEO_FORMAT_RGB || GST_VIDEO_FORMAT_BGR:
    /*
     For the rgb format,need convert to NV12 via ge2d.
     new imageproc handle when RGB case.
  */
        encoder->imgproc.handle = imgproc_init();
    if (encoder->imgproc.handle == NULL) {
        GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
          ("Can not initialize imgproc."), (NULL));
      return FALSE;
    }
        encoder->imgproc.outbuf_size = (info->width * info->height * 3) / 2;
        GST_DEBUG_OBJECT (encoder,"iformat=%d",iformat);
        break;

    case GST_VIDEO_FORMAT_NV12:
        iformat = FMT_NV12 ;
        GST_DEBUG_OBJECT (encoder,"iformat=%d",iformat);
        break;

    case GST_VIDEO_FORMAT_NV21:
        iformat = FMT_NV21 ;
        GST_DEBUG_OBJECT (encoder,"iformat=%d",iformat);
        break;

    case GST_VIDEO_FORMAT_YUY2:
        iformat = FMT_YUV422_SINGLE ;
        GST_DEBUG_OBJECT (encoder,"iformat=%d",iformat);
        break;

    default:
        GST_DEBUG_OBJECT (encoder, "no NV12/YUY2/RGB/BGR iformat");
        break;
   }

  if (is_dmabuf) {
    fd = gst_dmabuf_memory_get_fd(memory);
    mem_type = JPEGENC_DMA_BUFF;
    GST_DEBUG_OBJECT (encoder,"mem_type=%d",mem_type);
    gst_memory_unref(memory);
  }  else {
    mem_type = JPEGENC_LOCAL_BUFF;
    GST_DEBUG_OBJECT (encoder,"mem_type=%d",mem_type);
    gst_memory_unref(memory);
    /*
      non dmabuf case,due to encoder driver only handle dmabuf,so need convert to dma buffer case below.
     */
    if (encoder->imgproc.input.memory == NULL) {
      encoder->imgproc.input.memory =
        gst_allocator_alloc(encoder->dmabuf_alloc, info->size, NULL);

      if (encoder->imgproc.input.memory == NULL) {
        return GST_FLOW_ERROR;
      }
      encoder->imgproc.input.fd = gst_dmabuf_memory_get_fd(encoder->imgproc.input.memory);
    }

    memory = encoder->imgproc.input.memory;
    fd = encoder->imgproc.input.fd;
    if (gst_memory_map(memory, &minfo, GST_MAP_WRITE)) {
      GstVideoFrame video_frame;
      gst_video_frame_map(&video_frame, info, frame->input_buffer, GST_MAP_READ);
      pixel = GST_VIDEO_FRAME_PLANE_DATA (&video_frame, 0);
      memcpy(minfo.data, pixel, info->size);
      gst_video_frame_unmap (&video_frame);
      gst_memory_unmap(memory, &minfo);
    }
  }

  if (encoder->imgproc.handle) {
    // format conversation needed
    if (encoder->dmabuf_alloc == NULL) {
      encoder->dmabuf_alloc = gst_amlion_allocator_obtain();
    }

    struct imgproc_buf inbuf, outbuf;
    inbuf.fd = fd;
    inbuf.is_ionbuf = gst_is_amlionbuf_memory(memory);

    if (encoder->imgproc.output.memory == NULL) {
      encoder->imgproc.output.memory = gst_allocator_alloc(
          encoder->dmabuf_alloc, encoder->imgproc.outbuf_size, NULL);
      if (encoder->imgproc.output.memory == NULL) {
        return GST_FLOW_ERROR;
      }
      encoder->imgproc.output.fd = gst_dmabuf_memory_get_fd(encoder->imgproc.output.memory);
    }

    fd = encoder->imgproc.output.fd;
    outbuf.fd = fd;
    outbuf.is_ionbuf = TRUE;
    struct imgproc_pos inpos = {
        0, 0, info->width, info->height, info->width, info->height};
    struct imgproc_pos outpos = {
        0, 0, info->width, info->height, info->width, info->height};

    imgproc_crop(encoder->imgproc.handle, inbuf, inpos,
                      GST_VIDEO_INFO_FORMAT(info), outbuf, outpos,
                      GST_VIDEO_FORMAT_NV12);
  }
    /*
      if want to see the parameters,please release the log.
     */

  int ret = jpegenc_encode(encoder->handle, info->width, info->height, w_stride, h_stride, encoder->quality, iformat, 0, mem_type, fd, pixel, encoder->outputbuf);

  if (0 == ret) {
    if (frame) {
      GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, ("Encode v frame failed."),
          ("gst_amlvencoder_encode return code=%d", encode_data_len));
      gst_video_codec_frame_unref (frame);
      return GST_FLOW_ERROR;
    } else {
      return GST_FLOW_EOS;
    }
  }

  if (frame) {
    gst_video_codec_frame_unref (frame);
  }

  //frame = gst_video_encoder_get_frame (GST_VIDEO_ENCODER (encoder), input_frame->system_frame_number);
  frame = gst_video_encoder_get_oldest_frame (GST_VIDEO_ENCODER (encoder));
  if (!frame) {
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  frame->output_buffer = gst_video_encoder_allocate_output_buffer(
      GST_VIDEO_ENCODER(encoder), ret);
  gst_buffer_map(frame->output_buffer, &map, GST_MAP_WRITE);
  memcpy (map.data, encoder->outputbuf, ret);
  gst_buffer_unmap (frame->output_buffer, &map);

  frame->dts = frame->pts;

  GST_DEBUG_OBJECT (encoder,
      "output: dts %" G_GINT64_FORMAT " pts %" G_GINT64_FORMAT,
      (gint64) frame->dts, (gint64) frame->pts);

  GST_DEBUG_OBJECT (encoder, "Output keyframe");
  GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
    return gst_video_encoder_finish_frame ( GST_VIDEO_ENCODER(encoder), frame);
}

static gboolean
gst_amljpegenc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
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
gst_amljpegenc_start (GstAmlJpegEnc * encoder)
{
  GstAmlJpegEnc *enc = (GstAmlJpegEnc *) encoder;

  enc->dmabuf_alloc = gst_amlion_allocator_obtain();

  GST_DEBUG_OBJECT(encoder, "malloc out_buf");

if (enc->outputbuf == NULL) {
    enc->outputbuf = g_new (guchar, enc->encoder_bufsize);
  }
  enc->imgproc.input.memory = NULL;
  enc->imgproc.output.memory = NULL;
  /* make sure that we have enough time for first DTS,
     this is probably overkill for most streams */
  gst_video_encoder_set_min_pts (encoder, GST_MSECOND * 30);

  GST_DEBUG_OBJECT(encoder, "malloc out_buf ok");
  return TRUE;
}

static gboolean
gst_amljpegenc_stop (GstAmlJpegEnc * encoder)
{
  GstAmlJpegEnc *enc = (GstAmlJpegEnc *) encoder;

 if (enc->outputbuf) {
    g_free((gpointer)enc->outputbuf);
    enc->outputbuf = NULL;
  }

  if (enc->dmabuf_alloc) {
    gst_object_unref(enc->dmabuf_alloc);
    enc->dmabuf_alloc = NULL;
  }

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
