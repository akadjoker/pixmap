
#include "gdx2d.h"
#include <stdlib.h>
#define STBI_HEADER_FILE_ONLY
#define STBI_NO_FAILURE_STRINGS
#include "SOIL.h"
#include "image_helper.h"

static int pixmap_blend = pixmap_BLEND_NONE;
static int pixmap_scale = pixmap_SCALE_NEAREST;

static int* lu4 = 0;
static int* lu5 = 0;
static int* lu6 = 0;

typedef void(*set_pixel_func)(unsigned char* pixel_addr, int color);
typedef int(*get_pixel_func)(unsigned char* pixel_addr);

static inline void generate_look_ups() {
	int i = 0;
	lu4 = malloc(sizeof(int) * 16);
	lu5 = malloc(sizeof(int) * 32);
	lu6 = malloc(sizeof(int) * 64);

	for(i = 0; i < 16; i++) {
		lu4[i] = (int) i / 15.0f * 255;
		lu5[i] = (int) i / 31.0f * 255;
		lu6[i] = (int) i / 63.0f * 255;
	}

	for(i = 16; i < 32; i++) {
		lu5[i] = (int) i / 31.0f * 255;
		lu6[i] = (int) i / 63.0f * 255;
	}

	for(i = 32; i < 64; i++) {
		lu6[i] = (int) i / 63.0f * 255;
	}
}

static inline int to_format(int format, int color) {
	int r, g, b, a, l;

	switch(format) {
		case pixmap_FORMAT_ALPHA:
			return color & 0xff;
		case pixmap_FORMAT_LUMINANCE_ALPHA:
			r = (color & 0xff000000) >> 24;
			g = (color & 0xff0000) >> 16;
			b = (color & 0xff00) >> 8;
			a = (color & 0xff);
			l = ((int)(0.2126f * r + 0.7152 * g + 0.0722 * b) & 0xff) << 8;
			return (l & 0xffffff00) | a;
		case pixmap_FORMAT_RGB888:
			return color >> 8;
		case pixmap_FORMAT_RGBA8888:
			return color;
		case pixmap_FORMAT_RGB565:
			r = (((color & 0xff000000) >> 27) << 11) & 0xf800;
			g = (((color & 0xff0000) >> 18) << 5) & 0x7e0;
			b = ((color & 0xff00) >> 11) & 0x1f;
			return r | g | b;
		case pixmap_FORMAT_RGBA4444:
			r = (((color & 0xff000000) >> 28) << 12) & 0xf000;
			g = (((color & 0xff0000) >> 20) << 8) & 0xf00;
			b = (((color & 0xff00) >> 12) << 4) & 0xf0;
			a = ((color & 0xff) >> 4) & 0xf;
			return r | g | b | a;
		default:
			return 0;
	}
}

#define min(a, b) (a > b?b:a)

static inline int weight_RGBA8888(int color, float weight) {
	int r, g, b, a;
	r = min((int)(((color & 0xff000000) >> 24) * weight), 255);
	g = min((int)(((color & 0xff0000) >> 16) * weight), 255);
	b = min((int)(((color & 0xff00) >> 8) * weight), 255);
	a = min((int)(((color & 0xff)) * weight), 255);

	return (r << 24) | (g << 16) | (b << 8) | a;
}

static inline int to_RGBA8888(int format, int color) {
	int r, g, b, a;

	if(!lu5) generate_look_ups();

	switch(format) {
		case pixmap_FORMAT_ALPHA:
			return (color & 0xff) | 0xffffff00;
		case pixmap_FORMAT_LUMINANCE_ALPHA:
			return ((color & 0xff00) << 16) | ((color & 0xff00) << 8) | (color & 0xffff);
		case pixmap_FORMAT_RGB888:
			return (color << 8) | 0x000000ff;
		case pixmap_FORMAT_RGBA8888:
			return color;
		case pixmap_FORMAT_RGB565:
			r = lu5[(color & 0xf800) >> 11] << 24;
			g = lu6[(color & 0x7e0) >> 5] << 16;
			b = lu5[(color & 0x1f)] << 8;
			return r | g | b | 0xff;
		case pixmap_FORMAT_RGBA4444:
			r = lu4[(color & 0xf000) >> 12] << 24;
			g = lu4[(color & 0xf00) >> 8] << 16;
			b = lu4[(color & 0xf0) >> 4] << 8;
			a = lu4[(color & 0xf)];
			return r | g | b | a;
		default:
			return 0;
	}
}

static inline void set_pixel_alpha(unsigned char *pixel_addr, int color) {
	*pixel_addr = (unsigned char)(color & 0xff);
}

