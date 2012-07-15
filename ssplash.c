/* ssplash.c --- 
 * 
 * Copyright (c) 2009 kio_34@163.com. 
 * 
 * Author:  Su Chang (kio_34@163.com)
 * Maintainer: 
 * 
 * Created: 2009/11/11 17:35
 * Last-Updated: 2009/11/18 09:31
 * 
 */

/* Code: */

#include <stdio.h> 
#include <stdlib.h> 
#include <ctype.h>
#include <string.h> 
#include <unistd.h>
#include <fcntl.h> 
#include <signal.h>
#include <linux/fb.h> 
#include <linux/vt.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/mman.h> 

#include <jpeglib.h>
#include <gif_lib.h>
#include "ssplash.h"
#include <assert.h>

#define SPL_OK  1
#define SPL_ERR 0

#define DLOG(fmt, args...) fprintf(stdout, "ssplash: "fmt, ##args)
#define DERR(fmt, args...) fprintf(stderr, "ssplash: "fmt, ##args)

#define SSPLASH "ssplash"       /* program name */

static spl_t spl = { .repeat_loop = 1 };

/* descriptor: open device, testing color mode and mmap framebuffer */
static int
spl_open_fbdev(fb_t *fb)
{ 
    struct fb_fix_screeninfo fxi;

    if ( !fb->name )
        goto err_exit;

    /* open the framebuffer device */ 
    fb->fd = open(fb->name, O_RDWR); 
    if  (fb->fd < 0) { 
        DERR("Error opening /dev/fb0\n"); 
        goto err_exit;
    }

    /* Get the fixed screen info */ 
    if  (ioctl(fb->fd, FBIOGET_FSCREENINFO, &fxi)) { 
        DERR("ioctl(FBIOGET_FSCREENINFO) failed\n");
        goto err_exit;
    }
 
    /* get the variable screen info */ 
    if  (ioctl(fb->fd, FBIOGET_VSCREENINFO, &fb->fvs)) { 
        DERR("ioctl(FBIOGET_VSCREENINFO) failed\n");
        goto err_exit;
    }

    if  (fxi.visual != FB_VISUAL_TRUECOLOR && fxi.visual != FB_VISUAL_DIRECTCOLOR) { 
        DERR("non-TRUE/DIRECT-COLOR visuals (0x%x) not supported by ssplash.\n" , fxi.visual); 
        goto err_exit;
    }

    /* fill information */
    fb->width = fb->fvs.xres;
    fb->height = fb->fvs.yres;

    fb->r = fb->fvs.red.offset >> 3;
    fb->g = fb->fvs.green.offset >> 3;
    fb->b = fb->fvs.blue.offset >> 3;

    fb->Bpp = fb->fvs.bits_per_pixel >> 3;

    fb->smem_len = fxi.smem_len;

    /* 
     * fbdev says the frame buffer is at offset zero, and the mmio
     * region is immediately after.
     */ 

    /* mmap the framebuffer into our address space */ 
    fb->mem = ( void  *) mmap(0,                      /* start */ 
                              fxi.smem_len,           /* bytes */ 
                              PROT_READ | PROT_WRITE, /* prot */ 
                              MAP_SHARED,             /* flags */ 
                              fb->fd,             /* fd */ 
                              0);                     /* offset */ 

    if  (fb->mem == ( void  *) - 1) { 
        DERR("error: unable to mmap framebuffer\n" ); 
        goto err_exit;
    }

    return SPL_OK;

err_exit:
    close(fb->fd);
    return SPL_ERR;
}

 
static void
spl_close_fbdev(fb_t *fb)
{
    if (fb) {
        ioctl(fb->fd, FBIOPUT_VSCREENINFO, &fb->fvs);
        munmap(fb->mem, fb->smem_len); 
        close(fb->fd);
    }
}



