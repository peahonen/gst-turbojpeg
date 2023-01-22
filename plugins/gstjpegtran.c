/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2023 Petri Ahonen <peahonen@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-jpegtran
 *
 * Rearranges the compressed data (DCT coefficients), without ever fully decoding the image.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 videotestsrc is-live=true ! jpegenc ! jpegtran xop=rot180 ! jpegdec ! aasink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <turbojpeg.h>
#include "gstjpegtran.h"

GST_DEBUG_CATEGORY_STATIC (gst_jpegtran_debug);
#define GST_CAT_DEFAULT gst_jpegtran_debug

enum
{
  PROP_0,
  PROP_XOP
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/jpeg")
    );

#define gst_jpegtran_parent_class parent_class
G_DEFINE_TYPE (Gstjpegtran, gst_jpegtran, GST_TYPE_ELEMENT);

GST_ELEMENT_REGISTER_DEFINE (jpegtran, "jpegtran", GST_RANK_NONE,
    GST_TYPE_JPEGTRAN);

#define DEFAULT_XOP TJXOP_NONE
#define GST_TYPE_JPEGTRAN_XOP (gst_jpegtran_xop_get_type ())
static GType
gst_jpegtran_xop_get_type (void)
{
  static GType jpegtran_xop_type = 0;
  static const GEnumValue xop_types[] = {
    {TJXOP_NONE, "Do not transform the position of the image pixels", "none"},
    {TJXOP_HFLIP, "Flip (mirror) image horizontally.", "hflip"},
    {TJXOP_VFLIP,"Flip (mirror) image vertically.","vflip"},
    {TJXOP_TRANSPOSE,"Transpose image (flip/mirror along upper left to lower right axis.) ","transpose"},
    {TJXOP_TRANSVERSE,"Transverse transpose image (flip/mirror along upper right to lower left axis.","transverse"},
    {TJXOP_ROT90, "Rotate image clockwise by 90 degrees.", "rot90"},
    {TJXOP_ROT180, "Rotate image clockwise by 180 degrees.", "rot180"},
    {TJXOP_ROT270, "Rotate image clockwise by 270 degrees.", "rot270"},
    {0, NULL, NULL}
  };
  if (!jpegtran_xop_type) {
    jpegtran_xop_type =
      g_enum_register_static ("GstJpegTranXop", xop_types);
  }
  return jpegtran_xop_type;
}

static void gst_jpegtran_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_jpegtran_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_jpegtran_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_jpegtran_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);

/* GObject vmethod implementations */