static inline void set_pixel_luminance_alpha(unsigned char *pixel_addr, int color) {
	*(unsigned short*)pixel_addr = (unsigned short)color;
}

static inline void set_pixel_RGB888(unsigned char *pixel_addr, int color) {
	//*(unsigned short*)pixel_addr = (unsigned short)(((color & 0xff0000) >> 16) | (color & 0xff00));
	pixel_addr[0] = (color & 0xff0000) >> 16;
	pixel_addr[1] = (color & 0xff00) >> 8;
	pixel_addr[2] = (color & 0xff);
}

static inline void set_pixel_RGBA8888(unsigned char *pixel_addr, int color) {
	*(int*)pixel_addr = ((color & 0xff000000) >> 24) |
							((color & 0xff0000) >> 8) |
							((color & 0xff00) << 8) |
							((color & 0xff) << 24);
}

static inline void set_pixel_RGB565(unsigned char *pixel_addr, int color) {
	*(int*)pixel_addr = (int)(color);
}

static inline void set_pixel_RGBA4444(unsigned char *pixel_addr, int color) {
	*(int*)pixel_addr = (int)(color);
}

static inline set_pixel_func set_pixel_func_ptr(int format) {
	switch(format) {
		case pixmap_FORMAT_ALPHA:			return &set_pixel_alpha;
		case pixmap_FORMAT_LUMINANCE_ALPHA:	return &set_pixel_luminance_alpha;
		case pixmap_FORMAT_RGB888:			return &set_pixel_RGB888;
		case pixmap_FORMAT_RGBA8888:			return &set_pixel_RGBA8888;
		case pixmap_FORMAT_RGB565:			return &set_pixel_RGB565;
		case pixmap_FORMAT_RGBA4444:			return &set_pixel_RGBA4444;
		default: return &set_pixel_alpha; // better idea for a default?
	}
}

static inline int blend(int src, int dst) {
	int src_r = (src & 0xff000000) >> 24;
	int src_g = (src & 0xff0000) >> 16;
	int src_b = (src & 0xff00) >> 8;
	int src_a = (src & 0xff);

	int dst_r = (dst & 0xff000000) >> 24;
	int dst_g = (dst & 0xff0000) >> 16;
	int dst_b = (dst & 0xff00) >> 8;
	int dst_a = (dst & 0xff);

	dst_r = dst_r + src_a * (src_r - dst_r) / 255;
	dst_g = dst_g + src_a * (src_g - dst_g) / 255;
	dst_b = dst_b + src_a * (src_b - dst_b) / 255;
	dst_a = (int)((1.0f - (1.0f - src_a / 255.0f) * (1.0f - dst_a / 255.0f)) * 255);
	return (int)((dst_r << 24) | (dst_g << 16) | (dst_b << 8) | dst_a);
}

static inline int get_pixel_alpha(unsigned char *pixel_addr) {
	return *pixel_addr;
}

static inline int get_pixel_luminance_alpha(unsigned char *pixel_addr) {
	return (((int)pixel_addr[0]) << 8) | pixel_addr[1];
}

static inline int get_pixel_RGB888(unsigned char *pixel_addr) {
	return (((int)pixel_addr[0]) << 16) | (((int)pixel_addr[1]) << 8) | (pixel_addr[2]);
}

static inline int get_pixel_RGBA8888(unsigned char *pixel_addr) {
	return (((int)pixel_addr[0]) << 24) | (((int)pixel_addr[1]) << 16) | (((int)pixel_addr[2]) << 8) | pixel_addr[3];
}

static inline int get_pixel_RGB565(unsigned char *pixel_addr) {
	return *(int*)pixel_addr;
}

static inline int get_pixel_RGBA4444(unsigned char *pixel_addr) {
	return *(int*)pixel_addr;
}

static inline get_pixel_func get_pixel_func_ptr(int format) {
	switch(format) {
		case pixmap_FORMAT_ALPHA:			return &get_pixel_alpha;
		case pixmap_FORMAT_LUMINANCE_ALPHA:	return &get_pixel_luminance_alpha;
		case pixmap_FORMAT_RGB888:			return &get_pixel_RGB888;
		case pixmap_FORMAT_RGBA8888:			return &get_pixel_RGBA8888;
		case pixmap_FORMAT_RGB565:			return &get_pixel_RGB565;
		case pixmap_FORMAT_RGBA4444:			return &get_pixel_RGBA4444;
		default: return &get_pixel_alpha; // better idea for a default?
	}
}