#ifdef SPL_GRAPHIC_MODE
static int
spl_open_tty_graphic_mode(spl_t *spl)
{
    int fd;
    char vtname[128];
    int vtnumber;

    struct vt_stat vts;
    struct vt_mode vtm;

    fd = open ("/dev/tty0", O_WRONLY); /* use tty0 to get vt number */
    if (fd < 0) {
        perror("open consoledevice");
        return SPL_ERR;
    }

    if (ioctl(fd, VT_OPENQRY, &vtnumber) < 0 || (vtnumber < 0)) {
        perror("ioctl VT_OPENQRY");
        return SPL_ERR;
    }
    close(fd);

    sprintf(vtname, "/dev/tty%d", vtnumber);

    spl->con_fd = open(vtname, O_RDWR | O_NDELAY);
    if (spl->con_fd < 0) {
        perror("open tty");
        return SPL_ERR;
    }

    if (ioctl(spl->con_fd, VT_GETSTATE, &vts) == 0)
        spl->org_vt = vts.v_active;

    /* disconnect from tty */
    fd = open("/dev/tty", O_RDWR);
    if (fd >= 0) {
        ioctl(fd, TIOCNOTTY, 0);
        close(fd);
    }

    if (ioctl(spl->con_fd, VT_ACTIVATE, vtnumber) < 0) {
        perror("VT_ACTIVATE");
        close(spl->con_fd);
        return SPL_ERR;
    }

    if (ioctl(spl->con_fd, VT_WAITACTIVE, vtnumber) < 0) {
        perror("VT_WAITACTIVE");
        close(spl->con_fd);
        return SPL_ERR;
    }

    if (ioctl(spl->con_fd, VT_GETMODE, &vtm) < 0) {
        perror("VT_GETMODE");
        return SPL_ERR;
    }
    
    spl->vtm = vtm;

    vtm.mode = VT_PROCESS;
    vtm.relsig = SIGUSR1;
    vtm.acqsig = SIGUSR1;

    if (ioctl(spl->con_fd, VT_SETMODE, &vtm) < 0) {
        perror("VT_SETMODE");
        return SPL_ERR;
    }

    if (ioctl(spl->con_fd, KDSETMODE, spl->mode) < 0) {
        perror("KDSETMODE");
        close(spl->con_fd);
        return SPL_ERR;
    }

    return SPL_OK;
}

static void
spl_close_tty_graphic_mode(spl_t *spl)
{
    struct vt_mode vt;

    if (ioctl(spl->con_fd, KDSETMODE, KD_TEXT) < 0) {
        perror("KDSETMODE");
        close(spl->con_fd);
    }

    if (ioctl(spl->con_fd, VT_GETMODE, &vt) != -1) {
        vt.mode = VT_AUTO;
        ioctl(spl->con_fd, VT_SETMODE, &vt);
    }

    if (spl->org_vt > 0) {
        ioctl(spl->con_fd, VT_ACTIVATE, spl->org_vt);
    }

    close(spl->con_fd);
}
#endif

/* description: paint background color */
static void
spl_fb_paint_background(fb_t *fb)
{
    int x, y, fpix;
    unsigned char *p = (unsigned char*)fb->mem;

    if ((fb->bg_r > 0) && (fb->bg_g > 0) && (fb->bg_b > 0)) {

        for (y = 0; y < fb->height; y++)
            for (x = 0; x < fb->width; x++) {

                fpix = FB_POS(fb, x, y); /* compiler will optimize it */

                p[fpix + fb->r] = fb->bg_r;
                p[fpix + fb->g] = fb->bg_g;
                p[fpix + fb->b] = fb->bg_b;
            }
    }
}

/* descriptor: bitblit image data, and paint center of the framebuffer */
static void
spl_fb_bitblit(fb_t *fb, img_t *img)
{
    unsigned char *p, *d;
    int ix, iy, xgap, ygap, fpix=0, ipix;

    p = (unsigned char*)fb->mem;
    d = img->data;

    if (img->width > fb->width || img->height > fb->height) {
        DERR("image is too big");
        return;
    }

    /* check center or not  */
    if (img->x>0 && img->y>0) {
        xgap = img->x;
        ygap = img->y;
    }
    else {
        xgap = (fb->width - img->width) >> 1;
        ygap = (fb->height - img->height) >> 1;
    }

    for (iy = 0; iy < img->height; iy++) {
        for (ix = 0; ix < img->width; ix++) {

            fpix = FB_CENTER_POS(fb, img, ix, iy, xgap, ygap);
            ipix = IMG_POS(img, ix, iy);

            p[ fpix + fb->r ] = d[ ipix + img->r ];
            p[ fpix + fb->g ] = d[ ipix + img->g ];
            p[ fpix + fb->b ] = d[ ipix + img->b ];
        }
    }
}

