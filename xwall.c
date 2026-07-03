#define _DEFAULT_SOURCE
#include <X11/keysym.h>
#include <err.h>
#include <libgen.h>
#include <pwd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "xwall.h"
#include "config.h"

static GLuint fis = 0;
static int fs_w, fs_h;

static GLuint tim = 0;
static int tim_w, tim_h;

static GLuint sz = 0;
static int sz_w, sz_h;

static GLuint fm = 0;
static int fm_w, fm_h;

#define CELL_PAD_X 20
#define CELL_PAD_Y 40
#define CELL_W (THUMB_MAX + CELL_PAD_X)
#define CELL_H (THUMB_MAX + CELL_PAD_Y)

static WallItem *items;
static int item_count;
static int dirty = 1;

static int selected = 0;

static pthread_t loader_thread;
static volatile int loader_done = 0;

static void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void resize(Client *c, int w, int h) {
    c->width = w;
    c->height = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static int grid_cols(Client *c) {
    int cols = c->width / CELL_W;
    return cols < 1 ? 1 : cols;
}

static int grid_rows(Client *c) {
    int rows = c->height / CELL_H;
    return rows < 1 ? 1 : rows;
}

static int items_page(Client *c) {
    return grid_cols(c) * grid_rows(c);
}

void initx(Client *c) {
    c->d = XOpenDisplay(NULL);
    if (!c->d)
        die("cannot open display");

    int mw = DisplayWidth(c->d, c->scr);
    int mh = DisplayHeight(c->d, c->scr);

    c->scr = DefaultScreen(c->d);
    c->root = RootWindow(c->d, c->scr);
    c->width = 900;
    c->height = 600;
    c->running = 1;

    int glattr[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        None
    };


  if (XineramaIsActive(c->d)) {
    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(c->d, &n);
    Window dw;
    int di;
    unsigned int du;
    int cx, cy;
    XQueryPointer(c->d, c->root, &dw, &dw, &cx, &cy, &di, &di, &du);
    for (int i = 0; i < n; i++) {
      if (cx >= info[i].x_org && cx < info[i].x_org + info[i].width &&
          cy >= info[i].y_org && cy < info[i].y_org + info[i].height) {
        mw = info[i].width;
        mh = info[i].height;
        break;
      }
    }
    XFree(info);
  }

    c->vi = glXChooseVisual(c->d, c->scr, glattr);
    if (!c->vi)
        die("no suitable GLX visual");

    c->vis = c->vi->visual;
    c->cmap = XCreateColormap(c->d, c->root, c->vis, AllocNone);

    c->attrs.override_redirect = True;
    c->attrs.colormap = c->cmap;
    c->attrs.background_pixmap = None;
    c->attrs.border_pixel = 0;
    c->attrs.bit_gravity = StaticGravity;
    c->attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask;


    c->w = XCreateWindow(c->d, c->root, 0 + (mw - c->width) / 2, 0 + (mh - c->height) / 2, c->width, c->height, 0, c->vi->depth,
                      InputOutput, c->vis,
                      CWOverrideRedirect |CWColormap | CWBackPixmap | CWBorderPixel | CWBitGravity | CWEventMask, &c->attrs);

    XStoreName(c->d, c->w, "app");

    c->wmdeletewin = XInternAtom(c->d, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(c->d, c->w, &c->wmdeletewin, 1);

    c->wm_protocols  = XInternAtom(c->d, "WM_PROTOCOLS", False);
    c->wm_state      = XInternAtom(c->d, "_NET_WM_STATE", False);
    c->wm_fullscreen = XInternAtom(c->d, "_NET_WM_STATE_FULLSCREEN", False);
    c->wm_type       = XInternAtom(c->d, "_NET_WM_WINDOW_TYPE", False);
    c->wm_type_utility = XInternAtom(c->d, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    c->netwmname = XInternAtom(c->d, "_NET_WM_NAME", False);
    c->wmnameutf8 = XInternAtom(c->d, "UTF8_STRING", False);
    c->wmname	= XInternAtom(c->d, "WM_NAME", False);

    XChangeProperty(c->d, c->w, c->netwmname, c->wmnameutf8, 8, 
                PropModeReplace, (const unsigned char *)"xwall", strlen("xwall"));

    XChangeProperty(c->d, c->w, c->wmname, c->wmnameutf8, 8, 
                PropModeReplace, (const unsigned char *)"xwall", strlen("xwall"));

    c->glc = glXCreateContext(c->d, c->vi, NULL, GL_TRUE);
    if (!c->glc)
        die("cannot create GLX context");

    XMapWindow(c->d, c->w);

    XEvent ev;
    do {
	XNextEvent(c->d, &ev);
    } while (ev.type != MapNotify || ev.xmap.window != c->w);


    glXMakeCurrent(c->d, c->w, c->glc);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 1);

    c->font = XftFontOpenName(c->d, c->scr, "monospace-12");
    if (!c->font)
        die("cannot load font");

    XRenderColor rc = { 0xffff, 0xffff, 0xffff, 0xffff };
    XftColorAllocValue(c->d, c->vis, c->cmap, &rc, &c->xftcolor);

    resize(c, c->width, c->height);
}

void nav_left(Client *c) {
    int cols = grid_cols(c);
    int col = selected % cols;
    if (col > 0) {
        selected--;
        dirty = 1;
    }
}

void nav_right(Client *c) {
    int cols = grid_cols(c);
    int col = selected % cols;
    if (col < cols - 1 && selected + 1 < item_count) {
        selected++;
        dirty = 1;
    }
}

void nav_up(Client *c) {
    int cols = grid_cols(c);
    int row = selected / cols;
    if (row > 0) {
        selected -= cols;
        dirty = 1;
    }
}

void nav_down(Client *c) {
    int cols = grid_cols(c);
    if (selected + cols < item_count) {
        selected += cols;
        dirty = 1;
    }
}

void nav_quit(Client *c) {
    c->running = 0;
}

void nav_select(Client *c) {
    (void)c;
    if (item_count == 0)
        return;

    const char *path = items[selected].path;

    char cmd[2048];
    int n = snprintf(cmd, sizeof(cmd), CMD, path);
    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        fprintf(stderr, "command too long, not running\n");
        return;
    }

    int ret = system(cmd);
    if (ret != 0)
        fprintf(stderr, "set_wallpaper command exited %d\n", ret);
    nav_quit(c);
}

void handle_key(Client *c, XKeyEvent *e) {
    KeySym ks = XLookupKeysym(e, 0);

    unsigned int state = e->state &
        (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);

    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
        if (ks == keys[i].keysym && state == keys[i].mod) {
            keys[i].func(c);
            return;
        }
    }
}