Pixmap* pixmap_loadmemory(const unsigned char *buffer, int len, int req_format) {
	int width, height, format;
	const unsigned char* pixels = SOIL_load_image_from_memory(buffer, len, &width, &height, &format, req_format);
	if(pixels == NULL)
		return NULL;

	Pixmap* pixmap = (Pixmap*)malloc(sizeof(Pixmap));
	pixmap->width = (int)width;
	pixmap->height = (int)height;
	pixmap->format = (int)format;
	pixmap->pixels = pixels;
	return pixmap;
}

Pixmap* pixmap_load(const  char *buffer,   int req_format) {
	int width, height, format;
	const unsigned char* pixels =SOIL_load_image(buffer,  &width, &height, &format, req_format);
	if(pixels == NULL)

		return NULL;

	Pixmap* pixmap = (Pixmap*)malloc(sizeof(Pixmap));
	pixmap->width = width;
	pixmap->height =height;
	pixmap->format =format;
	pixmap->pixels = pixels;

	//printf(" f%s w:%i  h:%i f:%i  \n",buffer,width,height,format);
	return pixmap;
}

void free_image_data
	(
		unsigned char *img_data
	)
{
	free( (void*)img_data );
}

int pixmap_save(Pixmap* map, const unsigned char *buffer,int format)
{
    return SOIL_save_image(buffer,format,map->width,map->height,map->format,map->pixels);



}
float sqrtf(float v)
{
	return 0.0f;
}

Pixmap* pixmap_rescale(Pixmap* map,  int new_width,int new_height )
{
    int width, height, format;

	       const	unsigned char *resampled = (unsigned char*)malloc( map->format*new_width*new_height );
			up_scale_image(map->pixels, map->width, map->height, map->format,resampled, new_width, new_height );


	Pixmap* pixmap = (Pixmap*)malloc(sizeof(Pixmap));
	pixmap->width = new_width;
	pixmap->height = new_height;
	pixmap->format = map->format;
	pixmap->pixels = resampled;
	pixmap_free(map );
	return pixmap;

}

Pixmap* pixmap_load_power_of2(const  char *buffer,   int req_format) {

	int width, height, format;
	if(req_format > pixmap_FORMAT_RGBA8888)  req_format = pixmap_FORMAT_RGBA8888;


	const unsigned char* pixels =SOIL_load_image(buffer,  &width, &height, &format, req_format);
	if(pixels == NULL) return NULL;



	    int new_width = 1;
		int new_height = 1;
		while( new_width < width )
		{
			new_width *= 2;
		}
		while( new_height < height )
		{
			new_height *= 2;
		}

		if( (new_width != width) || (new_height != height) )
		{

		const	unsigned char *resampled = (unsigned char*)malloc( format*new_width*new_height );
			up_scale_image(pixels, width, height, format,resampled, new_width, new_height );
			free_image_data( pixels );
			pixels = resampled;
			width = new_width;
			height = new_height;
			//printf("resample  w:%i h:%i c:%i \n",width,height,channels);
		}



	Pixmap* pixmap = (Pixmap*)malloc(sizeof(Pixmap));
	pixmap->width = width;
	pixmap->height = height;
	pixmap->format = format;
	pixmap->pixels = pixels;


	return pixmap;
}

Pixmap* pixmap_loadmemory_power_of2(const unsigned char *buffer, int len, int req_format) {
	int width, height, format;
	// TODO fix this! Add conversion to requested format
//	if(req_format > pixmap_FORMAT_RGBA8888)
	//	req_format = pixmap_FORMAT_RGBA8888;
	const unsigned char* pixels = SOIL_load_image_from_memory(buffer, len, &width, &height, &format, req_format);
	if(pixels == NULL)
		return NULL;



	    int new_width = 1;
		int new_height = 1;
		while( new_width < width )
		{
			new_width *= 2;
		}
		while( new_height < height )
		{
			new_height *= 2;
		}

		if( (new_width != width) || (new_height != height) )
		{

		const	unsigned char *resampled = (unsigned char*)malloc( format*new_width*new_height );
			up_scale_image(pixels, width, height, format,resampled, new_width, new_height );
			free_image_data( pixels );
			pixels = resampled;
			width = new_width;
			height = new_height;
			//printf("resample  w:%i h:%i c:%i \n",width,height,channels);
		}

	Pixmap* pixmap = (Pixmap*)malloc(sizeof(Pixmap));
	pixmap->width = width;
	pixmap->height =height;
	pixmap->format =format;
	pixmap->pixels = pixels;
	return pixmap;
}


int pixmap_bytes_per_pixel(int format) {
	switch(format) {
		case pixmap_FORMAT_ALPHA:
			return 1;
		case pixmap_FORMAT_LUMINANCE_ALPHA:
		case pixmap_FORMAT_RGB565:
		case pixmap_FORMAT_RGBA4444:
			return 2;
		case pixmap_FORMAT_RGB888:
			return 3;
		case pixmap_FORMAT_RGBA8888:
			return 4;
		default:
			return 4;
	}
}

