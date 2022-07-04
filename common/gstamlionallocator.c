/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
//#include <ion.h>
#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/allocators/gstdmabuf.h>
#include "gstamlionallocator.h"

#include "ion.h"


#define MAX_HEAP_NAME 32
/**
 * struct ion_heap_data - data about a heap
 * @name - first 32 characters of the heap name
 * @type - heap type
 * @heap_id - heap id for the heap
 */
struct ion_heap_data {
    char name[MAX_HEAP_NAME];
    guint type;
    guint heap_id;
    guint reserved0;
    guint reserved1;
    guint reserved2;
};

#define ION_FLAG_CACHED 1
#define ION_FLAG_CACHED_NEEDS_SYNC 2

GST_DEBUG_CATEGORY_STATIC(amlion_allocator_debug);
#define GST_CAT_DEFAULT amlion_allocator_debug

#define gst_amlion_allocator_parent_class parent_class

G_DEFINE_TYPE(GstAmlIONAllocator, gst_amlion_allocator, GST_TYPE_ALLOCATOR)

static void
gst_amlion_mem_init(void)
{
  GstAllocator *allocator = g_object_new(gst_amlion_allocator_get_type(), NULL);
  GstAmlIONAllocator *self = GST_AMLION_ALLOCATOR (allocator);

  gint fd = ion_open ();
  if (fd < 0) {
    GST_ERROR ("Could not open ion driver");
    g_object_unref (self);
    return;
  }

  GST_DEBUG("Xiaobo add here for debugss %s,%d fd=[%d]",__FUNCTION__,__LINE__,fd);
  self->fd = fd;

  self->dma_allocator = gst_dmabuf_allocator_new();

  gst_allocator_register(g_type_name(GST_TYPE_AMLION_ALLOCATOR), allocator);
}

GstAllocator*
gst_amlion_allocator_obtain(void)
{
  static GOnce ion_allocator_once = G_ONCE_INIT;
  GstAllocator *allocator;

  g_once(&ion_allocator_once, (GThreadFunc)gst_amlion_mem_init, NULL);

  allocator = gst_allocator_find(g_type_name(GST_TYPE_AMLION_ALLOCATOR));

  if (allocator == NULL)
    GST_WARNING("No allocator named %s found", g_type_name(GST_TYPE_AMLION_ALLOCATOR));

  return allocator;
}

GQuark
gst_amlion_memory_quark (void)
{
  static GQuark quark = 0;

  if (quark == 0)
    quark = g_quark_from_string ("GstAmlIONPrivate");

  return quark;
}

#define ION_FLAG_EXTEND_MESON_HEAP_EXT          (1 << 30)


static GstMemory *
gst_amlion_alloc_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  GstAmlIONAllocator *self = GST_AMLION_ALLOCATOR (allocator);
  gint ionSize = (size + params->prefix + params->padding);
  gint ret = -1;
  gint data_fd = -1;
  gint num_heaps = 0;
  guint heap_mask = 0;
  gint legacy_ion = 0;
  struct ion_heap_data *heaps = NULL;
  guint heap_types[] = {ION_HEAP_TYPE_DMA, ION_HEAP_TYPE_CARVEOUT, ION_NUM_HEAPS};
  gint heap_type_idx = -1;
  // use GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS to indicate cached/uncached dma buffer
  gboolean is_uncached = (params->flags & GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS);

  if (self->fd < 0)
    return NULL;

  legacy_ion = ion_is_legacy(self->fd);