static __inline__ void
spl_img_update_and_display_info(img_t *img, int w, int h, int rf, int gf, int bf, int Bpp)
{
    static char image_type[][8] = {
        "UNKNOW",
        "BMP",
        "JPEG",
        "GIF"
    };
    
    img->width = w;
    img->height = h;

    img->r = rf;
    img->g = gf;
    img->b = bf;
    img->Bpp = Bpp;

    DLOG("%s, %dx%d, r:%d, g:%d, b:%d, Bpp:%d\n",
         image_type[img->type >= (sizeof(image_type)/sizeof(image_type[0])) ? 0 : img->type],
         img->width, img->height, 
         img->r, img->g, img->b, img->Bpp);
}

#define BMP_HEAD  0x4d42
#define JPEG_HEAD 0xd8ff
#define JPEG_END  0xd9ff

#define GET_WORD(p) (p ? (((p[1]&0xff)<<8 | (p[0]&0xff))) : 0)

#define GIF_HEADER_A "GIF89a"
#define GIF_HEADER_B "GIF87a"

static __inline__ int
spl_img_get_type(FILE *fp)
{
    int image_type = TYPE_NONE;
    unsigned char *p, img_head[8] = {0};

    p = img_head;
    fread(p, 1, sizeof(img_head), fp);

    if (GET_WORD(p) == BMP_HEAD){
        image_type = TYPE_BMP;
        goto ok_out;
    }

    if (GET_WORD(p) == JPEG_HEAD) {
        
        fseek(fp, -2, SEEK_END);
        fread(&p[0], 1, sizeof(img_head), fp);

        if (GET_WORD(p) == JPEG_END) {
            image_type = TYPE_JPEG;
            goto ok_out;
        }
    }

    p[ strlen(GIF_HEADER_A) ] = 0;
    if (!strcmp((char*)p, GIF_HEADER_A) || !strcmp((char*)p, GIF_HEADER_B)){
        image_type = TYPE_GIF;
        goto ok_out;
    }

ok_out:
    fseek(fp, 0, SEEK_SET);
    return image_type;
}

static int
spl_img_draw_bmp(fb_t *fb, img_t *img, FILE *fp)
{
    int image_size;

    bmp_header_t sbh, *bh;
    bmp_info_header_t sbi, *bi;

    bh = &sbh;
    bi = &sbi;

    memset(bh, 0, sizeof(*bh));
    memset(bi, 0, sizeof(*bi));

    fread(bh, 1, sizeof(*bh), fp);
    fread(bi, 1, sizeof(*bi), fp);

    /* BMP is BGR, framebuffer is BGRA */
    spl_img_update_and_display_info(img,
                          GET_BMP_DWORD(bi, biWidth),
                          GET_BMP_DWORD(bi, biHeight),
                          2,
                          1,
                          0,
                          GET_BMP_WORD(bi, biPerPixel) >> 3);


    image_size = GET_BMP_DWORD(bh, bfSize) - GET_BMP_WORD(bh, bfOffBits);
    img->data = (unsigned char*)malloc(image_size);
    if ( !img->data ) {
        DERR("bmp malloc error !\n");
        goto bmp_close;
    }

    /* seek to the img->data start */
    fseek(fp, GET_BMP_WORD(bh, bfOffBits) - sizeof(*bh) - sizeof(*bi), SEEK_CUR);
    fread(img->data, 1, image_size, fp);
    
    spl_fb_paint_background(fb);
    spl_fb_bitblit(fb, img);

    free(img->data);
    return SPL_OK;

bmp_close:
    return SPL_ERR;
}


