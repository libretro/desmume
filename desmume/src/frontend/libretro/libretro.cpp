#include <stdarg.h>
#include <libretro.h>

#if defined(VITA)
  int _newlib_heap_size_user = 128 * 1024 * 1024;
#endif

#include "cheatSystem.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "debug.h"
#include "render3D.h"
#include "rasterize.h"
#include "saves.h"
#include "firmware.h"
#include "GPU.h"
#include "SPU.h"
#include "emufile.h"
#include "common.h"
#include "path.h"

#include "streams/file_stream.h"

#ifdef HAVE_OPENGL
#include "OGLRender.h"
#include "OGLRender_3_2.h"

static GLuint pbo = 0;
static GLuint fbo = 0;
static GLuint tex = 0;

static GLuint current_texture_width = 0;
static GLuint current_texture_height = 0;

typedef void (glBindFramebufferProc) (GLenum, GLuint);
static glBindFramebufferProc *glBindFramebuffer = NULL;
typedef void (glGenFramebuffersProc) (GLsizei, GLuint *);
static glGenFramebuffersProc *glGenFramebuffers = NULL;
typedef void (glDeleteFramebuffersProc) (GLsizei, GLuint *);
static glDeleteFramebuffersProc *glDeleteFramebuffers = NULL;
typedef void (glFramebufferTexture2DProc) (GLenum, GLenum, GLenum, GLuint, GLint);
static glFramebufferTexture2DProc *glFramebufferTexture2D = NULL;
typedef void (glBlitFramebufferProc) (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
static glBlitFramebufferProc *glBlitFramebuffer = NULL;
typedef void *(glMapBufferRangeProc) (GLenum, GLintptr, GLsizeiptr, GLbitfield);
static glMapBufferRangeProc *glMapBufferRange = NULL;

static GLuint internal_format = GL_RGB565;
static GLuint texture_format  = GL_UNSIGNED_SHORT_5_6_5;
static GLuint texture_type    = GL_RGB;

#endif

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

enum {
   LAYOUT_TOP_BOTTOM = 0,
   LAYOUT_BOTTOM_TOP = 1,
   LAYOUT_LEFT_RIGHT = 2,
   LAYOUT_RIGHT_LEFT = 3,
   LAYOUT_TOP_ONLY = 4,
   LAYOUT_BOTTOM_ONLY = 5,
   LAYOUT_HYBRID_TOP_ONLY = 6,
   LAYOUT_HYBRID_BOTTOM_ONLY = 7,
   LAYOUTS_MAX = 8
};

retro_log_printf_t log_cb = NULL;
static retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_environment_t environ_cb = NULL;
static struct retro_hw_render_callback hw_render;

static bool libretro_supports_bitmasks = false;

volatile bool execute = 0;

static int delay_timer = 0;
static bool mouse_enable = false;
static double mouse_speed= 1.0;
static double mouse_x_delta = 0.0;
static double mouse_y_delta = 0.0;
static int pointer_device_l = 0;
static int pointer_device_r = 0;
static int analog_stick_deadzone;
static int analog_stick_acceleration = 2048;
static int analog_stick_acceleration_modifier = 0;
static int nds_screen_gap = 0;
static bool opengl_mode = false;
static int hybrid_layout_scale = 1;
static int hybrid_layout_ratio = 3;
static bool hybrid_layout_showbothscreens = true;
static bool hybrid_cursor_always_smallscreen = true;
static uint16_t pointer_colour = 0xFFFF;
static uint32_t pointer_color_32 = 0xFFFFFFFF;
static int bpp = 2;
static int current_max_width = 0;
static int current_max_height = 0;
static int input_rotation = 0;

static retro_pixel_format colorMode = RETRO_PIXEL_FORMAT_RGB565;
static uint32_t frameSkip;
static uint32_t frameIndex;

static uint16_t *screen_buf = NULL;
static size_t screen_buf_byte_size = 0;

extern GPUSubsystem *GPU;

unsigned GPU_LR_FRAMEBUFFER_NATIVE_WIDTH  = 256;
unsigned GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT = 192;
unsigned scale = 1;

int current_layout = LAYOUT_TOP_BOTTOM;

const int NDS_MAX_SCREEN_GAP = 100;

static inline int gap_size()
{
    int max_gap = NDS_MAX_SCREEN_GAP;
    if (current_layout == LAYOUT_HYBRID_BOTTOM_ONLY || current_layout == LAYOUT_HYBRID_TOP_ONLY)
    {
        if (hybrid_layout_ratio == 3)
            max_gap = 64;
        else
            max_gap = 0;
    }

    if (nds_screen_gap > max_gap)
       return max_gap;

    return nds_screen_gap;
}

struct LayoutData
{
   uint16_t *dst;
   uint16_t *dst2;
   uint32_t touch_x;
   uint32_t touch_y;
   uint32_t width;
   uint32_t height;
   uint32_t pitch;
   size_t offset1;
   size_t offset2;
   size_t byte_size;
   bool draw_screen1;
   bool draw_screen2;
};

static bool touchEnabled;

static unsigned host_get_language(void)
{
   static const u8 langconv[]={ // libretro to NDS
      NDS_FW_LANG_ENG,
      NDS_FW_LANG_JAP,
      NDS_FW_LANG_FRE,
      NDS_FW_LANG_SPA,
      NDS_FW_LANG_GER,
      NDS_FW_LANG_ITA
   };

   unsigned lang = RETRO_LANGUAGE_ENGLISH;
   environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &lang);
   if (lang >= 6) lang = RETRO_LANGUAGE_ENGLISH;
   return langconv[lang];
}

static inline int32_t Saturate(int32_t min, int32_t max, int32_t aValue)
{
   return std::max(min, std::min(max, aValue));
}

static int32_t TouchX;
static int32_t TouchY;

static const uint32_t FramesWithPointerBase = 60 * 10;
static int32_t FramesWithPointer;

static void DrawPointerLine_32(uint32_t* aOut, uint32_t aPitchInPix)
{
   for(int i = 0; i < (5 * scale) ; i ++)
      aOut[aPitchInPix * i] = pointer_color_32;
}

static void DrawPointerLineSmall_32(uint32_t* aOut, uint32_t aPitchInPix, int factor)
{
   for(int i = 0; i < (factor * scale) ; i ++)
      aOut[aPitchInPix * i] = pointer_color_32;
}

static void DrawPointerLine(uint16_t* aOut, uint32_t aPitchInPix)
{
   for(int i = 0; i < (5 * scale) ; i ++)
      aOut[aPitchInPix * i] = pointer_colour;
}

static void DrawPointerLineSmall(uint16_t* aOut, uint32_t aPitchInPix, int factor)
{
   for(int i = 0; i < (factor * scale) ; i ++)
      aOut[aPitchInPix * i] = pointer_colour;
}

static void DrawPointer(uint16_t* aOut, uint32_t aPitchInPix)
{
   if(FramesWithPointer-- < 0)
      return;

   TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), TouchX);
   TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), TouchY);

   if (colorMode != RETRO_PIXEL_FORMAT_XRGB8888)
   {
       if (TouchX >   (5 * scale)) DrawPointerLine(&aOut[TouchY * aPitchInPix + TouchX - (5 * scale) ], 1);
       if (TouchX < (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH - (5 * scale) )) DrawPointerLine(&aOut[TouchY * aPitchInPix + TouchX + 1], 1);
       if (TouchY >   (5 * scale)) DrawPointerLine(&aOut[(TouchY - (5 * scale) ) * aPitchInPix + TouchX], aPitchInPix);
       if (TouchY < (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-(5 * scale) )) DrawPointerLine(&aOut[(TouchY + 1) * aPitchInPix + TouchX], aPitchInPix);
   }
   else
   {
       uint32_t *aOut_32 = (uint32_t *) aOut;
       if (TouchX >   (5 * scale)) DrawPointerLine_32(&aOut_32[TouchY * aPitchInPix + TouchX - (5 * scale) ], 1);
       if (TouchX < (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH - (5 * scale) )) DrawPointerLine_32(&aOut_32[TouchY * aPitchInPix + TouchX + 1], 1);
       if (TouchY >   (5 * scale)) DrawPointerLine_32(&aOut_32[(TouchY - (5 * scale) ) * aPitchInPix + TouchX], aPitchInPix);
       if (TouchY < (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-(5 * scale) )) DrawPointerLine_32(&aOut_32[(TouchY + 1) * aPitchInPix + TouchX], aPitchInPix);
   }
}

static void DrawPointerHybrid(uint16_t* aOut, uint32_t aPitchInPix, bool large)
{
    unsigned height,width;
    unsigned DrawX, DrawY;
    int factor;

    if (FramesWithPointer-- < 0)
        return;

    if (!large)
    {
        aOut += aPitchInPix * hybrid_layout_scale * bpp / 2 * (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT / hybrid_layout_ratio + gap_size() * scale);
        width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * hybrid_layout_scale / hybrid_layout_ratio;
        height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * hybrid_layout_scale / hybrid_layout_ratio;

        DrawX = Saturate(0, (width - 1), TouchX * hybrid_layout_scale / hybrid_layout_ratio);
        DrawY = Saturate(0, (height - 1), TouchY * hybrid_layout_scale / hybrid_layout_ratio);
    }
    else
    {
        height = hybrid_layout_scale *GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
        width = hybrid_layout_scale *GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
        DrawX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH - 1), TouchX);
        DrawY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT - 1), TouchY);
    }

    if(large)
    {
        factor = 5 * hybrid_layout_scale;
        if(hybrid_layout_scale == hybrid_layout_ratio)
        {
            DrawX = hybrid_layout_ratio * DrawX;
            DrawY = hybrid_layout_ratio * DrawY;
        }
    }
    else if(hybrid_layout_scale == hybrid_layout_ratio)
        factor = 2 * hybrid_layout_ratio;
    else
        factor = hybrid_layout_ratio;

    if (colorMode != RETRO_PIXEL_FORMAT_XRGB8888)
    {
        if (DrawX >   (factor * scale)) DrawPointerLineSmall(&aOut[DrawY * aPitchInPix + DrawX - (factor * scale) ], 1, factor);
        if (DrawX < (width - (factor * scale) )) DrawPointerLineSmall(&aOut[DrawY * aPitchInPix + DrawX + 1], 1, factor);
        if (DrawY >   (factor * scale)) DrawPointerLineSmall(&aOut[(DrawY - (factor * scale) ) * aPitchInPix + DrawX], aPitchInPix, factor);
        if (DrawY < (height-(factor * scale) )) DrawPointerLineSmall(&aOut[(DrawY + 1) * aPitchInPix + DrawX], aPitchInPix, factor);
    }
    else
    {
        uint32_t *aOut_32 = (uint32_t *) aOut;
        if (DrawX >   (factor * scale)) DrawPointerLineSmall_32(&aOut_32[DrawY * aPitchInPix + DrawX - (factor * scale) ], 1, factor);
        if (DrawX < (width - (factor * scale) )) DrawPointerLineSmall_32(&aOut_32[DrawY * aPitchInPix + DrawX + 1], 1, factor);
        if (DrawY >   (factor * scale)) DrawPointerLineSmall_32(&aOut_32[(DrawY - (factor * scale) ) * aPitchInPix + DrawX], aPitchInPix, factor);
        if (DrawY < (height-(factor * scale) )) DrawPointerLineSmall_32(&aOut_32[(DrawY + 1) * aPitchInPix + DrawX], aPitchInPix, factor);
    }
}