Pixmap* pixmap_new(int width, int height, int format) {
	Pixmap* pixmap = (Pixmap*)malloc(sizeof(Pixmap));
	pixmap->width = width;
	pixmap->height = height;
	pixmap->format = format;
	pixmap->pixels = (unsigned char*)malloc(width * height * pixmap_bytes_per_pixel(format));
	return pixmap;
}
void pixmap_free(const Pixmap* pixmap) {
	free((void*)pixmap->pixels);
	free((void*)pixmap);
}

void pixmap_set_blend (int blend) {
	pixmap_blend = blend;
}

void pixmap_set_scale (int scale) {
	pixmap_scale = scale;
}

const char *pixmap_get_failure_reason(void) {
  return stbi_failure_reason();
}

static inline void clear_alpha(const Pixmap* pixmap, int col) {
	int pixels = pixmap->width * pixmap->height;
	memset((void*)pixmap->pixels, col, pixels);
}

static inline void clear_luminance_alpha(const Pixmap* pixmap, int col) {
	int pixels = pixmap->width * pixmap->height;
	unsigned short* ptr = (unsigned short*)pixmap->pixels;
	unsigned short l = (col & 0xff) << 8 | (col >> 8);

	for(; pixels > 0; pixels--) {
		*ptr = l;
		ptr++;
	}
}

static inline void clear_RGB888(const Pixmap* pixmap, int col) {
	int pixels = pixmap->width * pixmap->height;
	unsigned char* ptr = (unsigned char*)pixmap->pixels;
	unsigned char r = (col & 0xff0000) >> 16;
	unsigned char g = (col & 0xff00) >> 8;
	unsigned char b = (col & 0xff);

	for(; pixels > 0; pixels--) {
		*ptr = r;
		ptr++;
		*ptr = g;
		ptr++;
		*ptr = b;
		ptr++;
	}
}

static inline void clear_RGBA8888(const Pixmap* pixmap, int col) {
	int pixels = pixmap->width * pixmap->height;
	int* ptr = (int*)pixmap->pixels;
	unsigned char r = (col & 0xff000000) >> 24;
	unsigned char g = (col & 0xff0000) >> 16;
	unsigned char b = (col & 0xff00) >> 8;
	unsigned char a = (col & 0xff);
	col = (a << 24) | (b << 16) | (g << 8) | r;

	for(; pixels > 0; pixels--) {
		*ptr = col;
		ptr++;
	}
}

static inline void clear_RGB565(const Pixmap* pixmap, int col) {
	int pixels = pixmap->width * pixmap->height;
	unsigned short* ptr = (unsigned short*)pixmap->pixels;
	unsigned short l = col & 0xffff;

	for(; pixels > 0; pixels--) {
		*ptr = l;
		ptr++;
	}
}

static inline void clear_RGBA4444(const Pixmap* pixmap, int col) {
	int pixels = pixmap->width * pixmap->height;
	unsigned short* ptr = (unsigned short*)pixmap->pixels;
	unsigned short l = col & 0xffff;

	for(; pixels > 0; pixels--) {
		*ptr = l;
		ptr++;
	}
}

void pixmap_clear(const Pixmap* pixmap, int col) {
	col = to_format(pixmap->format, col);

	switch(pixmap->format) {
		case pixmap_FORMAT_ALPHA:
			clear_alpha(pixmap, col);
			break;
		case pixmap_FORMAT_LUMINANCE_ALPHA:
			clear_luminance_alpha(pixmap, col);
			break;
		case pixmap_FORMAT_RGB888:
			clear_RGB888(pixmap, col);
			break;
		case pixmap_FORMAT_RGBA8888:
			clear_RGBA8888(pixmap, col);
			break;
		case pixmap_FORMAT_RGB565:
			clear_RGB565(pixmap, col);
			break;
		case pixmap_FORMAT_RGBA4444:
			clear_RGBA4444(pixmap, col);
			break;
		default:
			break;
	}
}

static inline int in_pixmap(const Pixmap* pixmap, int x, int y) {
	if(x < 0 || y < 0)
		return 0;
	if(x >= pixmap->width || y >= pixmap->height)
		return 0;
	return -1;
}

static inline void set_pixel(unsigned char* pixels, int width, int height, int bpp, set_pixel_func pixel_func, int x, int y, int col) {
	if(x < 0 || y < 0) return;
	if(x >= (int)width || y >= (int)height) return;
	pixels = pixels + (x + width * y) * bpp;
	pixel_func(pixels, col);
}

