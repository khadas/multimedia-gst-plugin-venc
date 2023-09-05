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
#include <gst/gstdrmbufferpool.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/base/base.h>
#include <gst/allocators/gstdmabuf.h>

#include "gstamljpegenc.h"

#define PROP_IDR_PERIOD_DEFAULT 30
#define PROP_FRAMERATE_DEFAULT 30
#define PROP_BITRATE_DEFAULT 2000
#define PROP_BITRATE_MAX 12000
#define PROP_MIN_BUFFERS_DEFAULT 2
#define PROP_MAX_BUFFERS_DEFAULT 6
#define PROP_ENCODER_BUFFER_SIZE_DEFAULT 2048
#define PROP_ENCODER_BUFFER_SIZE_MIN 1024
#define PROP_ENCODER_BUFFER_SIZE_MAX 4096

enum
{
  PROP_0,
  PROP_GOP,
  PROP_FRAMERATE,
  PROP_BITRATE,
  PROP_MIN_BUFFERS,
  PROP_MAX_BUFFERS,
  PROP_ENCODER_BUFSIZE,
};

GST_DEBUG_CATEGORY_STATIC (amljpegenc_debug);
#define GST_CAT_DEFAULT amljpegenc_debug
#define JPEG_DEFAULT_QUALITY 50
#define JPEG_DEFAULT_SMOOTHING 0
#define JPEG_DEFAULT_SNAPSHOT		FALSE
#define PROP_MIN_BUFFERS_DEFAULT 2
#define PROP_MAX_BUFFERS_DEFAULT 6

#define ALIGNE_64(a) (((a + 63) >> 6) << 6)

#define DRMBP_EXTRA_BUF_SIZE_FOR_DISPLAY 1
#define DRMBP_LIMIT_MAX_BUFSIZE_TO_BUFSIZE 1

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

#if SUPPORT_SCALE
static gint SRC1_PIXFORMAT = PIXEL_FORMAT_YCrCb_420_SP;
static gint SRC2_PIXFORMAT = PIXEL_FORMAT_YCrCb_420_SP;
static gint DST_PIXFORMAT = PIXEL_FORMAT_YCrCb_420_SP;

static GE2DOP OP = AML_GE2D_STRETCHBLIT;

#define g_align32(a)     ((((a)+31)>>5)<<5)

static gint do_strechblit(aml_ge2d_info_t* pge2dinfo, GstVideoInfo *info)
{
    gint ret = -1;
    pge2dinfo->src_info[0].memtype = GE2D_CANVAS_ALLOC;
    pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;
    pge2dinfo->src_info[0].canvas_w = info->width;
    pge2dinfo->src_info[0].canvas_h = info->height;
    pge2dinfo->src_info[0].format = SRC1_PIXFORMAT;

    pge2dinfo->dst_info.canvas_w = g_align32(info->width);
    pge2dinfo->dst_info.canvas_h = info->height;
    pge2dinfo->dst_info.format = DST_PIXFORMAT;

    pge2dinfo->src_info[0].rect.x = 0;
    pge2dinfo->src_info[0].rect.y = 0;
    pge2dinfo->src_info[0].rect.w = info->width;
    pge2dinfo->src_info[0].rect.h = info->height;
    pge2dinfo->dst_info.rect.x = 0;
    pge2dinfo->dst_info.rect.y = 0;
    pge2dinfo->dst_info.rect.w = g_align32(info->width);
    pge2dinfo->dst_info.rect.h = info->height;
    pge2dinfo->dst_info.rotation = GE2D_ROTATION_0;
    ret = aml_ge2d_process(pge2dinfo);
    return ret;
}