static bool NDS_3D_ChangeCore(int newCore)
{
        int value = GPU->Change3DRendererByID (newCore);
        return value;
}

#define CONVERT_COLOR(color) (((color & 0x001f) << 11) | ((color & 0x03e0) << 1) | ((color & 0x0200) >> 4) | ((color & 0x7c00) >> 10))
void conv_0rgb1555_rb_swapped_rgb565(void * __restrict output_, const void * __restrict input_,
      int width, int height,
      int out_stride, int in_stride)
{
   int h, max_width;
   const uint16_t *input   = (const uint16_t*)input_;
   uint16_t *output        = (uint16_t*)output_;

#if defined(__SSE2__)
   max_width = width - 7;
   const __m128i b_mask = _mm_set1_epi16(0x001f);
   const __m128i g_mask = _mm_set1_epi16(0x07c0);
   const __m128i r_mask = _mm_set1_epi16(0xf800);
   const __m128i a_mask = _mm_set1_epi16(0x0020);
#elif defined(HOST_64)
   max_width = width - 3;
   const uint64_t b_mask = 0x001f001f001f001f;
   const uint64_t g_mask = 0x07c007c007c007c0;
   const uint64_t r_mask = 0xf800f800f800f800;
   const uint64_t a_mask = 0x0020002000200020;
#else
   max_width = width - 1;
   const uint64_t b_mask = 0x001f001f;
   const uint64_t g_mask = 0x07c007c0;
   const uint64_t r_mask = 0xf800f800;
   const uint64_t a_mask = 0x00200020;
#endif

   for (h = 0; h < height;
         h++, output += out_stride, input += in_stride)
   {
      int w = 0;
#if defined(__SSE2__)
      for (; w < max_width; w += 8)
      {
         const __m128i in = _mm_loadu_si128((const __m128i*)(input + w));
         __m128i r = _mm_and_si128(_mm_slli_epi16(in, 11), r_mask);
         __m128i g = _mm_and_si128(_mm_slli_epi16(in, 1 ), g_mask);
         __m128i b = _mm_and_si128(_mm_srli_epi16(in, 10), b_mask);
         __m128i a = _mm_and_si128(_mm_srli_epi16(in, 4 ), a_mask);
         _mm_storeu_si128((__m128i*)(output + w),
                          _mm_or_si128(r, _mm_or_si128(g, _mm_or_si128(b, a))));
      }
#elif defined(HOST_64)
      for (; w < max_width; w += 4)
      {
         const uint64_t in = *((uint64_t *)(input + w));
         uint64_t r = (in << 11) & r_mask;
         uint64_t g = (in << 1 ) & g_mask;
         uint64_t b = (in >> 10) & b_mask;
         uint64_t a = (in >> 4 ) & a_mask;
         *((uint64_t *)(output + w)) = r | g | b | a;
      }
#else
      for (; w < max_width; w += 2)
      {
         const uint32_t in = *((uint32_t *)(input + w));
         uint32_t r = (in << 11) & r_mask;
         uint32_t g = (in << 1 ) & g_mask;
         uint32_t b = (in >> 10) & b_mask;
         uint32_t a = (in >> 4 ) & a_mask;
         *((uint32_t *)(output + w)) = r | g | b | a;
      }
#endif

      for (; w < width; w++)
      {
         uint16_t col  = input[w];
         output[w]     = CONVERT_COLOR(col);
      }
   }
}

static void SwapScreen(uint16_t *dst, const uint16_t *src, uint32_t pitch)
{
   conv_0rgb1555_rb_swapped_rgb565(dst,
                                   src,
                                   GPU_LR_FRAMEBUFFER_NATIVE_WIDTH,
                                   GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT,
                                   pitch,
                                   GPU_LR_FRAMEBUFFER_NATIVE_WIDTH);
}

static void SwapScreen_32(uint32_t *dst, const uint32_t *src, uint32_t pitch)
{
   unsigned i;

   if (pitch == GPU_LR_FRAMEBUFFER_NATIVE_WIDTH)
   {
       memcpy (dst, src, GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * 4);
       return;
   }

   for(i = 0; i < GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT; i++)
   {
       memcpy (dst + i * pitch, src + i * GPU_LR_FRAMEBUFFER_NATIVE_WIDTH, GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * 4);
   }

}

static void SwapScreenLarge_32 (uint32_t *dst, const uint32_t *src, uint32_t pitch)
{
   unsigned y, x, z;

   for (y = 0; y < GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT; y++)
   {
        uint32_t *out = dst + (y * hybrid_layout_scale) * pitch;
        for (x = 0; x < GPU_LR_FRAMEBUFFER_NATIVE_WIDTH; x++)
        {
            for (z = 0; z < hybrid_layout_scale; z++)
                out[x * hybrid_layout_scale + z] = src[y * GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + x];
        }

        for (z = 1; z < hybrid_layout_scale; z++)
            memcpy (dst + ((y * hybrid_layout_scale) + z) * pitch, out, GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * hybrid_layout_scale * 4);
   }
}

static void SwapScreenSmall_32(uint32_t *dst, const uint32_t *src, uint32_t pitch, bool first, bool draw)
{
    unsigned x, y;

    if (!draw)
        return;

    if(!first)
    {
        int screenheight = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * hybrid_layout_scale / hybrid_layout_ratio;
        int gapheight    = gap_size() * hybrid_layout_scale * scale;
        // If it is the bottom screen, move the pointer down by a screen and the gap
        dst += (screenheight + gapheight) * pitch;
    }

    if (hybrid_layout_scale != hybrid_layout_ratio)
    {
        //Shrink to 1/3 the width and 1/3 the height
        for(y = 0; y < GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT / hybrid_layout_ratio; y++)
        {
            for(x = 0; x < GPU_LR_FRAMEBUFFER_NATIVE_WIDTH / hybrid_layout_ratio; x++)
            {
                *dst++ = src[hybrid_layout_ratio * (y * GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + x)];
            }
            dst += GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
        }
    }
    else
    {
        for (y = 0; y < GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT; y++)
        {
            memcpy (dst, src + y * GPU_LR_FRAMEBUFFER_NATIVE_WIDTH, (pitch - GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * hybrid_layout_ratio) * 4);
            dst += pitch;
        }
    }
}

static void SwapScreenLarge(uint16_t *dst, const uint16_t *src, uint32_t pitch)
{
    unsigned y, x, z;

    for (y = 0; y < GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT; y++)
    {
         uint16_t *out = dst + (y * hybrid_layout_scale) * pitch;
         for (x = 0; x < GPU_LR_FRAMEBUFFER_NATIVE_WIDTH; x++)
         {
             for (z = 0; z < hybrid_layout_scale; z++)
                 out[x * hybrid_layout_scale + z] = CONVERT_COLOR(src[y * GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + x]);
         }

         for (z = 1; z < hybrid_layout_scale; z++)
             memcpy (dst + ((y * hybrid_layout_scale) + z) * pitch, out, GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * hybrid_layout_scale * 2);
    }
}

static void SwapScreenSmall(uint16_t *dst, const uint16_t *src, uint32_t pitch, bool first, bool draw)
{
    unsigned x, y;

    if (!draw)
        return;

    if(!first)
    {
        int screenheight = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * hybrid_layout_scale / hybrid_layout_ratio;
        int gapheight    = gap_size() * hybrid_layout_scale * scale;
        // If it is the bottom screen, move the pointer down by a screen and the gap
        dst += (screenheight + gapheight) * pitch;
    }

    if (hybrid_layout_scale != hybrid_layout_ratio)
    {
        //Shrink to 1/3 the width and 1/3 the height
        for(y = 0; y < GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT / hybrid_layout_ratio; y++)
        {
            for(x = 0; x < GPU_LR_FRAMEBUFFER_NATIVE_WIDTH / hybrid_layout_ratio; x++)
            {
                *dst++ = CONVERT_COLOR(src[hybrid_layout_ratio * (y * GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + x)]);
            }
            dst += GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
        }
    }
    else
    {
        conv_0rgb1555_rb_swapped_rgb565(dst,
                                        src,
                                        (pitch - GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * hybrid_layout_ratio),
                                        GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT,
                                        pitch,
                                        GPU_LR_FRAMEBUFFER_NATIVE_WIDTH);
    }
}

namespace
{
    uint32_t firmwareLanguage;
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "DeSmuME";
#ifdef GIT_VERSION
   info->library_version = "git" GIT_VERSION;
#else
   info->library_version = "SVN";
#endif
   info->valid_extensions = "nds|bin";
   info->need_fullpath = true;
   info->block_extract = false;
}

static void update_layout_screen_buffers(LayoutData *layout)
{
    if (screen_buf == NULL || screen_buf_byte_size != layout->byte_size)
    {
        if (screen_buf)
            free(screen_buf);

        screen_buf = (uint16_t *) malloc(layout->byte_size);
        screen_buf_byte_size = layout->byte_size;
        memset(screen_buf, 0, screen_buf_byte_size);
    }

    layout->dst  = (uint16_t *)(((uint8_t *) screen_buf) + layout->offset1);
    layout->dst2 = (uint16_t *)(((uint8_t *) screen_buf) + layout->offset2);
}