void draw_rect(int x, int y, int w, int h, float r, float g, float b, float a) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
        glVertex2i(x, y);
        glVertex2i(x + w, y);
        glVertex2i(x + w, y + h);
        glVertex2i(x, y + h);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

GLuint make_text_texture(Client *c, const char *text, XGlyphInfo *extents_out) {
    XGlyphInfo ext;
    XftTextExtentsUtf8(c->d, c->font, (const FcChar8 *)text, strlen(text), &ext);

    int w = ext.width > 0 ? ext.width : 1;
    int h = ext.height > 0 ? ext.height : 1;

    Pixmap pm = XCreatePixmap(c->d, c->root, w, h, c->vi->depth);
    GC gc = XCreateGC(c->d, pm, 0, NULL);
    XSetForeground(c->d, gc, 0);
    XFillRectangle(c->d, pm, gc, 0, 0, w, h);

    XftDraw *xd = XftDrawCreate(c->d, pm, c->vis, c->cmap);
    XftDrawStringUtf8(xd, &c->xftcolor, c->font, ext.x, ext.y, (const FcChar8 *)text, strlen(text));

    XImage *img = XGetImage(c->d, pm, 0, 0, w, h, AllPlanes, ZPixmap);

    unsigned char *rgba = malloc((size_t)w * h * 4);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            unsigned long px = XGetPixel(img, x, y);
            unsigned char lum = (px >> 16) & 0xff;
            int i = (y * w + x) * 4;
            rgba[i + 0] = 255;
            rgba[i + 1] = 255;
            rgba[i + 2] = 255;
            rgba[i + 3] = lum;
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    free(rgba);
    XDestroyImage(img);
    XftDrawDestroy(xd);
    XFreeGC(c->d, gc);
    XFreePixmap(c->d, pm);

    if (extents_out)
        *extents_out = ext;

    return tex;
}