#ifdef SPL_SUPPORT_JPEG
static int
spl_img_draw_jpg(fb_t *fb, img_t *img, FILE *fp)
{
    JSAMPROW raw_pointer[1];    

    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct jds;

    jds.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&jds);

    jpeg_stdio_src(&jds, fp);
    jpeg_read_header(&jds, TRUE);

    /* fill image attribute */
    spl_img_update_and_display_info(img, jds.image_width, jds.image_height, 0, 1, 2, 3);


    img->data = (unsigned char*)malloc(jds.image_width * jds.image_height * jds.num_components);
    if (img->data == NULL) {
        DERR("jpeg malloc error !\n");
        goto err_malloc;
    }

    jpeg_start_decompress(&jds);
    while (jds.output_scanline < jds.output_height) {
        raw_pointer[0] = &img->data[(jds.output_height - jds.output_scanline - 1) * jds.output_width * jds.output_components];
        jpeg_read_scanlines(&jds, raw_pointer, 1);
    }
    jpeg_finish_decompress(&jds);
    jpeg_destroy_decompress(&jds);

    /* framebuffer is BGR, JPEG is RGB */
    spl_fb_paint_background(fb);
    spl_fb_bitblit(fb, img);

    free(img->data);    
    return SPL_OK;

err_malloc:
    jpeg_destroy_decompress(&jds);
    return SPL_ERR;
}
#else
#define spl_img_draw_jpg(fb, img, fp) (0)
#endif


#ifdef SPL_SUPPORT_GIF

#define GIF_EXT_GRAPHIC_CONTROL_LABEL 0xf9
#define GIF_POS(IMG, X, Y) ((X) + IMG_SCAN_LINE(IMG, Y)*(IMG)->width)

typedef struct {
    GifFileType *gf;            /* gif file process handler */
    int index;                  /* image file index */
    unsigned char *data;        /* decoded image buf */
    unsigned char *pre_data;    /* previous decoded image buf */

    unsigned char trans_flag;   /* transparent color flag */
    unsigned char trans_index;  /* transparent index */
} gif_t;


/* descriptor: call back function of gif color index method */
typedef unsigned char (*gif_get_color_index_method)(img_t *img, gif_t *gt, int x, int y);

/* descriptor: scan gif raster bytes, and locate it in the color map
 *
 * parameters: spl, ssplash instance handler
 *             data, image data pool
 *             rb, raster bit buffer pointer
 *             color_map, color_map of current image
 *             
 */
static void
spl_img_gif_index_color_to_rgb(fb_t *fb, img_t *img, gif_t *gt, gif_get_color_index_method regular_method)
{
    GifColorType *ct;
    SavedImage *si;
    ColorMapObject *color_map;

    int x, y, ipix, fpix, xgap, ygap;
    unsigned char idx, *p = (unsigned char*)fb->mem;

    if (img->x > 0 && img->y > 0) {
        xgap = img->x;
        ygap = img->y;
    }
    else {
        xgap = (fb->width - img->width) >> 1;
        ygap = (fb->height - img->height) >> 1;
    }

    si = &gt->gf->SavedImages[gt->index];
    color_map = si->ImageDesc.ColorMap ? si->ImageDesc.ColorMap : gt->gf->SColorMap;

    for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {

            ipix = IMG_POS(img, x, y);
            fpix = FB_CENTER_POS(fb, img, x, y, xgap, ygap);

            idx = regular_method(img, gt, x, y);
            ct = &color_map->Colors[ idx ]; /* get color from gif color map */

            /* transparent here */
            if (idx == gt->trans_index) {
                gt->data[ ipix + img->r ] = p[ fpix + fb->r ];
                gt->data[ ipix + img->g ] = p[ fpix + fb->g ];
                gt->data[ ipix + img->b ] = p[ fpix + fb->b ];
            }
            else {
                gt->data[ipix + img->r] = ct->Red;
                gt->data[ipix + img->g] = ct->Green;
                gt->data[ipix + img->b] = ct->Blue;
            }
        }
    }
}


/* description: process extension data 
 *
 * assumption : here will cause block with delay time instruction
 */