static void update_layout_params(unsigned id, LayoutData *layout)
{
   int awidth, bwidth;

   /* Helper variables */
   int bytewidth  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * bpp;
   int byteheight = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
   int gapwidth   = gap_size() * bpp * scale;
   int gapsize    = gap_size() * scale;

   if (!layout)
      return;

   switch (id)
   {
      case LAYOUT_TOP_BOTTOM:
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * 2 + gapsize;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_x= 0;
         layout->touch_y= GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT + gapsize;

         layout->draw_screen1  = true;
         layout->draw_screen2  = true;

         layout->offset1 = 0;
         layout->offset2 = bytewidth * (byteheight + gapsize);
         break;

      case LAYOUT_BOTTOM_TOP:
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * 2 + gapsize;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_x= 0;
         layout->touch_y= 0;

         layout->draw_screen1  = true;
         layout->draw_screen2  = true;

         layout->offset1 = bytewidth * (byteheight + gapsize);
         layout->offset2 = 0;
         break;

      case LAYOUT_LEFT_RIGHT:
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * 2 + gapsize;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * 2 + gapsize;
         layout->touch_x= GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_y= 0;

         layout->draw_screen1  = true;
         layout->draw_screen2  = true;

         layout->offset1 = 0;
         layout->offset2 = bytewidth + gapwidth;
         break;

      case LAYOUT_RIGHT_LEFT:
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * 2 + gapsize;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * 2 + gapsize;
         layout->touch_x= 0;
         layout->touch_y= 0;

         layout->draw_screen1  = true;
         layout->draw_screen2  = true;

         layout->offset1 = bytewidth + gapwidth;
         layout->offset2 = 0;
         break;

      case LAYOUT_HYBRID_TOP_ONLY:
      case LAYOUT_HYBRID_BOTTOM_ONLY:
         awidth = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH / hybrid_layout_ratio;
         bwidth = awidth * bpp;

         layout->width  = hybrid_layout_scale * (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + awidth);
         layout->height = hybrid_layout_scale * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = hybrid_layout_scale * (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + awidth);

         if (id == LAYOUT_HYBRID_TOP_ONLY)
         {
            layout->touch_x = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * hybrid_layout_scale;
            layout->touch_y = (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT + gapsize) * hybrid_layout_scale / 2;
            layout->draw_screen1 = true;
            layout->draw_screen2 = false;
         }
         else
         {
             layout->touch_x = 0;
             layout->touch_y = 0;

             layout->draw_screen1 = false;
             layout->draw_screen2 = true;
         }

         layout->offset1 = 0;
         {
            size_t out = 0; // Start pointer
            out += bytewidth * hybrid_layout_scale; // Move pointer to right by large screen width
            int pitch = layout->pitch * bpp; // byte size of a line
            int halfscreen = layout->height / 2; // y offset: midpoint of the screen height
            halfscreen -= (gap_size() * scale * hybrid_layout_scale) / 2; // move upward by half the gap height
            halfscreen -= GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * hybrid_layout_scale / hybrid_layout_ratio; // move y offset framebuffer height upward
            out += pitch * halfscreen; // add this offset to pointer
            layout->offset2 = out;
         }

         if (id == LAYOUT_HYBRID_BOTTOM_ONLY)
         {
             size_t swap;
             swap = layout->offset1;
             layout->offset1 = layout->offset2;
             layout->offset2 = swap;
         }

         break;

      case LAYOUT_TOP_ONLY:
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_x= 0;
         layout->touch_y= GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;

         layout->draw_screen1 = true;
         layout->draw_screen2 = false;

         layout->offset1 = 0;
         layout->offset2 = bytewidth * byteheight;
         break;

      case LAYOUT_BOTTOM_ONLY:
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_x= 0;
         layout->touch_y= 0;

         layout->draw_screen2 = true;
         layout->draw_screen1 = false;

         layout->offset1 = bytewidth * byteheight;
         layout->offset2 = 0;

         break;
   }

   layout->byte_size = layout->width * layout->height * bpp;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   struct LayoutData layout;
   update_layout_params(current_layout, &layout);

   switch (current_layout)
   {
          case LAYOUT_TOP_BOTTOM:
          case LAYOUT_BOTTOM_TOP:
		  info->geometry.base_width   = 256;
		  info->geometry.base_height  = 384;
		  break;
          case LAYOUT_LEFT_RIGHT:
          case LAYOUT_RIGHT_LEFT:
		  info->geometry.base_width   = 512;
		  info->geometry.base_height  = 192;
		  break;
	  case LAYOUT_TOP_ONLY:
	  case LAYOUT_BOTTOM_ONLY:
		  info->geometry.base_width   = 256;
		  info->geometry.base_height  = 192;
		  break;
	  case LAYOUT_HYBRID_TOP_ONLY:
	  case LAYOUT_HYBRID_BOTTOM_ONLY:
		  info->geometry.base_width   = 341;
		  info->geometry.base_height  = 192;
		  break;
   }
   info->geometry.max_width    = layout.width;
   info->geometry.max_height   = layout.height;
   info->geometry.aspect_ratio = 0.0;
   info->timing.fps            = 59.8261;
   info->timing.sample_rate    = 44100.0;
}


static void MicrophoneToggle(void)
{
   if (NDS_getFinalUserInput().mic.micButtonPressed)
      NDS_setMic(false);
   else
      NDS_setMic(true);
}