try_heap:
  heap_type_idx ++;
  if (heap_types[heap_type_idx] >= ION_NUM_HEAPS) {
    GST_ERROR ("ion alloc failed, no heap usable");
    goto fail_exit;
  }

  if (legacy_ion) {
    heap_mask = 1 << heap_types[heap_type_idx];
  } else {
    if ((ret = ion_query_heap_cnt (self->fd, &num_heaps)) < 0) {
      GST_ERROR ("ion query heap cnt failed, ret:%d", ret);
      goto fail_exit;
    }

    if (num_heaps <= 0) {
      GST_ERROR ("unexpected error: ion query heap cnt is 0");
      goto fail_exit;
    }

    heaps = g_new0 (struct ion_heap_data, num_heaps);

    if (heaps == NULL) {
      GST_ERROR ("failed to allocate ion heap data");
      goto fail_exit;
    }

    if ((ret = ion_query_get_heaps (self->fd, num_heaps, heaps)) < 0) {
      GST_ERROR ("ion query get heaps failed, ret:%d", ret);
      goto fail_exit;
    }

    for (int i = 0; i != num_heaps; ++i) {
      GST_DEBUG("Heap_name = [%s],heap type = [%d],heap id = [%d] heap_types[heap_type_idx][%d]",heaps[i].name,heaps[i].type,heaps[i].heap_id,heap_types[heap_type_idx]);
      if (heaps[i].type == heap_types[heap_type_idx]) {
        heap_mask = 1 << heaps[i].heap_id;
        break;
      }
    }

    g_free (heaps);
    heaps = NULL;
    if (heap_mask == 0) {
      GST_WARNING ("failed to match heapmask, heap type = %d", heap_types[heap_type_idx]);
      goto try_heap;
    }
  }
  guint flag = is_uncached ? 0 : ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC | ION_FLAG_EXTEND_MESON_HEAP_EXT;

  if (legacy_ion) {
    // force to uncached buffer if is legacy ion,
    // to avoid the memory conherency issue
    flag = 0;
  }

  ret = ion_alloc_fd (self->fd, ionSize, 0, heap_mask, flag, &data_fd);
  if (ret < 0) {
    GST_WARNING ("ion alloc failed on heap %d, ret: %d data_fd[%d]", heap_types[heap_type_idx], ret,data_fd);
    goto try_heap;
  }

  GST_DEBUG("phyalloc ionSize:%d dmafd: %d on heap %d, type: %s", ionSize,
            data_fd, heap_types[heap_type_idx],
            is_uncached ? "uncached" : "cached");

  GstAmlIONMemory *ion_mem = g_slice_new0 (GstAmlIONMemory);
  gst_memory_init (GST_MEMORY_CAST(ion_mem), GST_MEMORY_FLAG_NO_SHARE, allocator,
                   0, size, 0, 0, size);

  ion_mem->size = ionSize;
  ion_mem->fd = data_fd;

  GstMemory *mem =
      gst_dmabuf_allocator_alloc (self->dma_allocator, data_fd, ionSize);

  gst_mini_object_set_qdata (GST_MINI_OBJECT(mem), GST_AMLION_MEMORY_QUARK,
                             ion_mem, (GDestroyNotify) gst_memory_unref);

  GST_LOG ("allocated memory %p by allocator %p with qdata %p\n", mem,
           allocator, ion_mem);

  return mem;

fail_exit:
  if (heaps) {
    g_free (heaps);
  }
  if (data_fd > 0) {
    close (data_fd);
  }
  return NULL;
}

static void
gst_amlion_alloc_free (GstAllocator * allocator, GstMemory * memory)
{
  GstAmlIONAllocator *self = GST_AMLION_ALLOCATOR (allocator);
  GstAmlIONMemory *ion_mem = (GstAmlIONMemory *) memory;

  if (!ion_mem || self->fd < 0)
    return;

  GST_DEBUG ("phyfree ionSize:%ld dmafd: %d",
             ion_mem->size, ion_mem->fd);
  g_slice_free (GstAmlIONMemory, ion_mem);
}

static void
gst_amlion_allocator_dispose (GObject * object)
{
  GstAmlIONAllocator *self = GST_AMLION_ALLOCATOR (object);

  if (self->fd) {
    ion_close (self->fd);
    self->fd = -1;
  }

  if (self->dma_allocator)
    gst_object_unref(self->dma_allocator);
  self->dma_allocator = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_amlion_allocator_class_init (GstAmlIONAllocatorClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);

  allocator_class->alloc = GST_DEBUG_FUNCPTR (gst_amlion_alloc_alloc);
  allocator_class->free = GST_DEBUG_FUNCPTR (gst_amlion_alloc_free);
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_amlion_allocator_dispose);

  GST_DEBUG_CATEGORY_INIT(amlion_allocator_debug, "amlionallocator", 0, "DMA FD memory allocator based on ion");
}

static void
gst_amlion_allocator_init (GstAmlIONAllocator * self)
{
  GstAllocator *allocator = GST_ALLOCATOR (self);

  allocator->mem_type = GST_ALLOCATOR_AMLION;
  allocator->mem_map = NULL;
  allocator->mem_unmap = NULL;
}

gboolean
gst_is_amlionbuf_memory (GstMemory * mem)
{
  g_return_val_if_fail (mem != NULL, FALSE);

  gpointer qdata = gst_mini_object_get_qdata (GST_MINI_OBJECT(mem), GST_AMLION_MEMORY_QUARK);
  return qdata != NULL;
}