static void
spl_img_gif_process_extension_block(
    fb_t *fb,
    img_t *img,
    gif_t *gt,
    gif_get_color_index_method background_method)
{
    int i;
    SavedImage *si;
    ExtensionBlock *ext;

    si = &gt->gf->SavedImages[gt->index];

    for (i = 0; i < si->ExtensionBlockCount; i++) {
        char disposal_method;

        ext = &si->ExtensionBlocks[i];
        if ( !ext )
            continue;

        disposal_method = (ext->Bytes[0] >> 2) & 0x7;
        gt->trans_flag = ext->Bytes[0] & 0x1;

        /* do not deal with user input */
        switch (disposal_method) {

            case 2:             /* restore to backgroud color */
                spl_img_gif_index_color_to_rgb(fb, img, gt, background_method);
                img->data = gt->data;
                spl_fb_bitblit(fb, img);                
                break;
                    
            case 3:             /* restore to previous */
                memcpy(gt->data, gt->pre_data, img->width * img->height * img->Bpp);
                break;

            case 0:             /* decoder will do nothing */
            case 1:             /* left graphic in that place */
            default:            /* no error code here */
                break;
        }

        /* delay time control */
        if (ext->Function == GIF_EXT_GRAPHIC_CONTROL_LABEL)
            usleep(10000 * GET_WORD((&ext->Bytes[1])));

        /* transparent control */
        if (gt->trans_flag)
            gt->trans_index = ext->Bytes[3];
    }
}

/* description: return color index of regular image pos */
static unsigned char
gif_img_gif_get_color_index_from_raster_bits(img_t *img, gif_t *gt, int x, int y)
{
    unsigned char *rb = gt->gf->SavedImages[gt->index].RasterBits;
    return rb[ GIF_POS(img, x, y) ];
}

/* description: use background color index */
static unsigned char
gif_img_gif_get_color_index_from_background_bits(img_t *img, gif_t *gt, int x, int y)
{
    unsigned char *rb = gt->gf->SavedImages[gt->index].RasterBits;
    return rb[ gt->gf->SBackGroundColor ];
}


/* description: about the GIF syntax and decoder information, refers to
 *              http://local.wasp.uwa.edu.au/~pbourke/dataformats/gif/, (en); or
 *              http://blog.csdn.net/friendwaters/archive/2008/07/30/2737328.aspx, (cn)
 */
static int
spl_img_draw_gif(fb_t *fb, img_t *img, int *repeat_loop)
{
    SavedImage *si;
    gif_t s_gt, *gt;
    int i, img_size, gif_x, gif_y, gif_w, gif_h;

    gt = &s_gt;

    if ((gt->gf = DGifOpenFileName(img->name)) == NULL) {
        PrintGifError();
        goto err_exit;
    }

    spl_img_update_and_display_info(img, gt->gf->SWidth, gt->gf->SHeight, 0, 1, 2, 3);

    gt->data = gt->pre_data = NULL;
    img_size = img->width * img->height * img->Bpp;

    gt->data = (unsigned char*)malloc(img_size);
    img->data = gt->data;
    gt->pre_data = (unsigned char*)malloc(img_size);
    if ( !gt->data || !gt->pre_data )
        goto err_malloc;


    /* decoder all image data here, assume enougth memory */
    if (DGifSlurp(gt->gf) == GIF_ERROR) {
        PrintGifError();
        goto err_malloc;
    }

    gif_w = gt->gf->SWidth;     /* canvas size */
    gif_h = gt->gf->SHeight;

    gif_x = img->x > 0 ? img->x : (fb->width - gif_w) >> 1; /* base pos */
    gif_y = img->y > 0 ? img->x : (fb->height - gif_h) >> 1;

    for (i = 0; *repeat_loop; i++) {

        gt->index = i % gt->gf->ImageCount;
        si = &gt->gf->SavedImages[gt->index];

        /* refresh image size, FIXME: should use seperate image handler to deal with it */
        img->width = si->ImageDesc.Width;
        img->height = si->ImageDesc.Height;

        img->x = gif_x + si->ImageDesc.Left;
        img->y = gif_y + si->ImageDesc.Top;


        /* process gif extension block, this function will cause block(time delayed) or
         * repaint canvas with bg color or previous image, all by decoded instructions
         */
        spl_img_gif_process_extension_block(fb, img, gt, gif_img_gif_get_color_index_from_background_bits);


        /* transform gif indexed color to flat rgb buffer */
        spl_img_gif_index_color_to_rgb(fb, img, gt, gif_img_gif_get_color_index_from_raster_bits);


        /* painting the decoded gif image */
        spl_fb_bitblit(fb, img);


        /* recored previous color */
        memcpy(gt->pre_data, gt->data, img_size);
    } /* end 'for gh->ImageCount' */

    DGifCloseFile(gt->gf);
    return SPL_OK;


err_malloc:
    if (gt->data) free(gt->data);
    if (gt->pre_data) free(gt->pre_data);
    DGifCloseFile(gt->gf);    
err_exit:
    return SPL_ERR;
}
#else
#define spl_img_draw_gif(spl, img_file) (0)
#endif