static void check_variables(bool first_boot)
{
   struct retro_variable var = {0};
   bool need_framebuffer_reset = false;

   if (first_boot)
   {
      var.key = "desmume_cpu_mode";
      var.value = 0;

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
          if (!strcmp(var.value, "jit"))
              CommonSettings.use_jit = true;
          else if (!strcmp(var.value, "interpreter"))
              CommonSettings.use_jit = false;
      }
     else
     {
#ifdef HAVE_JIT
        CommonSettings.use_jit = true;
#else
        CommonSettings.use_jit = false;
#endif
     }

#ifdef HAVE_JIT
      var.key = "desmume_jit_block_size";

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
          CommonSettings.jit_max_block_size = var.value ? strtol(var.value, 0, 10) : 100;
      else
          CommonSettings.jit_max_block_size = 100;
#endif

      var.key = "desmume_use_external_bios";

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
          const char *system_directory = NULL;

          if (!strcmp(var.value, "enabled"))
          {
              if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_directory) && system_directory)
              {
                  std::string bios7_loc    = std::string(system_directory) + DIRECTORY_DELIMITER_CHAR + "bios7.bin";
                  std::string bios9_loc    = std::string(system_directory) + DIRECTORY_DELIMITER_CHAR + "bios9.bin";
                  std::string firmware_loc = std::string(system_directory) + DIRECTORY_DELIMITER_CHAR + "firmware.bin";

                  strncpy(CommonSettings.ARM7BIOS, bios7_loc.c_str(), 256);
                  strncpy(CommonSettings.ARM9BIOS, bios9_loc.c_str(), 256);
                  strncpy(CommonSettings.ExtFirmwarePath, firmware_loc.c_str(), 256);

                  CommonSettings.ARM7BIOS[255] = '\0';
                  CommonSettings.ARM9BIOS[255] = '\0';
                  CommonSettings.ExtFirmwarePath[255] = '\0';
              }

              CommonSettings.UseExtBIOS = true;
              CommonSettings.UseExtFirmware = true;
              CommonSettings.SWIFromBIOS = true;
              CommonSettings.PatchSWI3 = true;
          }
      }

      var.key = "desmume_boot_into_bios";
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
          if (!strcmp(var.value, "enabled"))
          {
              if (CommonSettings.UseExtBIOS && !CommonSettings.use_jit)
              {
                  CommonSettings.BootFromFirmware = true;
                  CommonSettings.UseExtFirmwareSettings = true;
              }
              else
              {
                  log_cb (RETRO_LOG_WARN, "Cannot boot into BIOS. Must enable external bios and interpreter mode.\n");
              }
          }
      }

      var.key = "desmume_opengl_mode";

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         if (!strcmp(var.value, "enabled"))
            opengl_mode = true;
         else if (!strcmp(var.value, "disabled"))
            opengl_mode = false;
      }
      else
         opengl_mode = false;

      var.key = "desmume_color_depth";
      if (opengl_mode && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
          if (!strcmp(var.value, "32-bit"))
          {
              colorMode = RETRO_PIXEL_FORMAT_XRGB8888;
              bpp = 4;
#ifdef HAVE_OPENGL
              internal_format = GL_RGBA;
              texture_type = GL_RGBA;
              texture_format = GL_UNSIGNED_BYTE;
#endif
          }
          else
          {
              colorMode = RETRO_PIXEL_FORMAT_RGB565;
              bpp = 2;
#ifdef HAVE_OPENGL
              internal_format = GL_RGB565;
              texture_type = GL_RGB;
              texture_format = GL_UNSIGNED_SHORT_5_6_5;
#endif
          }
      }
   }

   var.key = "desmume_internal_resolution";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      char *pch;
      char str[100];
      snprintf(str, sizeof(str), "%s", var.value);

      pch = strtok(str, "x");
      if (pch)
         GPU_LR_FRAMEBUFFER_NATIVE_WIDTH = strtoul(pch, NULL, 0);
      pch = strtok(NULL, "x");
      if (pch)
         GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT = strtoul(pch, NULL, 0);

      switch (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH)
      {
         case 256:
            scale = 1;
            break;
         case 512:
            scale = 2;
            break;
         case 768:
            scale = 3;
            break;
         case 1024:
            scale = 4;
            break;
         case 1280:
            scale = 5;
            break;
         case 1536:
            scale = 6;
            break;
         case 1792:
            scale = 7;
            break;
         case 2048:
            scale = 8;
            break;
         case 2304:
            scale = 9;
            break;
         case 2560:
            scale = 10;
            break;
      }

      if (!first_boot && GPU->GetCustomFramebufferWidth() != GPU_LR_FRAMEBUFFER_NATIVE_WIDTH)
         need_framebuffer_reset = true;

   }

   var.key = "desmume_num_cores";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
       CommonSettings.num_cores = var.value ? strtol(var.value, 0, 10) : 1;
   else
       CommonSettings.num_cores = 1;

    var.key = "desmume_screens_layout";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
       static int old_layout_id      = -1;
       unsigned new_layout_id        = 0;

       if (!strcmp(var.value, "top/bottom"))
          new_layout_id = LAYOUT_TOP_BOTTOM;
       else if (!strcmp(var.value, "bottom/top"))
          new_layout_id = LAYOUT_BOTTOM_TOP;
       else if (!strcmp(var.value, "left/right"))
          new_layout_id = LAYOUT_LEFT_RIGHT;
       else if (!strcmp(var.value, "right/left"))
          new_layout_id = LAYOUT_RIGHT_LEFT;
       else if (!strcmp(var.value, "top only"))
           new_layout_id = LAYOUT_TOP_ONLY;
       else if (!strcmp(var.value, "bottom only"))
           new_layout_id = LAYOUT_BOTTOM_ONLY;
       else if(!strcmp(var.value, "hybrid/top"))
           new_layout_id = LAYOUT_HYBRID_TOP_ONLY;
       else if(!strcmp(var.value, "hybrid/bottom"))
           new_layout_id = LAYOUT_HYBRID_BOTTOM_ONLY;

       if (old_layout_id != new_layout_id)
       {
          old_layout_id = new_layout_id;
          current_layout = new_layout_id;
       }
    }

    var.key = "desmume_hybrid_layout_ratio";
    hybrid_layout_ratio = 3;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (!strcmp(var.value, "2:1"))
            hybrid_layout_ratio = 2;
    }

    var.key = "desmume_hybrid_layout_scale";
    hybrid_layout_scale = 1;
    if (scale < hybrid_layout_ratio && environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (!strcmp(var.value, "enabled"))
            hybrid_layout_scale = hybrid_layout_ratio;
    }

    var.key = "desmume_pointer_mouse";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (!strcmp(var.value, "enabled"))
            mouse_enable = true;
      else if (!strcmp(var.value, "disabled"))
            mouse_enable = false;
    }
   else
      mouse_enable = false;

    var.key = "desmume_pointer_device_l";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (!strcmp(var.value, "emulated"))
            pointer_device_l = 1;
        else if(!strcmp(var.value, "absolute"))
            pointer_device_l = 2;
        else if (!strcmp(var.value, "pressed"))
            pointer_device_l = 3;
        else
            pointer_device_l=0;
    }
    else
        pointer_device_l=0;

    var.key = "desmume_pointer_device_r";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (!strcmp(var.value, "emulated"))
            pointer_device_r = 1;
        else if(!strcmp(var.value, "absolute"))
            pointer_device_r = 2;
        else if (!strcmp(var.value, "pressed"))
            pointer_device_r = 3;
        else
            pointer_device_r=0;
    }
    else
        pointer_device_r=0;

    var.key = "desmume_pointer_device_deadzone";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      analog_stick_deadzone = (int)(atoi(var.value));

    var.key = "desmume_pointer_type";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        touchEnabled = var.value && (!strcmp(var.value, "touch"));
    }

    var.key = "desmume_mouse_speed";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        mouse_speed = (float) atof(var.value);
    }
    else
        mouse_speed = 1.0f;

    var.key = "desmume_input_rotation";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        input_rotation = atoi(var.value);
    }
    else
        input_rotation = 0;

    var.key = "desmume_frameskip";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        frameSkip = var.value ? strtol(var.value, 0, 10) : 0;
   else
      frameSkip = 0;

    var.key = "desmume_firmware_language";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        static const struct { const char* name; int id; } languages[] =
        {
            { "Auto", -1 },
            { "Japanese", 0 },
            { "English", 1 },
            { "French", 2 },
            { "German", 3 },
            { "Italian", 4 },
            { "Spanish", 5 }
        };

        for (int i = 0; i < 7; i ++)
        {
            if (!strcmp(languages[i].name, var.value))
            {
                firmwareLanguage = languages[i].id;
                if (firmwareLanguage == -1) firmwareLanguage = host_get_language();
                break;
            }
        }
    }
   else
      firmwareLanguage = 1;

   var.key = "desmume_opengl_shadow_polygon";
   CommonSettings.OpenGL_Emulation_ShadowPolygon = true;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         CommonSettings.OpenGL_Emulation_ShadowPolygon = false;
   }

   var.key = "desmume_opengl_special_zero_alpha";
   CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending = true;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         CommonSettings.OpenGL_Emulation_SpecialZeroAlphaBlending = false;
   }

   var.key = "desmume_opengl_nds_depth_calculation";
   CommonSettings.OpenGL_Emulation_NDSDepthCalculation = true;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
         CommonSettings.OpenGL_Emulation_NDSDepthCalculation = false;
   }

   var.key = "desmume_opengl_depth_lequal_polygon_facing";
   CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.OpenGL_Emulation_DepthLEqualPolygonFacing = true;
   }

   var.key = "desmume_gfx_texture_smoothing";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
        if (!strcmp(var.value, "enabled"))
         CommonSettings.GFX3D_Renderer_TextureSmoothing = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.GFX3D_Renderer_TextureSmoothing = false;
   }
   else
      CommonSettings.GFX3D_Renderer_TextureSmoothing = false;

   var.key = "desmume_gfx_multisampling";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "disabled"))
      {
         CommonSettings.GFX3D_Renderer_MultisampleSize = 1;
      }
      else
      {
         int newvalue = atoi(var.value);
         CommonSettings.GFX3D_Renderer_MultisampleSize = newvalue;;
      }
   }
   else
      CommonSettings.GFX3D_Renderer_MultisampleSize = 1;

   var.key = "desmume_gfx_highres_interpolate_color";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.GFX3D_HighResolutionInterpolateColor = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.GFX3D_HighResolutionInterpolateColor = false;
   }
   else
      CommonSettings.GFX3D_HighResolutionInterpolateColor = false;

   var.key = "desmume_gfx_texture_deposterize";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.GFX3D_Renderer_TextureDeposterize = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.GFX3D_Renderer_TextureDeposterize = false;
   }
   else
      CommonSettings.GFX3D_Renderer_TextureDeposterize = false;

   var.key = "desmume_gfx_texture_scaling";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      CommonSettings.GFX3D_Renderer_TextureScalingFactor = atoi(var.value);
   }
   else
      CommonSettings.GFX3D_Renderer_TextureScalingFactor = 1;

   var.key = "desmume_gfx_edgemark";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.GFX3D_EdgeMark = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.GFX3D_EdgeMark = false;
   }
   else
      CommonSettings.GFX3D_EdgeMark = true;

   var.key = "desmume_gfx_linehack";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.GFX3D_LineHack = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.GFX3D_LineHack = false;
   }
   else
      CommonSettings.GFX3D_LineHack = true;

   var.key = "desmume_gfx_txthack";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.GFX3D_TXTHack = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.GFX3D_TXTHack = false;
   }
   else
      CommonSettings.GFX3D_TXTHack = false;

   var.key = "desmume_mic_mode";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "pattern"))
         CommonSettings.micMode = TCommonSettings::InternalNoise;
      else if(!strcmp(var.value, "sample"))
         CommonSettings.micMode = TCommonSettings::Sample;
      else if(!strcmp(var.value, "random"))
         CommonSettings.micMode = TCommonSettings::Random;
      else if(!strcmp(var.value, "physical"))
         CommonSettings.micMode = TCommonSettings::Physical;
   }
   else
      CommonSettings.micMode = TCommonSettings::InternalNoise;

   var.key = "desmume_pointer_device_acceleration_mod";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      analog_stick_acceleration_modifier = atoi(var.value);
   else
      analog_stick_acceleration_modifier = 0;

   var.key = "desmume_pointer_stylus_pressure";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      CommonSettings.StylusPressure = atoi(var.value);
   else
      CommonSettings.StylusPressure = 50;

   var.key = "desmume_load_to_memory";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.loadToMemory = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.loadToMemory = false;
   }
   else
      CommonSettings.loadToMemory = false;

   var.key = "desmume_advanced_timing";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.advanced_timing = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.advanced_timing = false;
   }
   else
      CommonSettings.advanced_timing = true;

   var.key = "desmume_screens_gap";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if ((atoi(var.value)) != nds_screen_gap)
      {
         nds_screen_gap = atoi(var.value);
         if (nds_screen_gap > NDS_MAX_SCREEN_GAP)
            nds_screen_gap = NDS_MAX_SCREEN_GAP;
      }
   }

  var.key = "desmume_hybrid_showboth_screens";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         hybrid_layout_showbothscreens = true;
      else if(!strcmp(var.value, "disabled"))
         hybrid_layout_showbothscreens = false;
   }
   else
      hybrid_layout_showbothscreens = true;

    var.key = "desmume_hybrid_cursor_always_smallscreen";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         hybrid_cursor_always_smallscreen = true;
      else if(!strcmp(var.value, "disabled"))
         hybrid_cursor_always_smallscreen = false;
   }
   else
      hybrid_cursor_always_smallscreen = true;


   var.key = "desmume_pointer_colour";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
       if(!strcmp(var.value, "white"))
       {
           pointer_colour = 0xFFFF;
           pointer_color_32 = 0xFFFFFFFF;
       }
       else if (!strcmp(var.value, "black"))
       {
           pointer_colour = 0x0000;
           pointer_color_32 = 0x00000000;
       }
       else if(!strcmp(var.value, "red"))
       {
           pointer_colour = 0xF800;
           pointer_color_32 = 0xFF0000FF;
       }
       else if(!strcmp(var.value, "yellow"))
       {
           pointer_colour = 0xFFE0;
           pointer_color_32 = 0x0000FFFF;
       }
       else if(!strcmp(var.value, "blue"))
       {
           pointer_colour = 0x001F;
           pointer_color_32 = 0xFFFF0000;
       }
       else
       {
           pointer_colour = 0xFFFF;
           pointer_color_32 = 0xFFFFFFFF;
       }
   }
   else
   {
       pointer_colour = 0xFFFF;
       pointer_color_32 = 0xFFFFFFFF;
   }

   if (need_framebuffer_reset)
   {
      GPU->SetCustomFramebufferSize(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH, GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
   }
}

#define GPU3D_NULL           0
#define GPU3D_SOFTRASTERIZER 1
#define GPU3D_OPENGL_AUTO    2

GPU3DInterface* core3DList[] =
{
   &gpu3DNull,
   &gpu3DRasterize,
#ifdef HAVE_OPENGL
   &gpu3Dgl,
   &gpu3DglOld,
   &gpu3Dgl_3_2,
#endif
    NULL
};

int SNDRetroInit(int buffersize) { return 0; }
void SNDRetroDeInit() {}
u32 SNDRetroGetAudioSpace() { return 0; }
void SNDRetroMuteAudio() {}
void SNDRetroUnMuteAudio() {}
void SNDRetroSetVolume(int volume) {}
void SNDRetroUpdateAudio(s16 *buffer, u32 num_samples) {}
void SNDRetroFetchSamples(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
    audio_batch_cb(sampleBuffer, sampleCount);
}

SoundInterface_struct SNDRetro = {
    0,
    "libretro Sound Interface",
    SNDRetroInit,
    SNDRetroDeInit,
    SNDRetroUpdateAudio,
    SNDRetroGetAudioSpace,
    SNDRetroMuteAudio,
    SNDRetroUnMuteAudio,
    SNDRetroSetVolume,
    NULL,
    SNDRetroFetchSamples,
    NULL
};

