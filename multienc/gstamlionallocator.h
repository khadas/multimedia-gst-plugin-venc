/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 */

#ifndef __GST_AMLAmlIONALLOCATOR_H__
#define __GST_AMLAmlIONALLOCATOR_H__

#include <gst/gst.h>
#include <gst/gstallocator.h>

G_BEGIN_DECLS

typedef struct _GstAmlIONAllocator GstAmlIONAllocator;
typedef struct _GstAmlIONAllocatorClass GstAmlIONAllocatorClass;
typedef struct _GstAmlIONMemory GstAmlIONMemory;

#define GST_ALLOCATOR_AMLION "amlionmem"

#define GST_TYPE_AMLION_ALLOCATOR gst_amlion_allocator_get_type ()
#define GST_IS_AMLION_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
    GST_TYPE_AMLION_ALLOCATOR))
#define GST_AMLION_ALLOCATOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AMLION_ALLOCATOR, GstAmlIONAllocator))
#define GST_AMLION_ALLOCATOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AMLION_ALLOCATOR, GstAmlIONAllocatorClass))
#define GST_AMLION_ALLOCATOR_CAST(obj) ((GstAmlIONAllocator *)(obj))

#define GST_AMLION_MEMORY_QUARK gst_amlion_memory_quark ()

struct _GstAmlIONAllocator
{
  GstAllocator parent;

  gint fd;
  GstAllocator *dma_allocator;
};

struct _GstAmlIONAllocatorClass
{
  GstAllocatorClass parent;
};

struct _GstAmlIONMemory {
  GstMemory mem;

  gint fd;
  gsize size;
};

GType gst_amlion_allocator_get_type (void);
GstAllocator* gst_amlion_allocator_obtain (void);
gboolean gst_is_amlionbuf_memory (GstMemory * mem);

G_END_DECLS

#endif /* __GST_AMLAmlIONALLOCATOR_H__ */