static void set_ge2dinfo(aml_ge2d_info_t* pge2dinfo,
                         GstVideoInfo *info)
{
    pge2dinfo->src_info[0].memtype = GE2D_CANVAS_ALLOC;
    pge2dinfo->src_info[0].canvas_w = info->width;
    pge2dinfo->src_info[0].canvas_h = info->height;
    pge2dinfo->src_info[0].format = SRC1_PIXFORMAT;
    pge2dinfo->src_info[1].memtype = GE2D_CANVAS_TYPE_INVALID;
    pge2dinfo->src_info[1].canvas_w = 0;
    pge2dinfo->src_info[1].canvas_h = 0;
    pge2dinfo->src_info[1].format = SRC2_PIXFORMAT;
    pge2dinfo->dst_info.memtype = GE2D_CANVAS_ALLOC;
    pge2dinfo->dst_info.canvas_w = g_align32(info->width);
    pge2dinfo->dst_info.canvas_h = info->height;
    pge2dinfo->dst_info.format = DST_PIXFORMAT;
    pge2dinfo->dst_info.rotation = GE2D_ROTATION_0;
    pge2dinfo->offset = 0;
    pge2dinfo->ge2d_op = OP;
    pge2dinfo->blend_mode = BLEND_MODE_PREMULTIPLIED;
}

static gboolean ge2d_colorFormat(GstVideoFormat vfmt)
{
    switch (vfmt) {
        case GST_VIDEO_FORMAT_BGR:
            SRC1_PIXFORMAT = PIXEL_FORMAT_BGR_888;
            SRC2_PIXFORMAT = PIXEL_FORMAT_BGR_888;
            return TRUE;
        default:
            return FALSE;
    }
}
#endif

static jpegenc_frame_fmt_e img_format_convert (GstVideoFormat vfmt)
{
    jpegenc_frame_fmt_e fmt;
    switch (vfmt) {
        case GST_VIDEO_FORMAT_NV12:
            fmt = FMT_NV12;
            break;
        case GST_VIDEO_FORMAT_NV21:
            fmt = FMT_NV21;
            break;
        case GST_VIDEO_FORMAT_I420:
        case GST_VIDEO_FORMAT_YV12:
            fmt = FMT_YUV420;
            break;
        case GST_VIDEO_FORMAT_RGB:
            fmt = FMT_RGB888;
            break;
        case GST_VIDEO_FORMAT_BGR:
            fmt = FMT_NV21;
            break;
        default:
            fmt = FMT_NV12;
            break;
    }
    return fmt;
}

/* static guint gst_amljpegenc_signals[LAST_SIGNAL] = { 0 }; */

#define gst_amljpegenc_parent_class parent_class
G_DEFINE_TYPE (GstAmlJpegEnc, gst_amljpegenc, GST_TYPE_VIDEO_ENCODER);

/* *INDENT-OFF* */
static GstStaticPadTemplate gst_amljpegenc_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ NV12, NV21, I420, "
         "YV12, RGB, BGR}"))
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
  venc_class->start = gst_amljpegenc_start;
  venc_class->stop = gst_amljpegenc_stop;
  venc_class->set_format = gst_amljpegenc_set_format;
  venc_class->handle_frame = gst_amljpegenc_handle_frame;
  venc_class->propose_allocation = gst_amljpegenc_propose_allocation;

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


  GST_DEBUG_CATEGORY_INIT (amljpegenc_debug, "jpegenc", 0,
      "JPEG encoding element");
}

/* initialize parameter */
static void
gst_amljpegenc_init (GstAmlJpegEnc * encoder)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_ENCODER_SINK_PAD (encoder));
  encoder->quality = 50;
  encoder->smoothing = JPEG_DEFAULT_SMOOTHING;
  encoder->snapshot = JPEG_DEFAULT_SNAPSHOT;
  encoder->handle = 0;
  encoder->max_buffers = PROP_MAX_BUFFERS_DEFAULT;
  encoder->min_buffers = PROP_MIN_BUFFERS_DEFAULT;
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
#if SUPPORT_SCALE
    if (encoder->ge2d_initial_done) {
        aml_ge2d_mem_free(&encoder->amlge2d);
        aml_ge2d_exit(&encoder->amlge2d);
        GST_DEBUG_OBJECT(encoder, "ge2d exit!!!");
    }
