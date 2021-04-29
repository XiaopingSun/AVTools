//
//  RawVideo.cpp
//  AVTools
//
//  Created by WorkSpace_Sun on 2021/4/28.
//

#include "RawVideo.h"
#include <getopt.h>
#include <stdlib.h>
#include "SDL.h"

// pix_format
// rgb
#define RGB24  "RGB24"
#define BGR24  "BGR24"
#define RGB555  "RGB555"
#define RGB565  "RGB565"
#define RGBA     "RGBA"
#define BGRA     "BGRA"

// yuv
#define YUV420P "YUV420P"
#define NV12      "NV12"
#define NV21      "NV21"

#define YUYV        "YUYV"
#define YVYU        "YVYU"
#define UYVY        "UYVY"

// sdl event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
#define QUIT_EVENT  (SDL_USEREVENT + 2)

static SDL_PixelFormatEnum get_pixel_format_in_sdl(char *pixel_format, int *bit_per_pixel, int *bit_per_pixel_for_render);
static void get_window_size(int *window_width, int *window_height, int pixel_width, int pixel_height);
static void open(char *url, char *pixel_format, int fps, int pixel_width, int pixel_height);
static int refresh_video_timer(void *udata);
static int thread_exit = 0;

const char *support_pf_list[] = {
    RGB24,
    BGR24,
    RGB555,
    RGB565,
    RGBA,
    BGRA,
    
    YUV420P,
    NV12,
    NV21,
    YUYV,
    YVYU,
    UYVY
};

static struct option tool_long_options[] = {
    {"help", no_argument, NULL, '`'}
};

static void show_module_help() {
    printf("Support Format:\n\n");
    for (int i = 0; i < sizeof(support_pf_list) / sizeof(char *); i++) {
        printf("  %s\n", support_pf_list[i]);
    }
    printf("\n");
    printf("Param:\n\n");
    printf("  -i:   Input File Local Path\n");
    printf("  -f:   Pixel Format\n");
    printf("  -r:   Frame Rate\n");
    printf("  -w:   Width\n");
    printf("  -h:   Height\n");
    printf("\n");
    printf("Usage:\n\n");
    printf("  AVTools RawVideo -f YUV420P -r 25 -w 720 -h 1280 -i input.flv\n");
}