#ifdef SPL_SUPPORT_PROGRESS_BAR
/* descriptor: draw rectangle fo r, with color c, if encouter parameters
 *             error, return silently
 */
static int
spl_fb_draw_rectangle(fb_t *fb, const rec_t *r, const pix_t *c)
{
    int fx, fy, fpix;
    unsigned char *p = (unsigned char*)fb->mem;

    fx = r->x + r->w;
    if (fx > fb->width || fx > fb->height)
        return SPL_ERR;

    for (fy = r->y; fy < (r->y + r->h); fy++) {
        for (fx = r->x; fx < (r->x + r->w); fx++) {

            fpix = FB_POS(fb, fx, fy);

            p[ fpix + fb->r ]  = c->r;
            p[ fpix + fb->g ]  = c->g;
            p[ fpix + fb->b ]  = c->b;
        }
    }

    return SPL_OK;
}

/* descriptor: update highlight units of progress bar, hardcoded with XP
 *             style
 */
static __inline__ void
spl_bar_update_unit_rec(rec_t *ru, const rec_t *rbar, const bar_t *bt)
{
    ru->x += (ru->w << 1);
    if (ru->x > (rbar->x + rbar->w + (ru->w << 1)*(bt->r_unit + 1)))
        ru->x = rbar->x;
}


/* descriptor: draw progress bar with their apperance and animation
 *             description bt
 */
static int
spl_bar_draw_progress(spl_t *spl)
{
    int i;

    fb_t *fb = spl->fb;
    bar_t *br = &spl->br;
    pix_t pu, *pbar = &br->pbar;
    rec_t ru, rmu, *rbar = &br->rbar;
    
    ru = br->ru;
    pu = br->pu;

    if ( !fb || !br->init)      /* only support one fb now */
        return SPL_ERR;

    spl_fb_draw_rectangle(fb, rbar, pbar);

    rmu = ru;
    for (i = 0; spl->repeat_loop; i++) {

        if (ru.x <= (rbar->x + rbar->w))
            spl_fb_draw_rectangle(fb, &ru, &pu);

        usleep(br->usec);
        spl_bar_update_unit_rec(&ru, rbar, br);

        if (i >= br->r_unit) {
            
            spl_fb_draw_rectangle(fb, &rmu, pbar);
            spl_bar_update_unit_rec(&rmu, rbar, br);
        }
    }
    
    return SPL_OK;
}
#endif /* SPL_SUPPORT_PROGRESS_BAR */



/* description: whether we should invoke splash program, decided by
 *              kernel command line
 */
static int
spl_is_to_invoke_splash(const char *file)
{
    FILE *fp;
    char *invoke_cmd=" splash", cmdline[256];

    if ((fp = fopen(file, "r")) == NULL)
        return SPL_ERR;
    
    while ( !feof(fp) ) {
        if ( fgets(cmdline, sizeof(cmdline), fp) ) {
            if ( strstr(cmdline, invoke_cmd) )
                goto ok_invoke;
        }
    }
    fclose(fp);
    return SPL_ERR;

ok_invoke:
    fclose(fp);
    return SPL_OK;
}

#define CONF_NEW_INSTANCE(H, TYPE, NAME)        \
    do {                                        \
        while (*(H) != NULL)                    \
            H = &(*(H))->next;                  \
        *(H) = (TYPE*)malloc(sizeof(**(H)));    \
        if (*(H) == NULL) {                     \
            DERR("conf malloc error !\n");      \
            return SPL_ERR;                     \
        }                                       \
        memset(*(H), 0, sizeof(**(H)));         \
        (*(H))->name = strdup(NAME);            \
    } while (0)

#define CONF_SET_VAL(H, M, VAL)                 \
    do {                                        \
        if (*(H) != NULL) {                     \
            while ((*(H))->next)                \
                (H) = &(*(H))->next;            \
            if ((VAL)[0] != '\0')               \
                (*(H))->M = atoi(VAL) & 0xFF;   \
            else                                \
                (*(H))->M = -1;                 \
        }                                       \
        else {                                  \
            DERR("conf set val error !\n");     \
            return SPL_ERR;                     \
        }                                       \
    } while (0)