#endif
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
#if SUPPORT_SCALE
  if (GST_VIDEO_INFO_FORMAT(info) == GST_VIDEO_FORMAT_BGR) {
      memset(&encoder->amlge2d,0x0,sizeof(aml_ge2d_t));
      aml_ge2d_info_t *pge2dinfo = &encoder->amlge2d.ge2dinfo;
      memset(pge2dinfo, 0, sizeof(aml_ge2d_info_t));
      memset(&(pge2dinfo->src_info[0]), 0, sizeof(buffer_info_t));
      memset(&(pge2dinfo->src_info[1]), 0, sizeof(buffer_info_t));
      memset(&(pge2dinfo->dst_info), 0, sizeof(buffer_info_t));

      set_ge2dinfo(pge2dinfo, info);

      gint ret = aml_ge2d_init(&encoder->amlge2d);
      if (ret < 0) {
          GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
              ("encode open ge2d failed"), (NULL));
          return FALSE;
      }
      encoder->ge2d_initial_done = 1;
      encoder->INIT_GE2D = TRUE;
  }
#endif
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
  GstVideoCodecState *state;
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
  guint8 ui1_plane_num = 1;

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

  GST_DEBUG_OBJECT(encoder, "is_dmabuf[%d] width[%d] height[%d]",is_dmabuf,info->width,info->height);
  guint8 *pixel = NULL;
  int datalen = 0;
  jpegenc_result_e result = ENC_FAILED;
  jpegenc_frame_info_t frame_info;

  memset(&frame_info, 0, sizeof(frame_info));
  frame_info.iformat = img_format_convert(GST_VIDEO_INFO_FORMAT(info));

#if SUPPORT_SCALE
  if (GST_VIDEO_INFO_FORMAT(info) == GST_VIDEO_FORMAT_BGR) {
      if (encoder->INIT_GE2D) {
          if (ge2d_colorFormat(GST_VIDEO_INFO_FORMAT(info)) == TRUE) {
              GST_DEBUG_OBJECT(encoder, "The color format that venc need ge2d to change!");
          } else {
              GST_DEBUG_OBJECT(encoder, "Encoder only not support fmt: %d", GST_VIDEO_INFO_FORMAT(info));
              if (frame)
                  gst_video_codec_frame_unref (frame);
              return GST_FLOW_ERROR;
          }
      }
  }
