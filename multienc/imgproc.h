/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 */
#ifndef _FRAMERESIZE_H
#define _FRAMERESIZE_H
#include <gst/gst.h>
#include <gst/video/video.h>

typedef enum {
  GST_AML_ROTATION_0,
  GST_AML_ROTATION_90,
  GST_AML_ROTATION_180,
  GST_AML_ROTATION_270,
} GstAmlRotation;

gint convert_video_format(GstVideoFormat format);
gint convert_video_rotation(GstAmlRotation rotation);

#define GST_TYPE_AML_ROTATION(module) (gst_aml_##module##_rotation_get_type())

#define GST_DECLARE_AML_ROTATION_GET_TYPE(module)                              \
  GType gst_aml_##module##_rotation_get_type(void)

#define GST_DEFINE_AML_ROTATION_GET_TYPE(module)                               \
  GType gst_aml_##module##_rotation_get_type(void) {                           \
    static GType aml_##module##_rotation_type = 0;                             \
    static const GEnumValue aml_##module##_rotation[] = {                      \
        {GST_AML_ROTATION_0, "0", "rotate 0 degrees"},                         \
        {GST_AML_ROTATION_90, "90", "rotate 90 degrees"},                      \
        {GST_AML_ROTATION_180, "180", "rotate 180 degrees"},                   \
        {GST_AML_ROTATION_270, "270", "rotate 270 degrees"},                   \
        {0, NULL, NULL},                                                       \
    };                                                                         \
    if (!aml_##module##_rotation_type) {                                       \
      aml_##module##_rotation_type = g_enum_register_static(                   \
          "GstAML" #module "Rotation", aml_##module##_rotation);               \
    }                                                                          \
    return aml_##module##_rotation_type;                                       \
  }

struct imgproc_buf {
  gint fd;
  gboolean is_ionbuf;
};

struct imgproc_pos {
  gint x;
  gint y;
  gint w;
  gint h;
  gint canvas_w;
  gint canvas_h;
};

void* imgproc_init ();
void imgproc_deinit (void *handle);
gboolean imgproc_crop (void* handle,
    struct imgproc_buf in_buf,
    struct imgproc_pos in_pos, GstVideoFormat in_format,
    struct imgproc_buf out_buf,
    struct imgproc_pos out_pos, GstVideoFormat out_format);
gboolean imgproc_transform(void *handle, struct imgproc_buf in_buf,
                           struct imgproc_pos in_pos, GstVideoFormat in_format,
                           struct imgproc_buf out_buf,
                           struct imgproc_pos out_pos,
                           GstVideoFormat out_format, GstAmlRotation rotation);
gboolean imgproc_fillrect(void *handle, GstVideoFormat format,
                          struct imgproc_buf buf, struct imgproc_pos pos,
                          guint color);

#endif /* _FRAMERESIZE_H */
