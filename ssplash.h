/* ssplash.h --- 
 * 
 * Copyright (c) 2009 kio_34@163.com. 
 * 
 * Author:  Su Chang (kio_34@163.com)
 * Maintainer: 
 * 
 * Created: 2009/11/11 17:35
 * Last-Updated: 2009/11/17 19:17
 * 
 */

/* Commentary: 
 * 
 * 
 */

/* Code: */

#ifndef _SSPLASH_H_
#define _SSPLASH_H_ 0

#define TYPE_NONE 0
#define TYPE_BMP  1
#define TYPE_JPEG 2
#define TYPE_GIF  3

typedef struct s_fb {

    struct fb_var_screeninfo fvs;

    int fd;
    void *mem;
    unsigned int smem_len;
    
    int width;
    int height;

    unsigned char r;            /* RGB byte offset */
    unsigned char g;
    unsigned char b;
    unsigned char Bpp;          /* byte per pix */

    /* listed below will be read from conf */
    char *name;                  /* dev name */
    int bg_r;                    /* fb background RGB */
    int bg_g;
    int bg_b;

    struct s_fb *next;
} fb_t;

typedef struct s_img {

    int width;
    int height;

    unsigned char r;            /* RGB byte offset */
    unsigned char g;
    unsigned char b;
    unsigned char Bpp;          /* byte per pix */

    int type;                   /* image type */

    unsigned char *data;        /* decoded data */

    /* listed below will be read from conf */
    char *name;                 /* raw image name */
    int x;
    int y;

    struct s_img *next;
} img_t;


/* Pix and Progress bar utilities
 */
typedef struct {
    int x, y, w, h;
} rec_t;


typedef struct {
    unsigned char r, g, b;
} pix_t;


typedef struct {
    rec_t rbar;                 /* rect of bar */
    rec_t ru;                   /* rect of unit */
    
    pix_t pbar;                 /* bar pix */
    pix_t pu;                   /* unit pix */

    unsigned int v_unit;        /* virtual unit */
    unsigned int r_unit;        /* real unit */
    unsigned int usec;          /* animation interval */

    int init;
} bar_t;

typedef struct {

    fb_t *fb;
    img_t *img;
    bar_t br;

    int repeat_loop;            /* repeat progress bar or gif */
    int mode;                   /* recovery or not */

#ifdef SPL_GRAPHIC_MODE
    int con_fd;
    int org_vt;
    struct vt_mode vtm;
#endif
} spl_t;



/* transform image scan line */
#define IMG_SCAN_LINE(IMG, Y)                   \
    ((IMG)->height - 1 - (Y))


/*
 * get image centered pos in the framebuffer
 *
 * 2-D image demention to linear framebuffer address, then WG,HG is the
 * width and height gap between image and framebuffer.
 */
#define FB_CENTER_POS(FB, IMG, X, Y, WG, HG)                        \
    (((X) + (WG) + (IMG_SCAN_LINE(IMG, Y) + (HG)) * (FB)->width) * (FB)->Bpp)


/* get image pos, 2-D to linear */
#define IMG_POS(IMG, X, Y)                              \
    (((X) + (Y)*(IMG)->width) * (IMG)->Bpp)


/* get framebuffer pos, 2-D to linear */
#define FB_POS(FB, X, Y)                        \
    (((X) + (Y)*(FB)->width) * (FB)->Bpp)



/* BMP header and utilities
 */
typedef struct s_bmp_header { /* bmfh */ 

    unsigned char bfType_1;
    unsigned char bfType_2;

    unsigned char bfSize_1;
    unsigned char bfSize_2;
    unsigned char bfSize_3;
    unsigned char bfSize_4;

    unsigned char bfReserved1_1;
    unsigned char bfReserved1_2;

    unsigned char bfReserved2_1;
    unsigned char bfReserved2_2;

    unsigned char bfOffBits_1;
    unsigned char bfOffBits_2;
    unsigned char bfOffBits_3;
    unsigned char bfOffBits_4;
} bmp_header_t;

typedef struct s_bmp_info_header { /* bmih */ 

    unsigned char biSize_1; 
    unsigned char biSize_2; 
    unsigned char biSize_3; 
    unsigned char biSize_4; 

    unsigned char biWidth_1;
    unsigned char biWidth_2;
    unsigned char biWidth_3;
    unsigned char biWidth_4;

    unsigned char biHeight_1;
    unsigned char biHeight_2;
    unsigned char biHeight_3;
    unsigned char biHeight_4;

    unsigned char biPlanes_1;
    unsigned char biPlanes_2;

    unsigned char biPerPixel_1;
    unsigned char biPerPixel_2;

    unsigned char biCompression_1; 
    unsigned char biCompression_2; 
    unsigned char biCompression_3; 
    unsigned char biCompression_4; 

    unsigned char biSizeImage_1;
    unsigned char biSizeImage_2;
    unsigned char biSizeImage_3;
    unsigned char biSizeImage_4;

    unsigned char biXPelsPerMeter_1;
    unsigned char biXPelsPerMeter_2;
    unsigned char biXPelsPerMeter_3;
    unsigned char biXPelsPerMeter_4;

    unsigned char biYPelsPerMeter_1;
    unsigned char biYPelsPerMeter_2;
    unsigned char biYPelsPerMeter_3;
    unsigned char biYPelsPerMeter_4;

    unsigned char biClrUsed_1;
    unsigned char biClrUsed_2;
    unsigned char biClrUsed_3;
    unsigned char biClrUsed_4;

    unsigned char biClrImportant_1;
    unsigned char biClrImportant_2;
    unsigned char biClrImportant_3;
    unsigned char biClrImportant_4;
} bmp_info_header_t;

#define GET_BMP_WORD(H, T)  ((H->T##_2)<<8 | (H->T##_1))
#define GET_BMP_DWORD(H, T) ((H->T##_4)<<24 | (H->T##_3)<<16 | (H->T##_2)<<8 | (H->T##_1))

#endif  /* _SSPLASH_H_ */

/* ssplash.h ends here */