#endif

  if (is_dmabuf) {
      encoder->fd[0] = gst_dmabuf_memory_get_fd(memory);
      gst_memory_unref(memory);
      switch (GST_VIDEO_INFO_FORMAT(info)) {
          case GST_VIDEO_FORMAT_NV12:
          case GST_VIDEO_FORMAT_NV21:
              {
                  /* handle dma case scenario media convet encoder/hdmi rx encoder scenario*/
                  GstMemory *memory_uv = gst_buffer_get_memory(frame->input_buffer, 1);
                  encoder->fd[1] = gst_dmabuf_memory_get_fd(memory_uv);
                  gst_memory_unref(memory_uv);
                  ui1_plane_num = 2;
                  break;
              }
          default: //hanle I420/YV12/RGB
              {
                  /*
                  Currently,For 420sp and RGB case,use one plane.
                  420sp for usb camera case,usb camera y/u/v address is continuous and no alignment requirement.
                  Therefore,use one plane.
                  */
                  ui1_plane_num = 1;
                  break;
              }
      }
#if SUPPORT_SCALE
      if (GST_VIDEO_INFO_FORMAT(info) == GST_VIDEO_FORMAT_BGR) {
          if (encoder->INIT_GE2D) {
              encoder->INIT_GE2D = FALSE;
              encoder->amlge2d.ge2dinfo.src_info[0].format = SRC1_PIXFORMAT;
              encoder->amlge2d.ge2dinfo.src_info[1].format = SRC2_PIXFORMAT;
              encoder->amlge2d.ge2dinfo.dst_info.plane_number = 1;
              gint ret = aml_ge2d_mem_alloc(&encoder->amlge2d);
              if (ret < 0) {
                  GST_DEBUG_OBJECT(encoder, "encode ge2d mem alloc failed, ret=0x%x", ret);
                  if (frame)
                      gst_video_codec_frame_unref (frame);
                  return GST_FLOW_ERROR;
              }
              GST_DEBUG_OBJECT(encoder, "ge2d init successful!");
          }
          encoder->amlge2d.ge2dinfo.src_info[0].shared_fd[0] = encoder->fd[0];

          do_strechblit(&encoder->amlge2d.ge2dinfo, info);
          aml_ge2d_invalid_cache(&encoder->amlge2d.ge2dinfo);

          ui1_plane_num = 1;
          frame_info.mem_type = JPEGENC_DMA_BUFF;
          frame_info.YCbCr[0] = encoder->amlge2d.ge2dinfo.dst_info.shared_fd[0];
          frame_info.plane_num = ui1_plane_num;
          encoder->amlge2d.ge2dinfo.src_info[0].shared_fd[0] = -1;
          GST_DEBUG_OBJECT(encoder, "Set DMA buffer planes %d fd[0x%lx]",
              frame_info.plane_num, frame_info.YCbCr[0]);
      } else
#endif
      {
          frame_info.mem_type = JPEGENC_DMA_BUFF;
          frame_info.YCbCr[0] = encoder->fd[0];
          frame_info.YCbCr[1] = encoder->fd[1];
          frame_info.YCbCr[2] = encoder->fd[2];
          frame_info.plane_num = ui1_plane_num;
          GST_DEBUG_OBJECT(encoder, "Set DMA buffer planes %d fd[0x%lx, 0x%lx, 0x%lx]",
              frame_info.plane_num, frame_info.YCbCr[0],
              frame_info.YCbCr[1], frame_info.YCbCr[2]);
      }
  } else {
      gst_memory_unref(memory);
      GstVideoFrame video_frame;
      gst_video_frame_map(&video_frame, info, frame->input_buffer, GST_MAP_READ);

      pixel = GST_VIDEO_FRAME_PLANE_DATA (&video_frame, 0);
#if SUPPORT_SCALE
      if (GST_VIDEO_INFO_FORMAT(info) == GST_VIDEO_FORMAT_BGR) {
          if (encoder->INIT_GE2D) {
              encoder->INIT_GE2D = FALSE;

              encoder->amlge2d.ge2dinfo.src_info[0].format = SRC1_PIXFORMAT;
              encoder->amlge2d.ge2dinfo.src_info[1].format = SRC2_PIXFORMAT;
              encoder->amlge2d.ge2dinfo.src_info[0].plane_number = 1;
              encoder->amlge2d.ge2dinfo.dst_info.plane_number = 1;
              gint ret = aml_ge2d_mem_alloc(&encoder->amlge2d);
              if (ret < 0) {
                  GST_DEBUG_OBJECT(encoder, "encode ge2d mem alloc failed, ret=0x%x", ret);
              if (frame)
                  gst_video_codec_frame_unref (frame);
                  return GST_FLOW_ERROR;
              }
              GST_DEBUG_OBJECT(encoder, "ge2d init successful!");
          }

          if (GST_VIDEO_INFO_FORMAT(info) == GST_VIDEO_FORMAT_BGR) {
              memcpy((void *)encoder->amlge2d.ge2dinfo.src_info[0].vaddr[0], (void *)pixel, info->size);
          }

          do_strechblit(&encoder->amlge2d.ge2dinfo, info);
          aml_ge2d_invalid_cache(&encoder->amlge2d.ge2dinfo);
          ui1_plane_num = 1;
          frame_info.mem_type = JPEGENC_DMA_BUFF;
          frame_info.YCbCr[0] = encoder->amlge2d.ge2dinfo.dst_info.shared_fd[0];
          frame_info.plane_num = ui1_plane_num;
          GST_DEBUG_OBJECT(encoder, "Set DMA buffer planes %d fd[0x%lx]",
          frame_info.plane_num, frame_info.YCbCr[0]);
          gst_video_frame_unmap (&video_frame);
      } else
#endif
      {
          frame_info.YCbCr[0] = (ulong) (pixel);
          frame_info.mem_type = JPEGENC_LOCAL_BUFF;
          gst_video_frame_unmap (&video_frame);
      }
  }

    /*
      if want to see the parameters,please release the log.
     */
  frame_info.width = info->width;
  frame_info.height = info->height;
  frame_info.w_stride = info->width;
  frame_info.h_stride = info->height;
  frame_info.quality = encoder->quality;
  frame_info.oformat = 0;

  GST_DEBUG_OBJECT (encoder,"iformat=%d",frame_info.iformat);
  GST_DEBUG_OBJECT (encoder,"mem_type=%d",frame_info.mem_type);
  GST_DEBUG_OBJECT (encoder,"pixel=0x%p",pixel);

  result = jpegenc_encode(encoder->handle, frame_info, encoder->outputbuf, &datalen);

  if (result == ENC_FAILED) {
    if (frame) {
      GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, ("Encode v frame failed."),
          ("gst_amlvencoder_encode return code=%d", result));
      gst_video_codec_frame_unref (frame);
      return GST_FLOW_ERROR;
    } else {
      return GST_FLOW_EOS;
    }
  }

  if (frame) {
    gst_video_codec_frame_unref (frame);
  }

  frame = gst_video_encoder_get_oldest_frame (GST_VIDEO_ENCODER (encoder));
  if (!frame) {
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  frame->output_buffer = gst_video_encoder_allocate_output_buffer(
      GST_VIDEO_ENCODER(encoder), datalen);
  gst_buffer_map(frame->output_buffer, &map, GST_MAP_WRITE);
  memcpy (map.data, encoder->outputbuf, datalen);
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
  GstAmlJpegEnc *self = GST_AMLJPEGENC (encoder);
  GstVideoInfo *info;
  guint size, min = 0, max = 0;
  GstCaps *caps;
  GstBufferPool *pool = NULL;
  gboolean need_pool = FALSE;

  if (!self->input_state)
    return FALSE;

  info = &self->input_state->info;

  if (info) {
      switch (GST_VIDEO_INFO_FORMAT(info)) {
      case GST_VIDEO_FORMAT_NV12:
      case GST_VIDEO_FORMAT_NV21:
          {
            GST_DEBUG_OBJECT(encoder, "choose gst_drm_bufferpool");
            gst_query_parse_allocation(query, &caps, &need_pool);
            GST_DEBUG_OBJECT(encoder, "need_pool: %d", need_pool);

              if (need_pool) {
                      pool = gst_drm_bufferpool_new(FALSE, GST_DRM_BUFFERPOOL_TYPE_VIDEO_PLANE);
                      GST_DEBUG_OBJECT(encoder, "new gst_drm_bufferpool");
                  }

              gst_query_add_allocation_pool(query, pool, info->size, DRMBP_EXTRA_BUF_SIZE_FOR_DISPLAY, DRMBP_LIMIT_MAX_BUFSIZE_TO_BUFSIZE);
              GST_DEBUG_OBJECT(encoder, "info->size: %d", info->size);
                      if (pool)
                      g_object_unref(pool);

              gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

          break;
          }
      default: //hanle not NV12/NV21
          {
            GST_DEBUG_OBJECT(encoder, "choose fake bufferpool");
            if (gst_query_get_n_allocation_pools (query) > 0) {
              gst_query_parse_nth_allocation_pool (query, 0, NULL, &size, &min, &max);
              size = MAX (size, info->size);
              gst_query_set_nth_allocation_pool (query, 0, NULL, size, self->min_buffers, self->max_buffers);
            } else {
              gst_query_add_allocation_pool (query, NULL, info->size, self->min_buffers, self->max_buffers);
              GST_DEBUG_OBJECT(encoder, "info->size: %d", info->size);
            }

              gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
          break;
          }
      }
  } else {
        GST_DEBUG_OBJECT(encoder, "can not get videoinfo");
        return FALSE;
  }

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

  GST_DEBUG_OBJECT(encoder, "malloc out_buf");

if (enc->outputbuf == NULL) {
    enc->outputbuf = g_new (guchar, enc->encoder_bufsize);
  }
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