/* description: construct instance base the key and val pair */
static int
spl_conf_construct_instance(spl_t *spl, char *key, char *val)
{
    fb_t **fb;
    img_t **img;
    bar_t *br = &spl->br;
    int ival;

    if ( !(spl && key) )
        return SPL_ERR;

    ival = atoi(val);
    
    if ( !strcmp(key, "fb_dev") ) { /* fb dev to opened */
        
        fb = &spl->fb;
        CONF_NEW_INSTANCE(fb, fb_t, val);

        spl_open_fbdev(*fb);
    }
    else if ( !strcmp(key, "fb_bg_r") ) { /* fb background RGB */
        fb = &spl->fb;
        CONF_SET_VAL(fb, bg_r, val);
    }
    else if ( !strcmp(key, "fb_bg_g") ) {
        fb = &spl->fb;
        CONF_SET_VAL(fb, bg_g, val);        
    }
    else if ( !strcmp(key, "fb_bg_b") ) {
        fb = &spl->fb;
        CONF_SET_VAL(fb, bg_b, val);
    }


    /* image */
    else if ( !strcmp(key, "img_name") ) { /* image file name */

        img = &spl->img;
        CONF_NEW_INSTANCE(img, img_t, val); /* create image discrption */
    }

    /* image x,y to framebuffer, omit or 0 will be centerlized */
    else if ( !strcmp(key, "img_x") ) {
        img = &spl->img;
        CONF_SET_VAL(img, x, val);
    }
    else if ( !strcmp(key, "img_y") ) {
        img = &spl->img;
        CONF_SET_VAL(img, y, val);
    }


    /* progress bar */
    else if ( !strcmp(key, "bar_y") ) { /* x will auto centered */
        br->rbar.y = ival;
    }
    else if ( !strcmp(key, "bar_w") ) { /* bar width */
        br->rbar.w = ival;
    }
    else if ( !strcmp(key, "bar_h") ) {
        br->rbar.h = ival;
    }
    else if ( !strcmp(key, "bar_r") ) { /* progress bar's RGB */
        br->pbar.r = ival;
    }
    else if ( !strcmp(key, "bar_g") ) {
        br->pbar.g = ival;
    }
    else if ( !strcmp(key, "bar_b") ) {
        br->pbar.b = ival;
    }
    else if ( !strcmp(key, "bar_unit_w") ) { /* unit's heigth will retrieve from bar's */
        br->ru.w = ival;
    }
    else if ( !strcmp(key, "bar_unit_r") ) { /* unit's RGB */
        br->pu.r = ival;
    }
    else if ( !strcmp(key, "bar_unit_g") ) {
        br->pu.g = ival;
    }
    else if ( !strcmp(key, "bar_unit_b") ) {
        br->pu.b = ival;
    }
    else if ( !strcmp(key, "bar_v_unit") ) { /* v_unit x unit_w = bar_width */
        br->v_unit = ival;
    }
    else if ( !strcmp(key, "bar_r_unit") ) { /* visible unit */
        br->r_unit = ival;
    }
    else if ( !strcmp(key, "bar_usec") ) { /* delay ms */
        br->usec = ival;
    }

    /* bar category, auto compute left attribute */
    if ( !strncmp(key, "bar", strlen("bar")) ) {
        fb = &spl->fb;
        br->init = 1;

        if (br->rbar.w > 0 && (*fb)->width > 0)
            br->rbar.x = ((*fb)->width - br->rbar.w) >> 1;

        if (br->rbar.x > 0)
            br->ru.x = br->rbar.x;
        if (br->rbar.y > 0)
            br->ru.y = br->rbar.y;

        if (br->rbar.w > 0 && br->v_unit > 0)
            br->ru.w = br->rbar.w / (br->v_unit<<1);
        if (br->rbar.h > 0)
            br->ru.h = br->rbar.h;

        /* DLOG("ru x %d, y %d, h %d, w %d\n", br->ru.x, br->ru.y, br->ru.h, br->ru.w); */
    }

    return SPL_OK;
}