int pixmap_get_pixel(const Pixmap* pixmap, int x, int y) {
	if(!in_pixmap(pixmap, x, y))
		return 0;
	unsigned char* ptr = (unsigned char*)pixmap->pixels + (x + pixmap->width * y) * pixmap_bytes_per_pixel(pixmap->format);
	return to_RGBA8888(pixmap->format, get_pixel_func_ptr(pixmap->format)(ptr));
}

void pixmap_set_pixel(const Pixmap* pixmap, int x, int y, int col) {
	if(pixmap_blend) {
		int dst = pixmap_get_pixel(pixmap, x, y);
		col = blend(col, dst);
		col = to_format(pixmap->format, col);
		set_pixel((unsigned char*)pixmap->pixels, pixmap->width, pixmap->height, pixmap_bytes_per_pixel(pixmap->format), set_pixel_func_ptr(pixmap->format), x, y, col);
	} else {
		col = to_format(pixmap->format, col);
		set_pixel((unsigned char*)pixmap->pixels, pixmap->width, pixmap->height, pixmap_bytes_per_pixel(pixmap->format), set_pixel_func_ptr(pixmap->format), x, y, col);
	}
}

void pixmap_draw_line(const Pixmap* pixmap, int x0, int y0, int x1, int y1, int col) {
    int dy = y1 - y0;
    int dx = x1 - x0;
	int fraction = 0;
    int stepx, stepy;
	unsigned char* ptr = (unsigned char*)pixmap->pixels;
	int bpp = pixmap_bytes_per_pixel(pixmap->format);
	set_pixel_func pset = set_pixel_func_ptr(pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(pixmap->format);
	int col_format = to_format(pixmap->format, col);
	void* addr = ptr + (x0 + y0 * pixmap->width) * bpp;

    if (dy < 0) { dy = -dy;  stepy = -1; } else { stepy = 1; }
    if (dx < 0) { dx = -dx;  stepx = -1; } else { stepx = 1; }
    dy <<= 1;
    dx <<= 1;

    if(in_pixmap(pixmap, x0, y0)) {
    	if(pixmap_blend) {
    		col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(addr))));
    	}
    	pset(addr, col_format);
    }
    if (dx > dy) {
        fraction = dy - (dx >> 1);
        while (x0 != x1) {
            if (fraction >= 0) {
                y0 += stepy;
                fraction -= dx;
            }
            x0 += stepx;
            fraction += dy;
			if(in_pixmap(pixmap, x0, y0)) {
				addr = ptr + (x0 + y0 * pixmap->width) * bpp;
				if(pixmap_blend) {
					col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(addr))));
				}
				pset(addr, col_format);
			}
        }
    } else {
		fraction = dx - (dy >> 1);
		while (y0 != y1) {
			if (fraction >= 0) {
				x0 += stepx;
				fraction -= dy;
			}
			y0 += stepy;
			fraction += dx;
			if(in_pixmap(pixmap, x0, y0)) {
				addr = ptr + (x0 + y0 * pixmap->width) * bpp;
				if(pixmap_blend) {
					col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(addr))));
				}
				pset(addr, col_format);
			}
		}
	}
}

static inline void hline(const Pixmap* pixmap, int x1, int x2, int y, int col) {
	int tmp = 0;
	set_pixel_func pset = set_pixel_func_ptr(pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(pixmap->format);
	unsigned char* ptr = (unsigned char*)pixmap->pixels;
	int bpp = pixmap_bytes_per_pixel(pixmap->format);
	int col_format = to_format(pixmap->format, col);

	if(y < 0 || y >= (int)pixmap->height) return;

	if(x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}

	if(x1 >= (int)pixmap->width) return;
	if(x2 < 0)  return;

	if(x1 < 0) x1 = 0;
	if(x2 >= (int)pixmap->width) x2 = pixmap->width - 1;
	x2 += 1;

	ptr += (x1 + y * pixmap->width) * bpp;

	while(x1 != x2) {
		if(pixmap_blend) {
			col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(ptr))));
		}
		pset(ptr, col_format);
		x1++;
		ptr += bpp;
	}
}