void raw_video_parse_cmd(int argc, char *argv[]) {
    
    int option = 0;   // getopt_long的返回值，返回匹配到字符的ascii码，没有匹配到可读参数时返回-1
    int fps = 0;    // 帧率
    int width = 0;    // 分辨率宽
    int height = 0;    // 分辨率高
    char *pixel_format = NULL;   // 像素格式
    char *url = NULL;   // 输入文件路径
    bool format_supported = false;   // 输入像素格式是否支持
    
    while (EOF != (option = getopt_long(argc, argv, "i:r:w:h:f:", tool_long_options, NULL))) {
        switch (option) {
            case '`':
                show_module_help();
                return;
                break;
            case 'i':
                url = optarg;
                break;
            case 'r':
                fps = atoi(optarg);
                break;
            case 'w':
                width = atoi(optarg);
                break;
            case 'h':
                height= atoi(optarg);
                break;
            case 'f':
                pixel_format = optarg;
                break;
            case '?':
                printf("Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
                return;
                break;
            default:
                break;
        }
    }
    
    if (NULL == url || 0 == width || 0 == height || NULL == pixel_format) {
        printf("RawVideo Param Error, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
        return;
    }
    
    // 帧率参数可选，默认25fps
    fps = fps == 0 ? 25 : fps;
    
    // 判断输入的像素格式是否支持
    for (int i = 0; i < sizeof(support_pf_list) / sizeof(char *); i++) {
        if (0 == strcmp(pixel_format, support_pf_list[i])) {
            format_supported = true;
            break;
        }
    }
    
    if (format_supported) {
        open(url, pixel_format, fps, width, height);
    } else {
        printf("Unsupport Pixel Buffer Format, Use 'AVTools %s --help' To Show Detail Usage.\n", argv[1]);
    }
}

static void  open(char *url, char *pixel_format, int fps, int pixel_width, int pixel_height) {
    
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_Thread *time_thread = NULL;
    SDL_Rect rect = {0, 0};
    SDL_Event event;
    FILE *file = NULL;
    
    // 计算窗口大小: 以pixel的宽高比计算SDL窗口大小
    int window_width = 0;
    int window_height = 0;
    get_window_size(&window_width, &window_height, pixel_width, pixel_height);
    
    // 获取SDL内部定义的像素格式枚举
    // bit_per_pixel用来标识单帧每个像素平均下来所占的bit大小
    // bit_per_pixel_for_render用来标识送进texture的每行的bit大小  planar和semi-planar需要先把Y送进去  所以bit_per_pixel_for_render等于pixel_width   packed需要把每个像素各分量打包在一起送上去
    int bit_per_pixel = 0;
    int bit_per_pixel_for_render = 0;
    SDL_PixelFormatEnum sdl_pixel_format = get_pixel_format_in_sdl(pixel_format, &bit_per_pixel, &bit_per_pixel_for_render);
    
    // 读取的数据按8字节对齐
    size_t byte_per_frame = pixel_width * pixel_height * bit_per_pixel / 8;
    char buffer[byte_per_frame];

    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf("Could Not Initialize SDL - %s\n", SDL_GetError());
        goto __FAIL;
    }
    
    // 创建SDL窗口
    window = SDL_CreateWindow("AVTools RawVideo Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("SDL: Could Not Create Window - Exiting:%s\n", SDL_GetError());
        goto __FAIL;
    }
    
    // 创建渲染图层
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        printf("SDL: Could Not Create Renderer - Exiting:%s\n", SDL_GetError());
        goto __FAIL;
    }
    
    // 图层上创建纹理  此处的宽高要用视频文件分辨率宽高  不要使用窗口宽高
    texture = SDL_CreateTexture(renderer, sdl_pixel_format, SDL_TEXTUREACCESS_STREAMING, pixel_width, pixel_height);
    if (!texture) {
        printf("SDL: Could Not Create Texture - Exiting:%s\n", SDL_GetError());
        goto __FAIL;
    }
    
    // 打开资源文件
    file = fopen(url, "rb+");
    if (!file) {
        printf("Could Not Open This File '%s', Please Check The Path.\n", url);
        goto __FAIL;
    }
    
    // 创建线程跑timer  定时通知主线程更新纹理
    time_thread = SDL_CreateThread(refresh_video_timer, NULL, &fps);
    
    // 循环监听SDL事件  更新纹理、调整窗口大小、退出
    while (1) {
        SDL_WaitEvent(&event);
        if(event.type == REFRESH_EVENT){
            // 读取每帧画面的数据填充到buffer
            // 这里有一个循环播放
            if (fread(buffer, 1, byte_per_frame, file) != byte_per_frame) {
                fseek(file, 0, SEEK_SET);
                fread(buffer, 1, byte_per_frame, file);
            }

            SDL_UpdateTexture(texture, NULL, buffer, pixel_width * bit_per_pixel_for_render / 8);

            rect.x = 0;
            rect.y = 0;
            rect.w = window_width;
            rect.h = window_height;

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, &rect);
            SDL_RenderPresent(renderer);
        } else if (event.type == SDL_WINDOWEVENT){
            // 窗口缩放
            SDL_GetWindowSize(window, &window_width, &window_height);
        } else if (event.type == SDL_QUIT){
            // SDL退出事件
            thread_exit=1;
        } else if (event.type == QUIT_EVENT){
            break;
        }
    }

__FAIL:
    
    // 关闭文件
    if (file) {
        fclose(file);
    }
    
    // 释放所有SDL组件
    SDL_Quit();
}

