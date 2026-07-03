<div align="center">
  <h1>X WallPaper</h1>
    <div>
      <p align="left">X Wallpaper is a wallpaper picker for X11, no dependencies, no extra commands, no for wayland, only select the directory, select the image and select the command or SH file to execute.</p>
    </div>
</div> 

<div align="center">
  <h1>Keybinds</h1>

| combo        | action     |
| -------------| -----------|
| `Arrow Up`   |  Move to the up index    |
| `Down Up`    |  Move to the down index  |
| `Left Up`    |  Move to the left index  |
| `Right Up`   |  Move to the right index |
| `k`          |  Move to the up index    |
| `j`          |  Move to the down index  |
| `h`          |  Move to the left index  |
| `l`          |  Move to the right index |
| `q`          |  Leave the selector      |
| `CTRL` + `C` |  Leave the selector      |

</div>

<div align="center">
  <h1>Install & Config</h1>
  <p align="left">You can install this with</p>
</div>

  ```
  git clone https://github.com/esnokum-dacom/xwallpaper
  ```

<div align="center">
  <p align="left">And then make the source</p>
</div>

  ```
  sudo make clean install
  ```

<div align="center">
  <p align="left">To configure the path of the images you can go to</p>
</div>

```c
/* config.def.h */
static const char *wdir = "Wallpapers";
```

and select the directory in your home path


<div align="center">
  <p align="left">To configure the command, in config.def.h you can select the command to execute (If is a file you need to make it executable with)<p>
</div>

```
chmod +x script/.sh
```

and then

```
/* config.def.h */
#define CMD "echo \"%s\""
```

where `\"%s\"` is the name of the file or the path, see like $1 in bash.

<div align="center">
  <h1>Dependencies</h1>
</div>

- Xlib (obiusly)
- Xinerama
- Xft
- Gl/GlX
- imlib2

Thank you so much