SoundInterface_struct *SNDCoreList[] = {
    &SNDRetro
};

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)   { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }

void retro_set_environment(retro_environment_t cb)
{
   struct retro_vfs_interface_info vfs_iface_info;
   environ_cb = cb;

   static const retro_variable values[] =
   {
      { "desmume_firmware_language", "Firmware Language; Auto|English|Japanese|French|German|Italian|Spanish" },
      { "desmume_use_external_bios", "Use External BIOS/Firmware (restart); disabled|enabled" },
      { "desmume_boot_into_bios", "Boot Into BIOS (interpreter and external bios only); disabled|enabled"},
      { "desmume_load_to_memory", "Load Game Into Memory (restart); disabled|enabled" },
      { "desmume_num_cores", "CPU Cores; 1|2|3|4" },
#ifdef HAVE_JIT
#if defined(IOS) || defined(ANDROID)
      { "desmume_cpu_mode", "CPU Mode; interpreter|jit" },
#else
      { "desmume_cpu_mode", "CPU Mode (restart); jit|interpreter" },
#endif
      { "desmume_jit_block_size", "JIT Block Size; 12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|0|1|2|3|4|5|6|7|8|9|10|11" },
#else
      { "desmume_cpu_mode", "CPU Mode; interpreter" },
#endif
      { "desmume_advanced_timing", "Enable Advanced Bus-Level Timing; enabled|disabled" },
      { "desmume_frameskip", "Frameskip; 0|1|2|3|4|5|6|7|8|9" },
      { "desmume_internal_resolution", "Internal Resolution; 256x192|512x384|768x576|1024x768|1280x960|1536x1152|1792x1344|2048x1536|2304x1728|2560x1920" },
#ifdef HAVE_OPENGL
      { "desmume_opengl_mode", "OpenGL Rasterizer (restart); disabled|enabled" },
      { "desmume_color_depth", "OpenGL: Color Depth (restart); 16-bit|32-bit"},
      { "desmume_gfx_multisampling", "OpenGL: Multisampling AA; disabled|2|4|8|16|32" },
      { "desmume_gfx_texture_smoothing", "OpenGL: Texture Smoothing; disabled|enabled" },
      { "desmume_opengl_shadow_polygon", "OpenGL: Shadow Polygons; enabled|disabled" },
      { "desmume_opengl_special_zero_alpha", "OpenGL: Special 0 Alpha; enabled|disabled" },
      { "desmume_opengl_nds_depth_calculation", "OpenGL: NDS Depth Calculation; enabled|disabled" },
      { "desmume_opengl_depth_lequal_polygon_facing", "OpenGL: Depth-LEqual Polygon Facing; disabled|enabled" },
#endif
      { "desmume_gfx_highres_interpolate_color", "Soft3D: High-res Color Interpolation; disabled|enabled" },
      { "desmume_gfx_linehack", "Soft3D: Line Hack; enabled|disabled" },
      { "desmume_gfx_txthack", "Soft3D: Texture Hack; disabled|enabled"},
      { "desmume_gfx_edgemark", "Edge Marking; enabled|disabled" },
      { "desmume_gfx_texture_scaling", "Texture Scaling (xBrz); 1|2|4" },
      { "desmume_gfx_texture_deposterize", "Texture Deposterization; disabled|enabled" },
      { "desmume_screens_layout", "Screen Layout; top/bottom|bottom/top|left/right|right/left|top only|bottom only|hybrid/top|hybrid/bottom" },
      { "desmume_screens_gap", "Screen Gap; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100" },
      { "desmume_hybrid_layout_ratio", "Hybrid Layout: Ratio; 3:1|2:1" },
      { "desmume_hybrid_layout_scale", "Hybrid Layout: Scale So Small Screen is 1:1px; disabled|enabled" },
      { "desmume_hybrid_showboth_screens", "Hybrid Layout: Show Both Screens; enabled|disabled"},
      { "desmume_hybrid_cursor_always_smallscreen", "Hybrid Layout: Cursor Always on Small Screen; enabled|disabled"},
      { "desmume_pointer_mouse", "Mouse/Pointer; enabled|disabled" },
      { "desmume_pointer_type", "Pointer Type; mouse|touch" },
      { "desmume_mouse_speed", "Mouse Speed; 1.0|1.5|2.0|0.01|0.02|0.03|0.04|0.05|0.125|0.25|0.5" },
      { "desmume_input_rotation", "Pointer Rotation; 0|90|180|270" },
      { "desmume_pointer_device_l", "Pointer Mode for Left Analog; none|emulated|absolute|pressed" },
      { "desmume_pointer_device_r", "Pointer Mode for Right Analog; none|emulated|absolute|pressed" },
      { "desmume_pointer_device_deadzone", "Emulated Pointer Deadzone Percent; 15|20|25|30|35|0|5|10" },
      { "desmume_pointer_device_acceleration_mod", "Emulated Pointer Acceleration Modifier Percent; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100" },
      { "desmume_pointer_stylus_pressure", "Emulated Stylus Pressure Modifier Percent; 50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|" },
      { "desmume_pointer_colour", "Pointer Colour; white|black|red|blue|yellow"},
      { "desmume_mic_mode", "Microphone Button Noise Type; pattern|random" },
      { 0, 0 }
   };

   environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)values);

   vfs_iface_info.required_interface_version = FILESTREAM_REQUIRED_VFS_VERSION;
   vfs_iface_info.iface                      = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
	   filestream_vfs_init(&vfs_iface_info);
}


//====================== Message box
#define MSG_ARG \
    char msg_buf[1024] = {0}; \
    { \
        va_list args; \
        va_start (args, fmt); \
        vsprintf (msg_buf, fmt, args); \
        va_end (args); \
    }

void msgWndInfo(const char *fmt, ...)
{
    MSG_ARG;
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s.\n", msg_buf);
}

bool msgWndConfirm(const char *fmt, ...)
{
    MSG_ARG;
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s.\n", msg_buf);
   return true;
}

void msgWndError(const char *fmt, ...)
{
    MSG_ARG;
   if (log_cb)
      log_cb(RETRO_LOG_ERROR, "%s.\n", msg_buf);
}

void msgWndWarn(const char *fmt, ...)
{
    MSG_ARG;
   if (log_cb)
      log_cb(RETRO_LOG_WARN, "%s.\n", msg_buf);
}

msgBoxInterface msgBoxWnd = {
    msgWndInfo,
    msgWndConfirm,
    msgWndError,
    msgWndWarn,
};
//====================== Dialogs end

static void check_system_specs(void)
{
   unsigned level = 15;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

static bool dummy_retro_gl_init() { return true; }
static void dummy_retro_gl_end() {}
static bool dummy_retro_gl_begin() { return true; }

static bool context_needs_reinit = false;

#ifdef HAVE_OPENGL
static bool initialize_gl()
{
    OGLLoadEntryPoints_3_2_Func = OGLLoadEntryPoints_3_2;
    OGLCreateRenderer_3_2_Func = OGLCreateRenderer_3_2;

    if (!NDS_3D_ChangeCore(GPU3D_OPENGL_AUTO))
    {
        log_cb(RETRO_LOG_WARN, "Failed to change to OpenGL core!\n");
        opengl_mode = false;
        NDS_3D_ChangeCore(GPU3D_SOFTRASTERIZER);
        return false;
    }
    glBindFramebuffer = (glBindFramebufferProc *) hw_render.get_proc_address ("glBindFramebuffer");
    glGenFramebuffers = (glGenFramebuffersProc *) hw_render.get_proc_address ("glGenFramebuffers");
    glDeleteFramebuffers = (glDeleteFramebuffersProc *) hw_render.get_proc_address ("glDeleteFramebuffers");
    glFramebufferTexture2D = (glFramebufferTexture2DProc *) hw_render.get_proc_address ("glFramebufferTexture2D");
    glBlitFramebuffer = (glBlitFramebufferProc *) hw_render.get_proc_address ("glBlitFramebuffer");
    glMapBufferRange = (glMapBufferRangeProc *) hw_render.get_proc_address ("glMapBufferRange");

    if (!glBindFramebuffer || !glGenFramebuffers || !glDeleteFramebuffers || !glFramebufferTexture2D || !glBlitFramebuffer)
    {
        log_cb(RETRO_LOG_WARN, "Don't have required OpenGL functions.\n");
        opengl_mode = false;
        NDS_3D_ChangeCore(GPU3D_SOFTRASTERIZER);
        return false;
    }

    return true;
}
#endif

static void context_destroy()
{
   NDS_3D_ChangeCore(GPU3D_NULL);
#ifdef HAVE_OPENGL
   pbo = fbo = tex = current_texture_width = current_texture_height = 0;
#endif

   context_needs_reinit = true;
}

static void context_reset() {
   if (!context_needs_reinit)
      return;

#ifdef HAVE_OPENGL
   initialize_gl();
#endif

   context_needs_reinit = false;
}


void retro_init (void)
{
   struct retro_log_callback log;
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

    check_variables(true);

    // Init DeSmuME
    NDS_SetupDefaultFirmware();
    CommonSettings.fwConfig.language = firmwareLanguage;

    //addonsChangePak(NDS_ADDON_NONE);

    const char *nickname;
    if (environ_cb(RETRO_ENVIRONMENT_GET_USERNAME, &nickname) && nickname)
    {
        int len = strlen(nickname);

        if (len > MAX_FW_NICKNAME_LENGTH)
            len = MAX_FW_NICKNAME_LENGTH;

        if (len > 0)
        {
            for (int i = 0; i < len; i++)
                CommonSettings.fwConfig.nickname[i] = nickname[i];
            CommonSettings.fwConfig.nicknameLength = len;
        }
    }

    NDS_Init();
    SPU_ChangeSoundCore(0, 0);
    SPU_SetSynchMode(ESynchMode_Synchronous, ESynchMethod_N);

    NDS_3D_ChangeCore(GPU3D_SOFTRASTERIZER);
    GPU->SetCustomFramebufferSize(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH, GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);

    log_cb(RETRO_LOG_INFO, "Setting %s color depth.\n", colorMode == RETRO_PIXEL_FORMAT_XRGB8888 ? "32-bit" : "16-bit");
    if(!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &colorMode))
       return;

    if (colorMode == RETRO_PIXEL_FORMAT_XRGB8888)
        GPU->SetColorFormat(NDSColorFormat_BGR888_Rev);
    else
        GPU->SetColorFormat(NDSColorFormat_BGR555_Rev);

    backup_setManualBackupType(MC_TYPE_AUTODETECT);

    msgbox = &msgBoxWnd;
   check_system_specs();

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;
}