static int
spl_init_ssplash(spl_t *spl, char *conf_name)
{
    FILE *fp;
    char line[128], *key, *val, *d;

    if ((fp = fopen(conf_name, "r")) == NULL)
        goto err_open;

    while ( !feof(fp) ) {

        fgets(line, sizeof(line), fp);

        d = strchr(line, '=');
        if (!d) continue;

        key = line;
        val = d + 1;
        line[d - line] = '\0';

        for (d = val; *d != '\0'; d++)
            if (*d == '\n' || *d == '\r')
                *d = '\0';

        if (spl_conf_construct_instance(spl, key, val) == SPL_ERR)
            goto err_conf;
    }

    fclose(fp);

    if (spl_open_tty_graphic_mode(spl) == SPL_ERR)
        goto err_conf;

    return SPL_OK;


err_conf:
    fclose (fp);
err_open:
    return SPL_ERR;
}

static void
spl_dinit_ssplash(spl_t *spl)
{
    fb_t *fb, *fb_next;
    img_t *img, *img_next;

    spl_close_tty_graphic_mode(spl);

    for (fb = spl->fb; fb; fb = fb_next) {
        fb_next = fb->next;
        spl_close_fbdev(fb);
        if (fb->name)
            free(fb->name);
        free(fb);
    }

    for (img = spl->img; img; img = img_next) {
        img_next = img->next;
        if (img->name)
            free(img->name);
        free(img);
    }
}


static void
spl_display_splash(spl_t *spl)
{
    FILE *fp;
    fb_t *fb, *fb_next;
    img_t *img, *img_next;

    for (fb = spl->fb; fb; fb = fb_next) {
        fb_next = fb->next;

        for (img = spl->img; img; img = img_next) {
            img_next = img->next;

            fp = fopen(img->name, "rb");
            if (fp == NULL)
                continue;

            switch((img->type = spl_img_get_type(fp))) {

                case TYPE_BMP:
                    spl_img_draw_bmp(fb, img, fp);
                    break;

                case TYPE_JPEG:
                    spl_img_draw_jpg(fb, img, fp);
                    break;
                
                case TYPE_GIF:
                    spl_img_draw_gif(fb, img, &spl->repeat_loop);
                    break;

                case TYPE_NONE:
                default:
                    break;
            }

            fclose(fp);
        }
    }
}


static void
spl_signal_handler(int sig_num)
{
    spl.repeat_loop = 0;
    DLOG("got signal %d\n", sig_num);
}


#define CMD_LINE_FILE "/proc/cmdline"
int
main(int argc, char *argv[])
{
    int recover = 0; /* for initramfs to recover TEXT mode from GRAPHIC mode */
    char *ssplash_conf;

    /* whether to invoke ssplash */
    if ( !spl_is_to_invoke_splash(CMD_LINE_FILE) )
        return 0;


    /* install signal handler */
    signal(SIGTERM, spl_signal_handler);
    signal(SIGINT, spl_signal_handler);


    /* parse command line */
    ssplash_conf = NULL;
    switch (argc) {
        case 2: ssplash_conf = argv[1];
            if ( !strcmp(ssplash_conf, "recover") ) {
                recover = 1;
                DLOG("recover vt\n");
            }
            spl.mode = recover ? KD_TEXT : KD_GRAPHICS;
            break;
        default:
            DERR("Usage:\n");
            DERR("%s [file.conf | recover]\n", argv[0]);
            return 0;
    }


    /* init ssplash */
    if (spl_init_ssplash(&spl, ssplash_conf) == SPL_ERR)
        goto err_dinit;

    /* test and show image file */
    if (!recover) {
        spl_display_splash(&spl);
        spl_bar_draw_progress(&spl);
    }

#ifdef TERMINAL_DEBUG
    {
        char ch;
        int fd, fd2;
        fd = open("/dev/stdin", O_NONBLOCK); /* contorl */
        fd2 = open("/dev/tty1", O_NONBLOCK);

        while (1) {
            usleep(1);

            if ( !read(fd, &ch, 1) 
                 && !read(fd2, &ch, 1))
                continue;

            if (ch == 'q')
                goto exit_while;
        }
    exit_while:
        close(fd);
        close(fd2);
    }
#endif  /* TERMINAL_DEBUG */

err_dinit:
    spl_dinit_ssplash(&spl);

    return  0; 
}

/* ssplash.c ends here */