/* initialize the jpegtran's class */
static void
gst_jpegtran_class_init (GstjpegtranClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_jpegtran_set_property;
  gobject_class->get_property = gst_jpegtran_get_property;
  gobject_class->finalize = gst_jpegtran_finalize;

  g_object_class_install_property (gobject_class, PROP_XOP,
      g_param_spec_enum ("xop", "transform",
          "Transform opertation to perform", GST_TYPE_JPEGTRAN_XOP,
          DEFAULT_XOP, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_details_simple (gstelement_class,
					"Losslessly transform a JPEG image into another JPEG image",
					"Filter Image",
					"Rearranges the compressed data (DCT coefficients) using libturbojpeg without ever fully decoding the image.",
					"Petri Ahonen <peahonen@gmail.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  GST_DEBUG_CATEGORY_INIT (gst_jpegtran_debug, "jpegtran", 0,
			   "jpegtran");

}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_jpegtran_init (Gstjpegtran * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_jpegtran_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
      GST_DEBUG_FUNCPTR (gst_jpegtran_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->xop = DEFAULT_XOP;

  filter->tjHandle = tjInitDecompress();
  if( filter->tjHandle == NULL) {
    GST_ELEMENT_ERROR (filter, STREAM, WRONG_TYPE, ("cannot init decompressor"),
        (NULL));
    // TODO
  }
  filter->tjInstance = tjInitTransform();
  if( filter->tjInstance == NULL) {
    GST_ELEMENT_ERROR (filter, STREAM, WRONG_TYPE, ("cannot init transform"),
        (NULL));
    // TODO
  }
}

static void
gst_jpegtran_finalize (Gstjpegtran * filter)
{
  tjDestroy(filter->tjHandle);
  tjDestroy(filter->tjInstance);
}

static void
gst_jpegtran_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstjpegtran *filter = GST_JPEGTRAN (object);

  switch (prop_id) {
    case PROP_XOP:
      filter->xop = g_value_get_enum(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_jpegtran_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstjpegtran *filter = GST_JPEGTRAN (object);

  switch (prop_id) {
    case PROP_XOP:
      g_value_set_enum(value, filter->xop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_jpegtran_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  Gstjpegtran *filter;
  gboolean ret;

  filter = GST_JPEGTRAN (parent);

  /*  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);*/

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_jpegtran_chain (GstPad * pad, GstObject * parent, GstBuffer * inbuf)
{
  GstFlowReturn ret;
  GstBuffer *outbuf = NULL;
  GstBuffer *trimmedbuf = NULL;
  Gstjpegtran *self;
  GstMapInfo in_info;
  GstMapInfo out_info;

  self = GST_JPEGTRAN (parent);

  if (!gst_buffer_map (inbuf, &in_info, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE, ("Unable to map memory"),
        (NULL));
    gst_buffer_unref (inbuf);
    return GST_FLOW_ERROR;
  }

  int width = 0,height = 0, jpegSubsamp = 0, jpegColorspace;

  if (tjDecompressHeader3(self->tjHandle,
			   in_info.data,
			   in_info.size,
			   &width,
			   &height,
			   &jpegSubsamp,
			   &jpegColorspace)) {
    GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE, ("cannot decompress header: %s", tjGetErrorStr2(self->tjHandle)),
        (NULL));
    // TODO
  }

  GST_LOG_OBJECT (pad,
		  "width %d, height %d, subsamp %d, size %d",
		  width,height,jpegSubsamp,
		  tjBufSize(width,
			    height,
			    jpegSubsamp)

		  );

  tjtransform xform;
  memset(&xform, 0, sizeof(tjtransform));
  xform.op = self->xop;
  xform.options |= TJXOPT_TRIM;
  int flags = TJFLAG_NOREALLOC;
  outbuf = gst_buffer_new ();
  GstMemory *mem;
  mem = gst_allocator_alloc (NULL,
			     tjBufSize(width,
				       height,
				       jpegSubsamp),
			     NULL);
  gst_buffer_append_memory (outbuf, mem);

  if (!gst_buffer_map (outbuf, &out_info, GST_MAP_WRITE)) {
    GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE, ("Unable to map memory"),
        (NULL));
    gst_buffer_unref (outbuf);
    // TODO
    return GST_FLOW_ERROR;
  }

  unsigned char *dstBufs[] = { out_info.data };
  unsigned long dstSizes[] = { out_info.size };

  if(tjTransform(self->tjInstance,
		 in_info.data,
		 in_info.size,
		 1,
		 dstBufs,
		 dstSizes,
		 &xform, flags) < 0) {
    GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE, ("tjTransform failed"),
		       (NULL));
    // TODO error
  }

  GST_LOG_OBJECT (pad,
		  "in %d dstSizes[0] %d delta %d",
		  in_info.size,
		  dstSizes[0],
		  dstSizes[0]-in_info.size
		  );

  gst_buffer_unmap (inbuf, &in_info);
  gst_buffer_unmap (outbuf, &out_info);

  trimmedbuf = gst_buffer_copy_region(outbuf,
				      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_META | GST_BUFFER_COPY_MEMORY,
				      0,
				      dstSizes[0]
				      );

  ret = gst_pad_push (self->srcpad, trimmedbuf);
  gst_buffer_unref (inbuf);
  gst_buffer_unref (outbuf);

  return ret;
}