void retro_deinit(void)
{
#ifdef HAVE_OPENGL
    if (pbo)
    {
       glDeleteBuffers(1, &pbo);
       pbo = 0;
    }
    if (fbo)
    {
       glDeleteFramebuffers(1, &fbo);
       fbo = 0;
    }
    if (tex)
    {
       glDeleteTextures(1, &tex);
       tex = 0;
    }
#endif
    NDS_DeInit();

#ifdef PERF_TEST
   rarch_perf_log();
#endif
   libretro_supports_bitmasks = false;
}

void retro_reset (void)
{
    NDS_Reset();
}

void rotate_input(int16_t &x, int16_t &y, int rotation)
{
    uint16_t tmp;

    switch (rotation)
    {
       case 270:
          tmp = x;
          x = y;
          y = -tmp;
          break;
       case 180:
          x = -x;
          y = -y;
          break;
       case 90:
          tmp = x;
          x = -y;
          y = tmp;
          break;
       default:
          break;
    }
}

void retro_run (void)
{
   struct LayoutData layout;
   int16_t l_analog_x_ret        = 0;
   int16_t l_analog_y_ret        = 0;
   int16_t r_analog_x_ret        = 0;
   int16_t r_analog_y_ret        = 0;
   int16_t ret                   = 0;
   bool updated                  = false;
   bool have_touch               = false;

#ifdef HAVE_OPENGL
   static bool gl_initialized = false;

   if (!gl_initialized && opengl_mode)
   {
          gl_initialized = initialize_gl();
   }
#endif

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
   {
      check_variables(false);
      struct retro_system_av_info new_av_info;
      retro_get_system_av_info(&new_av_info);

      /* Reinitialize the video if the layout is significantly different than previously set.
         This makes image uploads more efficient */
      if (current_max_width != 0 &&
          (current_max_width      < new_av_info.geometry.max_width  ||
           current_max_height     < new_av_info.geometry.max_height ||
           current_max_width / 2  > new_av_info.geometry.max_width  ||
           current_max_height / 2 > new_av_info.geometry.max_height))
      {
         log_cb (RETRO_LOG_INFO, "Screen size changed significantly. Reinitializing.\n");
         current_max_width = new_av_info.geometry.max_width;
         current_max_height = new_av_info.geometry.max_height;
         environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &new_av_info);
      }
      else
      {
         environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &new_av_info);
      }
   }

   update_layout_params(current_layout, &layout);
   update_layout_screen_buffers(&layout);

   if (current_max_width == 0)
   {
       current_max_width = layout.width;
       current_max_height = layout.height;
   }

   poll_cb();

   if (libretro_supports_bitmasks)
      ret = input_cb(0, RETRO_DEVICE_JOYPAD,
            0, RETRO_DEVICE_ID_JOYPAD_MASK);
   else
   {
      unsigned i;
      for (i = 0; i < RETRO_DEVICE_ID_JOYPAD_R3+1; i++)
      {
         if (input_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
            ret |= (1 << i);
      }
   }
   
   l_analog_x_ret = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
   l_analog_y_ret = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
   r_analog_x_ret = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
   r_analog_y_ret = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

   if(pointer_device_l != 0 || pointer_device_r != 0)  // 1=emulated pointer, 2=absolute pointer, 3=absolute pointer constantly pressed
   {
        int16_t analogX_l = 0;
        int16_t analogY_l = 0;
        int16_t analogX_r = 0;
        int16_t analogY_r = 0;
        int16_t analogX = 0;
        int16_t analogY = 0;
        int16_t analogXpointer = 0;
        int16_t analogYpointer = 0;

        //emulated pointer on one or both sticks
        //just prioritize the stick that has a higher radius
        if((pointer_device_l == 1) || (pointer_device_r == 1))
        {
            double radius            = 0;
            double angle             = 0;
            float final_acceleration = analog_stick_acceleration * (1.0 + (float)analog_stick_acceleration_modifier / 100.0);

            if((pointer_device_l == 1) && (pointer_device_r == 1))
            {
                analogX_l = l_analog_x_ret /  final_acceleration;
                analogY_l = l_analog_y_ret / final_acceleration;
                rotate_input(analogX_l, analogY_l, input_rotation);
                analogX_r = r_analog_x_ret /  final_acceleration;
                analogY_r = r_analog_y_ret / final_acceleration;
                rotate_input(analogX_r, analogY_r, input_rotation);

                double radius_l = sqrt(analogX_l * analogX_l + analogY_l * analogY_l);
                double radius_r = sqrt(analogX_r * analogX_r + analogY_r * analogY_r);

                if(radius_l > radius_r)
                {
                    radius = radius_l;
                    angle = atan2(analogY_l, analogX_l);
                    analogX = analogX_l;
                    analogY = analogY_l;
                }
                else
                {
                    radius = radius_r;
                    angle = atan2(analogY_r, analogX_r);
                    analogX = analogX_r;
                    analogY = analogY_r;
                }
            }

            else if(pointer_device_l == 1)
            {
                analogX = l_analog_x_ret / final_acceleration;
                analogY = l_analog_y_ret / final_acceleration;
                rotate_input(analogX, analogY, input_rotation);
                radius = sqrt(analogX * analogX + analogY * analogY);
                angle = atan2(analogY, analogX);
            }
            else
            {
                analogX = r_analog_x_ret / final_acceleration;
                analogY = r_analog_y_ret / final_acceleration;
                rotate_input(analogX, analogY, input_rotation);
                radius = sqrt(analogX * analogX + analogY * analogY);
                angle = atan2(analogY, analogX);
            }

            // Convert cartesian coordinate analog stick to polar coordinates
            double max = (float)0x8000/analog_stick_acceleration;

            //log_cb(RETRO_LOG_DEBUG, "%d %d.\n", analogX,analogY);
            //log_cb(RETRO_LOG_DEBUG, "%d %d.\n", radius,analog_stick_deadzone);
            if(radius > (float)analog_stick_deadzone*max/100)
            {
                // Re-scale analog stick range to negate deadzone (makes slow movements possible)
                radius = (radius - (float)analog_stick_deadzone*max/100)*((float)max/(max - (float)analog_stick_deadzone*max/100));

                // Convert back to cartesian coordinates
                analogXpointer = (int32_t)round(radius * cos(angle));
                analogYpointer = (int32_t)round(radius * sin(angle));

                TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), TouchX + analogXpointer);
                TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), TouchY + analogYpointer);


            }

        }

        //absolute pointer -- doesn't run if emulated pointer > deadzone
        if(((pointer_device_l == 2) || (pointer_device_l == 3) || (pointer_device_r == 2) || (pointer_device_r == 3)) && !(analogXpointer || analogYpointer))
        {
            if(((pointer_device_l == 2) || (pointer_device_l == 3)) && ((pointer_device_r == 2) || (pointer_device_r == 3))) //both sticks set to absolute or pressed
            {

                if(pointer_device_l == 3) //left analog is always pressed
                {
                    int16_t analogXpress = l_analog_x_ret;
                    int16_t analogYpress = l_analog_y_ret;
                    rotate_input(analogXpress, analogYpress, input_rotation);

                    double radius = sqrt(analogXpress * analogXpress + analogYpress * analogYpress);

                    //check if analog exceeds deadzone
                    if (radius > (float)analog_stick_deadzone*0x8000/100)
                    {
                        have_touch = 1;

                        //scale analog position to ellipse enclosing framebuffer rectangle
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogXpress / (float)0x8000;
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogYpress / (float)0x8000;

                    }
                    else if (pointer_device_r == 2) //use the other stick as absolute
                    {
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2 * r_analog_x_ret / (float)0x8000;
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2 * r_analog_y_ret / (float)0x8000;
                        rotate_input(analogX, analogY, input_rotation);
                    }
                }

                else if(pointer_device_r == 3) // right analog is always pressed
                {
                    int16_t analogXpress = r_analog_x_ret;
                    int16_t analogYpress = r_analog_y_ret;
                    rotate_input(analogXpress, analogYpress, input_rotation);

                    double radius = sqrt(analogXpress * analogXpress + analogYpress * analogYpress);

                    if (radius > (float)analog_stick_deadzone*0x8000/100)
                    {
                        have_touch = 1;
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogXpress / (float)0x8000;
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogYpress / (float)0x8000;

                    }
                    else if (pointer_device_l == 2)
                    {
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2 * l_analog_x_ret / (float)0x8000;
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2 * l_analog_y_ret / (float)0x8000;
                        rotate_input(analogX, analogY, input_rotation);
                    }

                }
                else //right analog takes priority when both set to absolute
                {
                    analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2 * r_analog_x_ret / (float)0x8000;
                    analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2 * r_analog_y_ret / (float)0x8000;
                    rotate_input(analogX, analogY, input_rotation);
                }

                //set absolute analog position offset to center of screen
                TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));
            }

            else if((pointer_device_l == 2) || (pointer_device_l == 3))
            {
                if(pointer_device_l == 2)
                {
                    analogX = l_analog_x_ret;
                    analogY = l_analog_y_ret;
                    rotate_input(analogX, analogY, input_rotation);
                    analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogX / (float)0x8000;
                    analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogY / (float)0x8000;

                    TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                    TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));


                }
                if(pointer_device_l == 3)
                {
                    int16_t analogXpress = l_analog_x_ret;
                    int16_t analogYpress = l_analog_y_ret;
                    rotate_input(analogXpress, analogYpress, input_rotation);
                    double radius = sqrt(analogXpress * analogXpress + analogYpress * analogYpress);
                    if (radius > (float)analog_stick_deadzone*(float)0x8000/100)
                    {
                        have_touch = 1;
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogXpress / (float)0x8000;
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogYpress / (float)0x8000;

                        TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                        TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));
                    }
                }
            }

            else
            {
                if(pointer_device_r == 2)
                {
                    analogX = r_analog_x_ret;
                    analogY = r_analog_y_ret;
                    rotate_input(analogX, analogY, input_rotation);
                    analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogX / (float)0x8000;
                    analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogY / (float)0x8000;

                    TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                    TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));
                }

                if(pointer_device_r == 3)
                {
                    int16_t analogXpress = r_analog_x_ret;
                    int16_t analogYpress = r_analog_y_ret;
                    rotate_input(analogXpress, analogYpress, input_rotation);
                    double radius = sqrt(analogXpress * analogXpress + analogYpress * analogYpress);
                    if (radius > (float)analog_stick_deadzone*(float)0x8000/100)
                    {
                        have_touch = 1;
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogXpress / (float)0x8000;
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogYpress / (float)0x8000;

                        TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                        TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));

                    }
                }
            }
        }

        //log_cb(RETRO_LOG_DEBUG, "%d %d.\n", GPU_LR_FRAMEBUFFER_NATIVE_WIDTH,GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
        //log_cb(RETRO_LOG_DEBUG, "%d %d.\n", analogX,analogY);

        have_touch = have_touch || (ret & (1 << RETRO_DEVICE_ID_JOYPAD_R2));

        FramesWithPointer = (analogX || analogY) ? FramesWithPointerBase : FramesWithPointer;

   }

   if(mouse_enable)
   {
      // TOUCH: Mouse
      if(!touchEnabled)
      {
         int16_t mouseX = input_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
         int16_t mouseY = input_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
         rotate_input(mouseX, mouseY, input_rotation);
         have_touch           = have_touch || input_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);

         mouse_x_delta += mouseX * mouse_speed;
         mouse_y_delta += mouseY * mouse_speed;

         mouseX = (int16_t) trunc(mouse_x_delta);
         mouse_x_delta -= mouseX;
         mouseY = (int16_t) trunc(mouse_y_delta);
         mouse_y_delta -= mouseY;

         TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), TouchX + mouseX);
         TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), TouchY + mouseY);
         FramesWithPointer = (mouseX || mouseY) ? FramesWithPointerBase : FramesWithPointer;
      }
      // TOUCH: Pointer
      else if(input_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED))
      {
         int touch_area_width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         int touch_area_height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;

         int16_t mouseX        = input_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
         int16_t mouseY        = input_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
         rotate_input(mouseX, mouseY, input_rotation);

         int x = ((int) mouseX + 0x8000) * layout.width / 0x10000;
         int y = ((int) mouseY + 0x8000) * layout.height / 0x10000;

         if (hybrid_layout_scale != 1 && current_layout == LAYOUT_HYBRID_BOTTOM_ONLY && !hybrid_cursor_always_smallscreen)
         {
             /* Hybrid: We're on the big screen at triple scale, so triple the size */
             touch_area_width *= hybrid_layout_scale;
             touch_area_height *= hybrid_layout_scale;
         }
         else if ((hybrid_layout_scale == 1)
                  && ((current_layout == LAYOUT_HYBRID_TOP_ONLY)  ||
                      (current_layout == LAYOUT_HYBRID_BOTTOM_ONLY && hybrid_cursor_always_smallscreen && hybrid_layout_showbothscreens)))
         {
             /* Hybrid: We're on the small screen at hybrid scale 1, so 1/3 the size */
             touch_area_width /= hybrid_layout_ratio;
             touch_area_height /= hybrid_layout_ratio;
         }

         if ((x >= layout.touch_x) && (x < layout.touch_x + touch_area_width) &&
               (y >= layout.touch_y) && (y < layout.touch_y + touch_area_height))
         {
            have_touch = true;

            TouchX = (x - layout.touch_x) * GPU_LR_FRAMEBUFFER_NATIVE_WIDTH / touch_area_width;
            TouchY = (y - layout.touch_y) * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT / touch_area_height;
         }
      }
   }

   if(have_touch)
      NDS_setTouchPos(TouchX / scale, TouchY / scale);
   else
      NDS_releaseTouch();

   NDS_setPad(
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT  )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT   )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN   )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_UP     )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_START  )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_B      )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_A      )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_Y      )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_X      )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_L      )),
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_R      )),
         0, // debug
         (ret & (1 << RETRO_DEVICE_ID_JOYPAD_L2     )) //Lid
         );

   if (ret & (1 << RETRO_DEVICE_ID_JOYPAD_L3))
      NDS_setMic(true);
   else
      NDS_setMic(false);

   // BUTTONS
   NDS_beginProcessingInput();

   if((ret & (1 << RETRO_DEVICE_ID_JOYPAD_R3)) && delay_timer == 0)
   {
      switch (current_layout)
      {
         case LAYOUT_TOP_BOTTOM:
            current_layout = LAYOUT_BOTTOM_TOP;
            break;
         case LAYOUT_BOTTOM_TOP:
            current_layout = LAYOUT_TOP_BOTTOM;
            break;
         case LAYOUT_LEFT_RIGHT:
            current_layout = LAYOUT_RIGHT_LEFT;
            break;
         case LAYOUT_RIGHT_LEFT:
            current_layout = LAYOUT_LEFT_RIGHT;
            break;
         case LAYOUT_TOP_ONLY:
            current_layout = LAYOUT_BOTTOM_ONLY;
            break;
         case LAYOUT_BOTTOM_ONLY:
            current_layout = LAYOUT_TOP_ONLY;
            break;
         case LAYOUT_HYBRID_TOP_ONLY:
            {
               current_layout = LAYOUT_HYBRID_BOTTOM_ONLY;
               //Need to swap around DST variables
               uint16_t*swap = layout.dst;
               layout.dst = layout.dst2;
               layout.dst2 = swap;
               //Need to reset Touch position to 0 with these conditions or it causes problems with mouse
               if(hybrid_layout_scale == 1 && (!hybrid_layout_showbothscreens || !hybrid_cursor_always_smallscreen))
               {
                  TouchX = 0;
                  TouchY = 0;
               }
               break;
            }
         case LAYOUT_HYBRID_BOTTOM_ONLY:
            {
               current_layout = LAYOUT_HYBRID_TOP_ONLY;
               uint16_t*swap = layout.dst;
               layout.dst = layout.dst2;
               layout.dst2 = swap;
               //Need to reset Touch position to 0 with these conditions are it causes problems with mouse
               if(hybrid_layout_scale == 1 && (!hybrid_layout_showbothscreens || !hybrid_cursor_always_smallscreen))
               {
                  TouchX = 0;
                  TouchY = 0;
               }
               break;
            }
      } // switch
      delay_timer++;
   }

   if(delay_timer != 0)
   {
      delay_timer++;
      if(delay_timer == 30)
         delay_timer = 0;
   }

   NDS_endProcessingInput();

   // RUN
   frameIndex ++;
   bool skipped = frameIndex <= frameSkip;

   if (skipped)
      NDS_SkipNextFrame();

   NDS_exec<false>();

   SPU_Emulate_user();

   static int previous_layout = LAYOUTS_MAX;
   static int previous_screen_gap = 0;
   static int previous_showbothscreens = true;

   if (previous_layout          != current_layout ||
       previous_screen_gap      != nds_screen_gap ||
       previous_showbothscreens != hybrid_layout_showbothscreens)
   {
          memset (screen_buf, 0, layout.width * layout.height * bpp);
          previous_layout = current_layout;
          previous_screen_gap = nds_screen_gap;
          previous_showbothscreens = hybrid_layout_showbothscreens;
   }

   if (!skipped)
   {
          if (colorMode == RETRO_PIXEL_FORMAT_XRGB8888)
          {
              if (current_layout == LAYOUT_HYBRID_TOP_ONLY || current_layout == LAYOUT_HYBRID_BOTTOM_ONLY)
              {
                    /* Using the top screen as source */
                    u32 *screen = (u32 *)(GPU->GetDisplayInfo().masterCustomBuffer);

                    if (current_layout == LAYOUT_HYBRID_TOP_ONLY)
                    {
                            if(hybrid_layout_scale == hybrid_layout_ratio)
                                    SwapScreenLarge_32((u32 *) layout.dst, screen, layout.pitch);
                            else
                                    SwapScreen_32 ((u32 *) layout.dst, screen, layout.pitch);

                            SwapScreenSmall_32((u32 *) layout.dst2, screen, layout.pitch, true, hybrid_layout_showbothscreens);
                    }
                    else
                        SwapScreenSmall_32((u32 *) layout.dst, screen, layout.pitch, true, true);

                    /* Using the bottom screen as source */
                    screen = (u32 *)(GPU->GetDisplayInfo().masterCustomBuffer) + (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
                    if (current_layout == LAYOUT_HYBRID_BOTTOM_ONLY)
                    {
                            if(hybrid_layout_scale == hybrid_layout_ratio)
                                    SwapScreenLarge_32((u32 *) layout.dst2,(u32 *) screen, layout.pitch);
                            else
                                    SwapScreen_32 ((u32 *) layout.dst2,  screen, layout.pitch);

                            SwapScreenSmall_32 ((u32 *) layout.dst, screen, layout.pitch, false , hybrid_layout_showbothscreens);

                            //Keep the Touch Cursor on the Small Screen, even if the bottom is the primary screen? Make this configurable by user? (Needs work to get working with hybrid_layout_scale==3 and layout_hybrid_bottom_only)
                            if (hybrid_cursor_always_smallscreen && hybrid_layout_showbothscreens)
                                    DrawPointerHybrid (layout.dst, layout.pitch, false);
                            else
                                    DrawPointerHybrid (layout.dst2, layout.pitch, true);
                    }
                    else
                    {
                            SwapScreenSmall_32 ((u32 *) layout.dst2, screen, layout.pitch, false, true);
                            DrawPointerHybrid (layout.dst2, layout.pitch, false);
                    }
              }
              //This is for every layout except Hybrid - same as before
              else
              {
                    u32 *screen = (u32 *)(GPU->GetDisplayInfo().masterCustomBuffer);
                    if (layout.draw_screen1)
                            SwapScreen_32 ((u32 *) layout.dst, (u32 *) screen, layout.pitch);
                    if (layout.draw_screen2)
                    {
                            screen = (u32 *) ((u8 *) (GPU->GetDisplayInfo().masterCustomBuffer) + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * bpp);
                            SwapScreen_32 ((u32 *) layout.dst2, (u32 *) screen, layout.pitch);
                            DrawPointer(layout.dst2, layout.pitch);
                    }
              }
          }
          else
          {
              if (current_layout == LAYOUT_HYBRID_TOP_ONLY || current_layout == LAYOUT_HYBRID_BOTTOM_ONLY)
              {
                  u16 *screen = (u16 *)(GPU->GetDisplayInfo().masterCustomBuffer);
                  if (current_layout == LAYOUT_HYBRID_TOP_ONLY)
                  {
                      if(hybrid_layout_scale == hybrid_layout_ratio)
                          SwapScreenLarge(layout.dst,  screen, layout.pitch);
                      else
                          SwapScreen (layout.dst,  screen, layout.pitch);
                      SwapScreenSmall(layout.dst2, screen, layout.pitch, true, hybrid_layout_showbothscreens);
                  }
                  else if (current_layout == LAYOUT_HYBRID_BOTTOM_ONLY)
                      SwapScreenSmall(layout.dst, screen, layout.pitch, true, true);

                  screen = (u16 *)(GPU->GetDisplayInfo().masterCustomBuffer) + (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
                  if (current_layout == LAYOUT_HYBRID_BOTTOM_ONLY)
                  {
                      if(hybrid_layout_scale == hybrid_layout_ratio)
                          SwapScreenLarge(layout.dst2,  screen, layout.pitch);
                      else
                          SwapScreen (layout.dst2,  screen, layout.pitch);
                      SwapScreenSmall (layout.dst, screen, layout.pitch, false , hybrid_layout_showbothscreens);
                      //Keep the Touch Cursor on the Small Screen, even if the bottom is the primary screen? Make this configurable by user? (Needs work to get working with hybrid_layout_scale==3 and layout_hybrid_bottom_only)
                      if(hybrid_cursor_always_smallscreen && hybrid_layout_showbothscreens)
                          DrawPointerHybrid (layout.dst, layout.pitch, false);
                      else
                          DrawPointerHybrid (layout.dst2, layout.pitch, true);
                  }
                  else
                  {
                      SwapScreenSmall (layout.dst2, screen, layout.pitch, false, true);
                      DrawPointerHybrid (layout.dst2, layout.pitch, false);
                  }
              }
          //This is for every layout except Hybrid - same as before
              else
              {
                  u16 *screen = (u16 *)(GPU->GetDisplayInfo().masterCustomBuffer);
                  if (layout.draw_screen1)
                      SwapScreen (layout.dst,  screen, layout.pitch);
                  if (layout.draw_screen2)
                  {
                      screen = (u16 *) ((u8 *)(GPU->GetDisplayInfo().masterCustomBuffer) + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * bpp);
                      SwapScreen (layout.dst2, screen, layout.pitch);
                      DrawPointer(layout.dst2, layout.pitch);
                  }
              }
          }
   }

   if (opengl_mode)
   {
#ifdef HAVE_OPENGL
      if (!skipped)
      {
          GLint drawfb = 0, readfb = 0, drawb = 0, readb = 0, active_texture = 0, program = 0;
          glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawfb);
          glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readfb);
          glGetIntegerv(GL_DRAW_BUFFER, &drawb);
          glGetIntegerv(GL_READ_BUFFER, &readb);
          glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);

          glActiveTexture(GL_TEXTURE0);
          glUseProgram(0);

          if (pbo == 0)
              glGenBuffers(1, &pbo);
          if (fbo == 0)
              glGenFramebuffers(1, &fbo);
          if (tex == 0)
              glGenTextures(1, &tex);

          /* Upload data via pixel-buffer object */
          glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
          glBufferData(GL_PIXEL_UNPACK_BUFFER, layout.width * layout.height * bpp, NULL, GL_STREAM_DRAW);
          void *pbo_buffer;
          if (glMapBufferRange)
             pbo_buffer = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, layout.width * layout.height * bpp, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
          else
             pbo_buffer = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
          memcpy(pbo_buffer, screen_buf, layout.width * layout.height * bpp);
          glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

          glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
          glBindTexture(GL_TEXTURE_2D, tex);

          if (current_texture_width != layout.width || current_texture_height != layout.height)
          {
             glTexImage2D(GL_TEXTURE_2D, 0, internal_format, layout.width, layout.height, 0, texture_type, texture_format, 0);
             glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
             glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
             glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
             glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

             current_texture_width = layout.width;
             current_texture_height = layout.height;
          }
          else
          {
             glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, layout.width, layout.height, texture_type, texture_format, 0);
          }

          glBindFramebuffer(GL_FRAMEBUFFER, hw_render.get_current_framebuffer());
          glClearColor(0.0, 0.0, 0.0, 1.0);
          glClear(GL_COLOR_BUFFER_BIT);

          glBindFramebuffer(GL_FRAMEBUFFER, fbo);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

          glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
          glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hw_render.get_current_framebuffer());
          glBlitFramebuffer(0, 0, layout.width, layout.height, 0, 0, layout.width, layout.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
          glBindTexture(GL_TEXTURE_2D, 0);
          glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

          glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawfb);
          glBindFramebuffer(GL_READ_FRAMEBUFFER, readfb);
          glReadBuffer(readb);
          glDrawBuffer(drawb);
          glActiveTexture(active_texture);
          glUseProgram(program);
      }

      video_cb(skipped ? 0 : RETRO_HW_FRAME_BUFFER_VALID, layout.width, layout.height, 0);
