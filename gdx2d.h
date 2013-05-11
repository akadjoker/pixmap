/*
 * Copyright 2010 Mario Zechner (contact@badlogicgames.com), Nathan Sweet (admin@esotericsoftware.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS"
 * BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language
 * governing permissions and limitations under the License.
 */
#ifndef __GDX2D__
#define __GDX2D__




#define JNIEXPORT

#ifdef __cplusplus
extern "C" {
#endif

/**
 * pixmap formats, components are laid out in memory
 * in the order they appear in the constant name. E.g.
 * GDX_FORMAT_RGB => pixmap[0] = r, pixmap[1] = g, pixmap[2] = b.
 * Components are 8-bit each except for RGB565 and RGBA4444 which
 * take up two bytes each. The order of bytes is machine dependent
 * within a short the high order byte holds r and the first half of g
 * the low order byte holds the lower half of g and b as well as a
 * if the format is RGBA4444
 */
#define pixmap_FORMAT_ALPHA 				1
#define pixmap_FORMAT_LUMINANCE_ALPHA 	2
#define pixmap_FORMAT_RGB888 			3
#define pixmap_FORMAT_RGBA8888			4
#define pixmap_FORMAT_RGB565				5
#define pixmap_FORMAT_RGBA4444			6

/**
 * blending modes, to be extended
 */
#define pixmap_BLEND_NONE 		0
#define pixmap_BLEND_SRC_OVER 	1

/**
 * scaling modes, to be extended
 */
#define pixmap_SCALE_NEAREST		0
#define pixmap_SCALE_BILINEAR	1

/**
 * simple pixmap struct holding the pixel data,
 * the dimensions and the format of the pixmap.
 * the format is one of the pixmap_FORMAT_XXX constants.
 */
typedef struct {
	int width;
	int height;
	int format;
	const unsigned char* pixels;
} Pixmap;

JNIEXPORT Pixmap* pixmap_loadmemory (const unsigned char *buffer, int len, int req_format);
JNIEXPORT Pixmap* pixmap_load (const  char *buffer,  int req_format);
JNIEXPORT Pixmap* pixmap_load_power_of2(const  char *buffer,   int req_format);
JNIEXPORT Pixmap* pixmap_loadmemory_power_of2(const unsigned char *buffer, int len, int req_format) ;
JNIEXPORT Pixmap* pixmap_new  (int width, int height, int format);
JNIEXPORT void 	  pixmap_free (const Pixmap* pixmap);

JNIEXPORT int pixmap_save(Pixmap* map, const unsigned char *buffer,int format);
JNIEXPORT Pixmap* pixmap_rescale(Pixmap* map,  int new_width,int new_height );

JNIEXPORT void pixmap_set_blend	  (int blend);
JNIEXPORT void pixmap_set_scale	  (int scale);

JNIEXPORT const char*   pixmap_get_failure_reason(void);
JNIEXPORT void		pixmap_clear	   	  (const Pixmap* pixmap, int col);
JNIEXPORT void		pixmap_set_pixel   (const Pixmap* pixmap, int x, int y, int col);
JNIEXPORT int       pixmap_get_pixel	  (const Pixmap* pixmap, int x, int y);
JNIEXPORT void		pixmap_draw_line   (const Pixmap* pixmap, int x, int y, int x2, int y2, int col);
JNIEXPORT void		pixmap_draw_rect   (const Pixmap* pixmap, int x, int y, int width, int height, int col);
JNIEXPORT void		pixmap_draw_circle (const Pixmap* pixmap, int x, int y, int radius, int col);
JNIEXPORT void		pixmap_fill_rect   (const Pixmap* pixmap, int x, int y, int width, int height, int col);
JNIEXPORT void		pixmap_fill_circle (const Pixmap* pixmap, int x, int y, int radius, int col);
JNIEXPORT void		pixmap_draw_pixmap (const Pixmap* src_pixmap,
								   const Pixmap* dst_pixmap,
								   int src_x, int src_y, int src_width, int src_height,
								   int dst_x, int dst_y, int dst_width, int dst_height);

JNIEXPORT int pixmap_bytes_per_pixel(int format);

#ifdef __cplusplus
}
#endif

#endif