static inline void vline(const Pixmap* pixmap, int y1, int y2, int x, int col) {
	int tmp = 0;
	set_pixel_func pset = set_pixel_func_ptr(pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(pixmap->format);
	unsigned char* ptr = (unsigned char*)pixmap->pixels;
	int bpp = pixmap_bytes_per_pixel(pixmap->format);
	int stride = bpp * pixmap->width;
	int col_format = to_format(pixmap->format, col);

	if(x < 0 || x >= pixmap->width) return;

	if(y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	if(y1 >= (int)pixmap->height) return;
	if(y2 < 0) return;

	if(y1 < 0) y1 = 0;
	if(y2 >= (int)pixmap->height) y2 = pixmap->height - 1;
	y2 += 1;

	ptr += (x + y1 * pixmap->width) * bpp;

	while(y1 != y2) {
		if(pixmap_blend) {
			col_format = to_format(pixmap->format, blend(col, to_RGBA8888(pixmap->format, pget(ptr))));
		}
		pset(ptr, col_format);
		y1++;
		ptr += stride;
	}
}

void pixmap_draw_rect(const Pixmap* pixmap, int x, int y, int width, int height, int col) {
	hline(pixmap, x, x + width - 1, y, col);
	hline(pixmap, x, x + width - 1, y + height - 1, col);
	vline(pixmap, y, y + height - 1, x, col);
	vline(pixmap, y, y + height - 1, x + width - 1, col);
}

static inline void circle_points(unsigned char* pixels, int width, int height, int bpp, set_pixel_func pixel_func, int cx, int cy, int x, int y, int col) {
    if (x == 0) {
        set_pixel(pixels, width, height, bpp, pixel_func, cx, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx, cy - y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + y, cy, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - y, cy, col);
    } else
    if (x == y) {
        set_pixel(pixels, width, height, bpp, pixel_func, cx + x, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - x, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + x, cy - y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - x, cy - y, col);
    } else
    if (x < y) {
        set_pixel(pixels, width, height, bpp, pixel_func, cx + x, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - x, cy + y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + x, cy - y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - x, cy - y, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + y, cy + x, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - y, cy + x, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx + y, cy - x, col);
        set_pixel(pixels, width, height, bpp, pixel_func, cx - y, cy - x, col);
    }
}

void pixmap_draw_circle(const Pixmap* pixmap, int x, int y, int radius, int col) {
    int px = 0;
    int py = radius;
    int p = (5 - (int)radius*4)/4;
	unsigned char* pixels = (unsigned char*)pixmap->pixels;
	int width = pixmap->width;
	int height = pixmap->height;
	int bpp = pixmap_bytes_per_pixel(pixmap->format);
	set_pixel_func pixel_func = set_pixel_func_ptr(pixmap->format);
	col = to_format(pixmap->format, col);

    circle_points(pixels, width, height, bpp, pixel_func, x, y, px, py, col);
    while (px < py) {
        px++;
        if (p < 0) {
            p += 2*px+1;
        } else {
            py--;
            p += 2*(px-py)+1;
        }
        circle_points(pixels, width, height, bpp, pixel_func, x, y, px, py, col);
    }
}

void pixmap_fill_rect(const Pixmap* pixmap, int x, int y, int width, int height, int col) {
	int x2 = x + width - 1;
	int y2 = y + height - 1;

	if(x >= (int)pixmap->width) return;
	if(y >= (int)pixmap->height) return;
	if(x2 < 0) return;
	if(y2 < 0) return;

	if(x < 0) x = 0;
	if(y < 0) y = 0;
	if(x2 >= (int)pixmap->width) x2 = pixmap->width - 1;
	if(y2 >= (int)pixmap->height) y2 = pixmap->height - 1;

	y2++;
	while(y!=y2) {
		hline(pixmap, x, x2, y, col);
		y++;
	}
}

void pixmap_fill_circle(const Pixmap* pixmap, int x0, int y0, int radius, int col) {
	int f = 1 - (int)radius;
	int ddF_x = 1;
	int ddF_y = -2 * (int)radius;
	int px = 0;
	int py = (int)radius;

	hline(pixmap, x0, x0, y0 + (int)radius, col);
	hline(pixmap, x0, x0, y0 - (int)radius, col);
	hline(pixmap, x0 - (int)radius, x0 + (int)radius, y0, col);


	while(px < py)
	{
		if(f >= 0)
		{
			py--;
			ddF_y += 2;
			f += ddF_y;
		}
		px++;
		ddF_x += 2;
		f += ddF_x;
		hline(pixmap, x0 - px, x0 + px, y0 + py, col);
		hline(pixmap, x0 - px, x0 + px, y0 - py, col);
		hline(pixmap, x0 - py, x0 + py, y0 + px, col);
		hline(pixmap, x0 - py, x0 + py, y0 - px, col);
	}
}

static inline void blit_same_size(const Pixmap* src_pixmap, const Pixmap* dst_pixmap,
						 			 int src_x, int src_y,
									 int dst_x, int dst_y,
									 int width, int height) {
	set_pixel_func pset = set_pixel_func_ptr(dst_pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(src_pixmap->format);
	get_pixel_func dpget = get_pixel_func_ptr(dst_pixmap->format);
	int sbpp = pixmap_bytes_per_pixel(src_pixmap->format);
	int dbpp = pixmap_bytes_per_pixel(dst_pixmap->format);
	int spitch = sbpp * src_pixmap->width;
	int dpitch = dbpp * dst_pixmap->width;

	int sx = src_x;
	int sy = src_y;
	int dx = dst_x;
	int dy = dst_y;

	for(;sy < src_y + height; sy++, dy++) {
		if(sy < 0 || dy < 0) continue;
		if(sy >= src_pixmap->height || dy >= dst_pixmap->height) break;

		for(sx = src_x, dx = dst_x; sx < src_x + width; sx++, dx++) {
			if(sx < 0 || dx < 0) continue;
			if(sx >= src_pixmap->width || dx >= dst_pixmap->width) break;

			const void* src_ptr = src_pixmap->pixels + sx * sbpp + sy * spitch;
			const void* dst_ptr = dst_pixmap->pixels + dx * dbpp + dy * dpitch;
			int src_col = to_RGBA8888(src_pixmap->format, pget((void*)src_ptr));

			if(pixmap_blend) {
				int dst_col = to_RGBA8888(dst_pixmap->format, dpget((void*)dst_ptr));
				src_col = to_format(dst_pixmap->format, blend(src_col, dst_col));
			} else {
				src_col = to_format(dst_pixmap->format, src_col);
			}

			pset((void*)dst_ptr, src_col);
		}
	}
}

static inline void blit_bilinear(const Pixmap* src_pixmap, const Pixmap* dst_pixmap,
		   int src_x, int src_y, int src_width, int src_height,
		   int dst_x, int dst_y, int dst_width, int dst_height) {
	set_pixel_func pset = set_pixel_func_ptr(dst_pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(src_pixmap->format);
	get_pixel_func dpget = get_pixel_func_ptr(dst_pixmap->format);
	int sbpp = pixmap_bytes_per_pixel(src_pixmap->format);
	int dbpp = pixmap_bytes_per_pixel(dst_pixmap->format);
	int spitch = sbpp * src_pixmap->width;
	int dpitch = dbpp * dst_pixmap->width;

	float x_ratio = ((float)src_width - 1)/ dst_width;
	float y_ratio = ((float)src_height - 1) / dst_height;
	float x_diff = 0;
	float y_diff = 0;

	int dx = dst_x;
	int dy = dst_y;
	int sx = src_x;
	int sy = src_y;
	int i = 0;
	int j = 0;

	for(;i < dst_height; i++) {
		sy = (int)(i * y_ratio) + src_y;
		dy = i + dst_y;
		y_diff = (y_ratio * i + src_y) - sy;
		if(sy < 0 || dy < 0) continue;
		if(sy >= src_pixmap->height || dy >= dst_pixmap->height) break;

		for(j = 0 ;j < dst_width; j++) {
			sx = (int)(j * x_ratio) + src_x;
			dx = j + dst_x;
			x_diff = (x_ratio * j + src_x) - sx;
			if(sx < 0 || dx < 0) continue;
			if(sx >= src_pixmap->width || dx >= dst_pixmap->width) break;

			const void* dst_ptr = dst_pixmap->pixels + dx * dbpp + dy * dpitch;
			const void* src_ptr = src_pixmap->pixels + sx * sbpp + sy * spitch;
			int c1 = 0, c2 = 0, c3 = 0, c4 = 0;
			c1 = to_RGBA8888(src_pixmap->format, pget((void*)src_ptr));
			if(sx + 1 < src_width) c2 = to_RGBA8888(src_pixmap->format, pget((void*)(src_ptr + sbpp))); else c2 = c1;
			if(sy + 1< src_height) c3 = to_RGBA8888(src_pixmap->format, pget((void*)(src_ptr + spitch))); else c3 = c1;
			if(sx + 1< src_width && sy + 1 < src_height) c4 = to_RGBA8888(src_pixmap->format, pget((void*)(src_ptr + spitch + sbpp))); else c4 = c1;

			float ta = (1 - x_diff) * (1 - y_diff);
			float tb = (x_diff) * (1 - y_diff);
			float tc = (1 - x_diff) * (y_diff);
			float td = (x_diff) * (y_diff);

			int r = (int)(((c1 & 0xff000000) >> 24) * ta +
									((c2 & 0xff000000) >> 24) * tb +
									((c3 & 0xff000000) >> 24) * tc +
									((c4 & 0xff000000) >> 24) * td) & 0xff;
			int g = (int)(((c1 & 0xff0000) >> 16) * ta +
									((c2 & 0xff0000) >> 16) * tb +
									((c3 & 0xff0000) >> 16) * tc +
									((c4 & 0xff0000) >> 16) * td) & 0xff;
			int b = (int)(((c1 & 0xff00) >> 8) * ta +
									((c2 & 0xff00) >> 8) * tb +
									((c3 & 0xff00) >> 8) * tc +
									((c4 & 0xff00) >> 8) * td) & 0xff;
			int a = (int)((c1 & 0xff) * ta +
									(c2 & 0xff) * tb +
									(c3 & 0xff) * tc +
									(c4 & 0xff) * td) & 0xff;

			int src_col = (r << 24) | (g << 16) | (b << 8) | a;

			if(pixmap_blend) {
				int dst_col = to_RGBA8888(dst_pixmap->format, dpget((void*)dst_ptr));
				src_col = to_format(dst_pixmap->format, blend(src_col, dst_col));
			} else {
				src_col = to_format(dst_pixmap->format, src_col);
			}

			pset((void*)dst_ptr, src_col);
		}
	}
}

static inline void blit_linear(const Pixmap* src_pixmap, const Pixmap* dst_pixmap,
		   int src_x, int src_y, int src_width, int src_height,
		   int dst_x, int dst_y, int dst_width, int dst_height) {
	set_pixel_func pset = set_pixel_func_ptr(dst_pixmap->format);
	get_pixel_func pget = get_pixel_func_ptr(src_pixmap->format);
	get_pixel_func dpget = get_pixel_func_ptr(dst_pixmap->format);
	int sbpp = pixmap_bytes_per_pixel(src_pixmap->format);
	int dbpp = pixmap_bytes_per_pixel(dst_pixmap->format);
	int spitch = sbpp * src_pixmap->width;
	int dpitch = dbpp * dst_pixmap->width;

	int x_ratio = (src_width << 16) / dst_width + 1;
	int y_ratio = (src_height << 16) / dst_height + 1;

	int dx = dst_x;
	int dy = dst_y;
	int sx = src_x;
	int sy = src_y;
	int i = 0;
	int j = 0;

	for(;i < dst_height; i++) {
		sy = ((i * y_ratio) >> 16) + src_y;
		dy = i + dst_y;
		if(sy < 0 || dy < 0) continue;
		if(sy >= src_pixmap->height || dy >= dst_pixmap->height) break;

		for(j = 0 ;j < dst_width; j++) {
			sx = ((j * x_ratio) >> 16) + src_x;
			dx = j + dst_x;
			if(sx < 0 || dx < 0) continue;
			if(sx >= src_pixmap->width || dx >= dst_pixmap->width) break;

			const void* src_ptr = src_pixmap->pixels + sx * sbpp + sy * spitch;
			const void* dst_ptr = dst_pixmap->pixels + dx * dbpp + dy * dpitch;
			int src_col = to_RGBA8888(src_pixmap->format, pget((void*)src_ptr));

			if(pixmap_blend) {
				int dst_col = to_RGBA8888(dst_pixmap->format, dpget((void*)dst_ptr));
				src_col = to_format(dst_pixmap->format, blend(src_col, dst_col));
			} else {
				src_col = to_format(dst_pixmap->format, src_col);
			}

			pset((void*)dst_ptr, src_col);
		}
	}
}

static inline void blit(const Pixmap* src_pixmap, const Pixmap* dst_pixmap,
					   int src_x, int src_y, int src_width, int src_height,
					   int dst_x, int dst_y, int dst_width, int dst_height) {
	if(pixmap_scale == pixmap_SCALE_NEAREST)
		blit_linear(src_pixmap, dst_pixmap, src_x, src_y, src_width, src_height, dst_x, dst_y, dst_width, dst_height);
	if(pixmap_scale == pixmap_SCALE_BILINEAR)
		blit_bilinear(src_pixmap, dst_pixmap, src_x, src_y, src_width, src_height, dst_x, dst_y, dst_width, dst_height);
}

void pixmap_draw_pixmap(const Pixmap* src_pixmap, const Pixmap* dst_pixmap,
					   int src_x, int src_y, int src_width, int src_height,
					   int dst_x, int dst_y, int dst_width, int dst_height) {
	if(src_width == dst_width && src_height == dst_height) {
		blit_same_size(src_pixmap, dst_pixmap, src_x, src_y, dst_x, dst_y, src_width, src_height);
	} else {
		blit(src_pixmap, dst_pixmap, src_x, src_y, src_width, src_height, dst_x, dst_y, dst_width, dst_height);
	}
}