GLuint load_image_texture(Client *c, const char *path, int *out_w, int *out_h) {
    Imlib_Image im = imlib_load_image(path);
    if (!im)
        return 0;

    imlib_context_set_display(c->d);
    imlib_context_set_visual(c->vis);
    imlib_context_set_colormap(c->cmap);
    imlib_context_set_drawable(c->w);
    imlib_context_set_image(im);

    int w = imlib_image_get_width();
    int h = imlib_image_get_height();
    DATA32 *data = imlib_image_get_data_for_reading_only();

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, data);

    imlib_free_image();

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;

    return tex;
}

static unsigned char *decode_thumbnail(const char *path, int *out_w, int *out_h, int *out_src_w, int *out_src_h) {
    Imlib_Image im = imlib_load_image(path);
    if (!im)
        return NULL;

    imlib_context_set_image(im);
    int src_w = imlib_image_get_width();
    int src_h = imlib_image_get_height();

    if (out_src_w) *out_src_w = src_w;
    if (out_src_h) *out_src_h = src_h;

    float scale_w = (float)THUMB_MAX / src_w;
    float scale_h = (float)THUMB_MAX / src_h;
    float scale = scale_w < scale_h ? scale_w : scale_h;

    int w = (int)(src_w * scale);
    int h = (int)(src_h * scale);
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    Imlib_Image thumb = imlib_create_cropped_scaled_image(0, 0, src_w, src_h, w, h);
    imlib_free_image();

    imlib_context_set_image(thumb);
    DATA32 *data = imlib_image_get_data_for_reading_only();

    size_t n = (size_t)w * h * 4;
    unsigned char *buf = malloc(n);
    memcpy(buf, data, n);

    imlib_free_image();

    *out_w = w;
    *out_h = h;
    return buf;
}

static void *loader_main(void *arg) {
    (void)arg;
    char dir[1024];
    if (!get_dir(dir, sizeof(dir))) {
        loader_done = 1;
        return NULL;
    }

    int n;
    char **files = get_files_from_dir(dir, &n);
    if (!files) {
        loader_done = 1;
        return NULL;
    }

    items = malloc(n * sizeof(WallItem));
    item_count = 0;

    for (int i = 0; i < n; i++) {
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);

        int w, h, src_w, src_h;
        unsigned char *px = decode_thumbnail(path, &w, &h, &src_w, &src_h);
        free(files[i]);
        if (!px)
            continue;

        WallItem *it = &items[item_count];
        it->path = strdup(path);
        it->pixels = px;
        it->w = w;
        it->h = h;
        it->src_w = src_w;
        it->src_h = src_h;
        it->tex = 0;
        it->uploaded = 0;
        __sync_synchronize();
        it->ready = 1;
        item_count++;
    }
    free(files);
    loader_done = 1;
    return NULL;
}

void load_wallpapers_async(Client *c) {
    (void)c;
    pthread_create(&loader_thread, NULL, loader_main, NULL);
}

void upload_pending_textures(Client *c) {
    for (int i = 0; i < item_count; i++) {
        WallItem *it = &items[i];
        if (it->ready && !it->uploaded) {
            glGenTextures(1, &it->tex);
            glBindTexture(GL_TEXTURE_2D, it->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, it->w, it->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, it->pixels);

            free(it->pixels);
            it->pixels = NULL;

	    const char *name = strrchr(items[i].path, '/');
	    name = name ? name + 1 : items[i].path;
	    
	    char local_name[256]; 
	    snprintf(local_name, sizeof(local_name), "%s", name);
	    
	    if (has_ext(name, ".png") || has_ext(name, ".jpg") || has_ext(name, ".jpeg")) {
		char *d = strrchr(local_name, '.');
		if (d) *d = '\0';

		const char *target = ".";

		char *match = strstr(local_name, target);
		if (match) {
		    size_t target_len = strlen(target);
		    
		    memmove(match + strlen(local_name), match + target_len, strlen(match + target_len) + 1);
		    memcpy(match, local_name, strlen(local_name));
		}
	    }

            XGlyphInfo ext;
            it->label_tex = make_text_texture(c, local_name, &ext);
            it->label_w = ext.width;
            it->label_h = ext.height;

            it->uploaded = 1;
            dirty = 1;
        }
    }
}

const char *get_home(void) {
    const char *home = getenv("HOME");
    if (home)
        return home;
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_dir : NULL;
}

const char *get_dir(char *buf, size_t bufz) {
    const char *home = get_home();
    snprintf(buf, bufz, "%s/%s", home, wdir);
    return buf;
}