static SDL_PixelFormatEnum get_pixel_format_in_sdl(char *pixel_format, int *bit_per_pixel, int *bit_per_pixel_for_render) {
    SDL_PixelFormatEnum sdl_pixel_format;
    if (0 == strcmp(pixel_format, RGB24)) {
        sdl_pixel_format = SDL_PIXELFORMAT_RGB24;
        *bit_per_pixel = 24;
        *bit_per_pixel_for_render = 24;
    } else if (0 == strcmp(pixel_format, BGR24)) {
        sdl_pixel_format = SDL_PIXELFORMAT_BGR24;
        *bit_per_pixel = 24;
        *bit_per_pixel_for_render = 24;
    } else if (0 == strcmp(pixel_format, RGB555)) {
        sdl_pixel_format = SDL_PIXELFORMAT_RGB555;
        *bit_per_pixel = 16;
        *bit_per_pixel_for_render = 16;
    } else if (0 == strcmp(pixel_format, RGB565)) {
        sdl_pixel_format = SDL_PIXELFORMAT_RGB565;
        *bit_per_pixel = 16;
        *bit_per_pixel_for_render = 16;
    } else if (0 == strcmp(pixel_format, RGBA)) {
        sdl_pixel_format = SDL_PIXELFORMAT_RGBA32;
        *bit_per_pixel = 32;
        *bit_per_pixel_for_render = 32;
    } else if (0 == strcmp(pixel_format, BGRA)) {
        sdl_pixel_format = SDL_PIXELFORMAT_BGRA32;
        *bit_per_pixel = 32;
        *bit_per_pixel_for_render = 32;
    } else if (0 == strcmp(pixel_format, YUV420P)) {
        sdl_pixel_format = SDL_PIXELFORMAT_IYUV;
        *bit_per_pixel = 12;
        *bit_per_pixel_for_render = 8;
    } else if (0 == strcmp(pixel_format, NV12)) {
        sdl_pixel_format = SDL_PIXELFORMAT_NV12;
        *bit_per_pixel = 12;
        *bit_per_pixel_for_render = 8;
    } else if (0 == strcmp(pixel_format, NV21)) {
        sdl_pixel_format = SDL_PIXELFORMAT_NV21;
        *bit_per_pixel = 12;
        *bit_per_pixel_for_render = 8;
    } else if (0 == strcmp(pixel_format, YUYV)) {
        sdl_pixel_format = SDL_PIXELFORMAT_YUY2;
        *bit_per_pixel = 16;
        *bit_per_pixel_for_render = 16;
    } else if (0 == strcmp(pixel_format, YVYU)) {
        sdl_pixel_format = SDL_PIXELFORMAT_YVYU;
        *bit_per_pixel = 16;
        *bit_per_pixel_for_render = 16;
    } else if (0 == strcmp(pixel_format, UYVY)) {
        sdl_pixel_format = SDL_PIXELFORMAT_UYVY;
        *bit_per_pixel = 16;
        *bit_per_pixel_for_render = 16;
    } else {
        sdl_pixel_format = SDL_PIXELFORMAT_UNKNOWN;
        *bit_per_pixel = 0;
    }
    return sdl_pixel_format;
}

static void get_window_size(int *window_width, int *window_height, int pixel_width, int pixel_height) {
    int base_length = 360;
    if (pixel_width < pixel_height) {
        *window_width = base_length;
        *window_height = base_length * pixel_height / pixel_width;
    } else if (pixel_width > pixel_height) {
        *window_height = base_length;
        *window_width = base_length * pixel_width / pixel_height;
    } else {
        *window_width = *window_height = base_length + 120;
    }
}

static int refresh_video_timer(void *udata) {
    int fps = *(int *)udata;
    thread_exit = 0;
    
    while (!thread_exit) {
        // 自定义事件  通知主线程更新纹理
        SDL_Event event;
        event.type = REFRESH_EVENT;
        SDL_PushEvent(&event);
        SDL_Delay(1000 / fps);
    }
    
    thread_exit = 0;
    // 自定义事件  退出sdl
    SDL_Event event;
    event.type = QUIT_EVENT;
    SDL_PushEvent(&event);
    
    return 0;
}








