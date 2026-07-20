#pragma once
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xinerama.h>
#include <X11/Xft/Xft.h>
#include <sys/wait.h> 
#include <sys/stat.h>
#include <pthread.h>
#include <Imlib2.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include <stdint.h>

/* Typedef definitions */
typedef XftDraw *Draw;
typedef XftColor *Color;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef unsigned short int uint16;
typedef unsigned int uint32;
typedef unsigned long long int uint64;

/* STRUCST */
typedef struct Client Client;

struct Client {
    Display *d;
    int scr;
    Colormap cmap;
    Window w;
    Drawable buf;
    Pixmap pix;
    Atom xembed, wmdeletewin, wmname, netwmname, wmnameutf8, netwmiconname, netwmpid, wm_state, wm_fullscreen, wm_type, wm_type_utility, wm_protocols;
    Draw draw;
    Visual *vis;
    XSetWindowAttributes attrs;
    int width, height;

    Window root;
    XVisualInfo *vi;
    GLXContext glc;
    XftFont *font;
    XftColor xftcolor;
    int running;
};

typedef struct {
  Color *col;
  size_t collen;
  Font font;
  GC gc;
} DC;

typedef struct {
    char *path;
    GLuint tex;
    int w, h;
    int src_w, src_h;
    unsigned char *pixels;
    int ready;
    int uploaded;
    GLuint label_tex;
    int label_w, label_h;
} WallItem;

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(Client *c);
} Key;


/* FUNCTION DEFINITION */
void initx(Client *c);
void run(Client *c);
void get_files(void);
void cleanup(Client *c);
void resize(Client *c, int w, int h);
GLuint make_text_texture(Client *c, const char *text, XGlyphInfo *extents_out);
void draw_quad(GLuint tex, int x, int y, int w, int h);
static int grid_cols(Client *c);
static int grid_rows(Client *c);
static int items_page(Client *c);

// Dirs
const char *get_home(void);
const char *get_dir(char *buf, size_t bufz);
const char *read_dir(char *output_buffer, size_t buffer_size);

// Ensure single instance
static void release_lock(void);
static void handle_term(int sig);
static void get_lock_path(char *buf, size_t n);
static void ensure_single_instance(void);


// IMAGES HANDLERS
GLuint load_image_texture(Client *c, const char *path, int *out_w, int *out_h);
static unsigned char *decode_thumbnail(const char *path, int *out_w, int *out_h, int *out_src_w, int *out_src_h);
void load_wallpapers_async(Client *c);
void draw_wallpapers(Client *c);
void upload_pending_textures(Client *c);
static void draw_highlight(int x, int y, int w, int h);
char **get_files_from_dir(const char *dir_path, int *out_count);

/* Handle ui nav */
void nav_left(Client *c);
void nav_right(Client *c);
void nav_up(Client *c);
void nav_down(Client *c);
void nav_select(Client *c);
void nav_quit(Client *c);
void handle_key(Client *c, XKeyEvent *e);

/* UI */
void draw_rect(int x, int y, int w, int h, float r, float g, float b, float a);
void draw_text(Client *c, GLuint *ut, const char *msg, int *w, int *h, size_t max_chars);
void draw_hud(Client *c);

static void *loader_main(void *arg);
int has_ext(const char *name, const char *ext);
long get_file_size(const char *path);
void format_size(long bytes, char *buf, size_t bufsz);
void get_imgd(char *dmw, size_t dwmz, char *dmh, size_t dhmz);
const char *get_ext(const char *path);
char *get_namef(void);