char **get_files_from_dir(const char *dir_path, int *out_count) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("Opendir failed");
        *out_count = 0;
        return NULL;
    }

    int capacity = 20;
    int count = 0;

    char **files = malloc(capacity * sizeof(char *));
    if (!files) {
        closedir(dir);
        return NULL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        if (count >= capacity) {
            capacity *= 2;
            char **temp = realloc(files, capacity * sizeof(char *));
            if (!temp) {
                for (int i = 0; i < count; i++) free(files[i]);
                free(files);
                closedir(dir);
                return NULL;
            }
            files = temp;
        }

        files[count] = strdup(entry->d_name);
        count++;
    }

    closedir(dir);
    *out_count = count;
    return files;
}

void draw_quad(GLuint tex, int x, int y, int w, int h) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(x, y);
        glTexCoord2f(1, 0); glVertex2i(x + w, y);
        glTexCoord2f(1, 1); glVertex2i(x + w, y + h);
        glTexCoord2f(0, 1); glVertex2i(x, y + h);
    glEnd();
}

int has_ext(const char *name, const char *ext) {
    size_t nlen = strlen(name);
    size_t elen = strlen(ext);
    if (nlen < elen)
        return 0;
    return strcasecmp(name + nlen - elen, ext) == 0;
}

void draw_wallpapers(Client *c) {
    int cols = grid_cols(c);
    int per_page = items_page(c);
    int page = selected / per_page;
    int page_start = page * per_page;
    int page_end = page_start + per_page;
    if (page_end > item_count) page_end = item_count;

    for (int i = page_start; i < page_end; i++) {
        if (!items[i].uploaded)
            continue;

	int local = i - page_start;
	
        int x = (local % cols) * CELL_W + CELL_PAD_X / 2;
        int y = (local / cols) * CELL_H + CELL_PAD_Y / 2;

        int lw = items[i].label_w;
        int lh = items[i].label_h;
        if (lw > items[i].w) {
            float scale = (float)items[i].w / lw;
            lw = items[i].w;
            lh = (int)(lh * scale);
        }

        glColor4f(1, 1, 1, 1);
        draw_quad(items[i].tex, x, y, items[i].w, items[i].h);

        if (i == selected)
            draw_highlight(x, y, items[i].w, items[i].h);

        glColor4f(1, 1, 1, 1);
        draw_quad(items[i].label_tex, x, y + items[i].h + 4, lw, lh);
    }
}

static void draw_highlight(int x, int y, int w, int h) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(1.0f, 0.8f, 0.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2i(x - 2, y - 2);
        glVertex2i(x + w + 2, y - 2);
        glVertex2i(x + w + 2, y + h + 2);
        glVertex2i(x - 2, y + h + 2);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

void draw_text(Client *c, GLuint *ut, const char *msg, int *w, int *h) {
    if (*ut) glDeleteTextures(1, ut);
    XGlyphInfo ext;
    *ut = make_text_texture(c, msg, &ext);
    
    if (w) *w = ext.width;
    if (h) *h = ext.height;
}

long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;
    return (long)st.st_size;
}

void format_size(long bytes, char *buf, size_t bufsz) {
    if (bytes < 0) {
        snprintf(buf, bufsz, "unknown");
    } else if (bytes < 1024) {
        snprintf(buf, bufsz, "%ld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, bufsz, "%.1f KB", bytes / 1024.0);
    } else {
        snprintf(buf, bufsz, "%.1f MB", bytes / (1024.0 * 1024.0));
    }
}

void get_imgd(char *dmw, size_t dwmz, char *dmh, size_t dhmz) {
    snprintf(dmw, dwmz, "%d", items[selected].src_w);
    snprintf(dmh, dhmz, "%d", items[selected].src_h);
}

const char *get_ext(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path)
        return "";
    return dot + 1;
}

char *get_namef() {
    const char *name = strrchr(items[selected].path, '/');
    name = name ? name + 1 : items[selected].path;

    static char ln[256];
    snprintf(ln, sizeof(ln), "%s", name);
    
    if (has_ext(name, ".png") || has_ext(name, ".jpg") || has_ext(name, ".jpeg")) {
        char *d = strrchr(ln, '.');
        if (d) *d = '\0';
    
        const char *target = ".";
    
        char *match = strstr(ln, target);
        if (match) {
            size_t target_len = strlen(target);
            
            memmove(match + strlen(ln), match + target_len, strlen(match + target_len) + 1);
            memcpy(match, ln, strlen(ln));
        }
    }

    return ln;
}