#endif
   }
   else
      video_cb(skipped ? 0 : screen_buf, layout.width, layout.height, layout.pitch * 2);

   frameIndex = skipped ? frameIndex : 0;
}

size_t retro_serialize_size (void)
{
    // HACK: Usually around 10 MB but can vary frame to frame!
    return 1024 * 1024 * 12;
}

bool retro_serialize(void *data, size_t size)
{
    EMUFILE_MEMORY state;
    savestate_save(state, 0);

    if(state.size() <= size)
    {
        memcpy(data, state.buf(), state.size());
        return true;
    }

    return false;
}

bool retro_unserialize(const void * data, size_t size)
{
    EMUFILE_MEMORY state(const_cast<void*>(data), size);
    return savestate_load(state);
}

bool init_gl_context(retro_hw_context_type type)
{
   hw_render.context_type = type;
   hw_render.cache_context = false;
   hw_render.context_reset = context_reset;
   hw_render.context_destroy = context_destroy;
   hw_render.bottom_left_origin = false;
   hw_render.depth = true;

   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         hw_render.version_major = 3;
         hw_render.version_minor = 1;
         break;
      case RETRO_HW_CONTEXT_OPENGL:
         hw_render.version_major = 0;
         hw_render.version_minor = 0;
         break;
   }

   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;
   return true;
}

