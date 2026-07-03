#include <X11/Xlib.h>
#include <X11/X.h>

/* define the image thumnail size */
#define THUMB_MAX 150

/* Status bar */
#define TITLE 1
#define FORMAT 1
#define SIZE 1
#define FS 1

/* Wallpaper script */
#define CMD "~/wall.sh \"%s\""

static const char *wdir = "Wallpapers";

#define MODKEY ControlMask
static const Key keys[] = {
    { 0,	XK_h,             nav_left   },
    { 0,	XK_Left,          nav_left   },
    { 0,	XK_l,             nav_right  },
    { 0,	XK_Right,         nav_right  },
    { 0,	XK_k,             nav_up     },
    { 0,	XK_Up,            nav_up     },
    { 0,	XK_j,             nav_down   },
    { 0,	XK_Down,          nav_down   },
    { 0,	XK_Return,        nav_select },
    { 0,	XK_q,             nav_quit   },
    { MODKEY,	XK_c,		  nav_quit   },
};