void draw_hud(Client *c) {
    char fs[128];
    char titleimg[256];
    char size[128];
    char ftex[128];

    int y = c->height - 30 - 10; 

    snprintf(fs, sizeof(fs), "%d/%d", selected, item_count);
    snprintf(titleimg, sizeof(titleimg), "%s", get_namef());
    snprintf(ftex, sizeof(ftex), "%s", get_ext(items[selected].path));
    format_size(get_file_size(items[selected].path), size, sizeof(size));

    draw_text(c, &fis, fs, &fs_w, &fs_h);
    draw_text(c, &tim, titleimg, &tim_w, &tim_h);
    draw_text(c, &sz, size, &sz_w, &sz_h);
    draw_text(c, &fm, ftex, &fm_w, &fm_h);

    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    
    if (FS)
	draw_quad(fis, 10, y, fs_w, fs_h);

    if (TITLE)
	draw_quad(tim, c->width - tim_w - 10, y, tim_w, tim_h);

    if (FORMAT) {
	if (SIZE) { draw_quad(fm, (c->width / 2) - (sz_w / 2) + sz_w, c->height - fm_h - 25, fm_w, fm_h); }
	else if (FS || SIZE) { draw_quad(fm, (c->width / 2) - (sz_w / 2) + sz_w, c->height - fm_h - 25, fm_w, fm_h); }
	else if (!FS) { draw_quad(fm, 10, c->height - fm_h - 25, fm_w, fm_h); }
	else { draw_quad(fm, (c->width / 2) - (sz_w / 2) + sz_w, c->height - fm_h - 25, fm_w, fm_h); }
    }

    if (SIZE) {
	if (FORMAT) { draw_quad(sz, (c->width / 2) - (sz_w / 2) - fm_w, y, sz_w, sz_h); }
	else { draw_quad(sz, (c->width / 2) - (sz_w / 2), y, sz_w, sz_h); }
    }
}

void run(Client *c) {
    while (c->running) {
        upload_pending_textures(c);

        if (!loader_done) {
            struct timeval tv = { 0, 16000 };
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(ConnectionNumber(c->d), &fds);
            select(ConnectionNumber(c->d) + 1, &fds, NULL, NULL, &tv);
        }

        while (XPending(c->d)) {
            XEvent e;
            XNextEvent(c->d, &e);
            switch (e.type) {
            case ConfigureNotify:
                if (e.xconfigure.width != c->width || e.xconfigure.height != c->height) {
                    resize(c, e.xconfigure.width, e.xconfigure.height);
                    dirty = 1;
                }
                break;
            case Expose:
                dirty = 1;
                break;
            case ClientMessage:
                if ((Atom)e.xclient.data.l[0] == c->wmdeletewin)
                    c->running = 0;
                break;
            case KeyPress:
                handle_key(c, &e.xkey);
                dirty = 1;
                break;
            }
        }

        if (!c->running)
            break;

        if (loader_done && !dirty) {
            XEvent e;
            XNextEvent(c->d, &e);
            XPutBackEvent(c->d, &e);
            continue;
        }

        if (dirty) {
            glClear(GL_COLOR_BUFFER_BIT);
            draw_wallpapers(c);

	    int y = c->height - 45; 

	    draw_rect(0, y, c->width, 25, 1, 1, 1, 1);
	    draw_hud(c);
	    
            glXSwapBuffers(c->d, c->w);
            dirty = 0;
        }
    }
}

void cleanup(Client *c) {
    for (int i = 0; i < item_count; i++) {
	if (items[i].tex) glDeleteTextures(1, &items[i].tex);
	if (items[i].label_tex) glDeleteTextures(1, &items[i].label_tex);
	free(items[i].pixels);
	free(items[i].path);
    }
    free(items);

    XftColorFree(c->d, c->vis, c->cmap, &c->xftcolor);
    XftFontClose(c->d, c->font);
    glXMakeCurrent(c->d, None, NULL);
    glXDestroyContext(c->d, c->glc);
    XDestroyWindow(c->d, c->w);
    XFreeColormap(c->d, c->cmap);
    XCloseDisplay(c->d);
}

int main(void) {
    Client c = {0};
    initx(&c);
    load_wallpapers_async(&c);
    run(&c);
    pthread_join(loader_thread, NULL);
    cleanup(&c);
    return 0;
}