bool retro_load_game(const struct retro_game_info *game)
{
   if (!game)
      return false;

#ifdef HAVE_OPENGL
   if (opengl_mode)
   {
       if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT, NULL))
       {
          log_cb(RETRO_LOG_WARN, "Couldn't set shared context. Some things may break.\n");
       }

       // get current video driver
       retro_hw_context_type preferred;
       if (!environ_cb(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &preferred))
          preferred = RETRO_HW_CONTEXT_DUMMY;
       bool found_gl_context = false;
       if (preferred == RETRO_HW_CONTEXT_OPENGL || preferred == RETRO_HW_CONTEXT_OPENGL_CORE)
       {
          // try requesting the right context for current driver
          found_gl_context = init_gl_context(preferred);
       }
       // try every context as fallback if current driver wasn't found
       if (!found_gl_context)
          found_gl_context = init_gl_context(RETRO_HW_CONTEXT_OPENGL_CORE);
       if (!found_gl_context)
          found_gl_context = init_gl_context(RETRO_HW_CONTEXT_OPENGL);

       oglrender_init        = dummy_retro_gl_init;
       oglrender_beginOpenGL = dummy_retro_gl_begin;
       oglrender_endOpenGL   = dummy_retro_gl_end;

       if (!found_gl_context)
       {
           log_cb(RETRO_LOG_ERROR, "Couldn't create rendering context. Using software rasterizer.\n");
           opengl_mode = false;
           bpp = 2;
           colorMode = RETRO_PIXEL_FORMAT_RGB565;
           environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &colorMode);
           GPU->SetColorFormat(NDSColorFormat_BGR555_Rev);
       }
   }
#endif

struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "X" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Y" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "Lid Close/Open" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,     "Make Microphone Noise" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "Tap Stylus" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,     "Quick Screen Switch" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,  "Select" },

      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (NDS_LoadROM(game->path) < 0)
   {
       execute = false;
       return false;
   }

   execute = true;

   return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
#if 0
    if(game_type == RETRO_GAME_TYPE_SUPER_GAME_BOY && num_info == 2)
    {
        strncpy(GBAgameName, info[1].path, sizeof(GBAgameName));
        addonsChangePak(NDS_ADDON_GBAGAME);

        return retro_load_game(&info[0]);
    }
#endif
    return false;
}

void retro_unload_game (void)
{
    NDS_FreeROM();
    if (screen_buf)
       free(screen_buf);
    screen_buf = NULL;
    execute    = 0;
}

void *retro_get_memory_data(unsigned type)
{
   if (type == RETRO_MEMORY_SYSTEM_RAM)
      return MMU.MAIN_MEM;
   else
      return NULL;
}

size_t retro_get_memory_size(unsigned type)
{
   if (type == RETRO_MEMORY_SYSTEM_RAM)
      return CommonSettings.ConsoleType == NDS_CONSOLE_TYPE_DSI ?
         0x1000000 : 0x0400000;
   else
      return 0;
}

// Stubs
void retro_set_controller_port_device(unsigned in_port, unsigned device) { }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

extern CHEATS *cheats;

void retro_cheat_reset(void)
{
   if (cheats)
      cheats->clear();
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   char ds_code[1024];
   char desc[1024];
   strcpy(ds_code, code);
   strcpy(desc, "N/A");

   if (!cheats)
      return;

   if (cheats->add_AR(ds_code, desc, 1) != TRUE)
   {
      /* Couldn't add Action Replay code */
   }
}

unsigned retro_get_region (void) { return RETRO_REGION_NTSC; }

#if defined(PSP)
int ftruncate(int fd, off_t length)
{
   int ret;
   SceOff oldpos;
   if (!__PSP_IS_FD_VALID(fd)) {
      errno = EBADF;
      return -1;
   }

   switch(__psp_descriptormap[fd]->type)
   {
      case __PSP_DESCRIPTOR_TYPE_FILE:
         if (__psp_descriptormap[fd]->filename != NULL) {
            if (!(__psp_descriptormap[fd]->flags & (O_WRONLY | O_RDWR)))
               break;
            return truncate(__psp_descriptormap[fd]->filename, length);
            /* ANSI sez ftruncate doesn't move the file pointer */
         }
         break;
      case __PSP_DESCRIPTOR_TYPE_TTY:
      case __PSP_DESCRIPTOR_TYPE_PIPE:
      case __PSP_DESCRIPTOR_TYPE_SOCKET:
      default:
         break;
   }

   errno = EINVAL;
   return -1;
}
#endif
