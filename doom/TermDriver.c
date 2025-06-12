
#include "DrawFunctions.h"

#include "math.c"
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WIDTH 200
#define HEIGHT 200
static char global_window_name[256] = {};
void CNFGGetDimensions(short *x, short *y) {
  *x = WIDTH;
  *y = HEIGHT;
}

void CNFGSetupFullscreen(const char *window_name, int screen_no) {}

char sys_char();
void CNFGSetup(const char *window_name, int w, int h) {
  // width = w;
  // height = h;

  strncpy(global_window_name, window_name, 255);
}

char key = '\0';
void CNFGHandleInput() {
  if (key != '\0') {
    HandleKey(key, 1);
    key = '\0';
  }

  key = sys_char();
  if (key != 0) {
    printf("key down %i\n", key);
    HandleKey(key, 0);
  }
  // TODO: create some semblance of working in terminal
  // HandleKey(XLookupKeysym(&report.xkey, 0), bKeyDirection);
  // HandleButton(report.xbutton.x, report.xbutton.y, report.xbutton.button,
  // HandleMotion(report.xmotion.x, report.xmotion.y, ButtonsDown >> 1);
}

// RENDERING CODE
#define BUFFER_SIZE WIDTH *HEIGHT
uint32_t back_buffer[BUFFER_SIZE];
uint32_t drawing_buffer[BUFFER_SIZE];
uint32_t get_index(short x, short y) {
  uint32_t bounded_x = min(x, WIDTH - 1);
  uint32_t bounded_y = min(y, HEIGHT - 1);
  return bounded_x + bounded_y * WIDTH;
}

void sys_savestate();
void sys_printint(int i);
void sys_write(const char *str, size_t len);
void CNFGUpdateScreenWithBitmap(unsigned long *data, int w, int h) {
  sys_savestate();
  size_t cursor = 0;
  for (int y = 0; y < h; y += 10) {
    for (int x = 0; x < w; x += 5) {
      uint32_t base_color = data[x + y * w];
      unsigned char red = base_color & 0xFF; // red and blue channels swapped!!
      unsigned char grn = (base_color >> 8) & 0xFF;
      unsigned char blu = (base_color >> 16) & 0xFF;
      // [38;2;47;55;31m█
      sys_write("\x1b[38;2;", 7);
      sys_printint(blu);
      sys_write(";", 1);
      sys_printint(grn);
      sys_write(";", 1);
      sys_printint(red);
      sys_write("m█\x1b[0m", 8) ;
    }
    sys_write("\n", 1);
  }
}

static uint32_t saved_color = 0;
uint32_t CNFGColor(uint32_t RGB) {
  unsigned char red = RGB & 0xFF;
  unsigned char grn = (RGB >> 8) & 0xFF;
  unsigned char blu = (RGB >> 16) & 0xFF;
  CNFGLastColor = RGB;
  unsigned long color = (red << 16) | (grn << 8) | (blu);
  saved_color = color;
  // XSetForeground(CNFGDisplay, CNFGGC, color);
  return color;
}

void CNFGClearFrame() {
  memset(back_buffer, CNFGColor(CNFGBGColor), WIDTH * HEIGHT);
}

void CNFGSwapBuffers() { memcpy(drawing_buffer, back_buffer, BUFFER_SIZE); }

void CNFGTackSegment(short x1, short y1, short x2, short y2) {
  printf("implement me!\n");
}

void CNFGTackPixel(short x1, short y1) {
  back_buffer[get_index(x1, y1)] = saved_color;
}

void CNFGTackRectangle(short x1, short y1, short x2, short y2) {
  printf("implement me!\n");
  // XFillRectangle(CNFGDisplay, CNFGPixmap, CNFGGC, x1, y1, x2 - x1, y2 - y1);
}

void CNFGTackPoly(RDPoint *points, int verts) {
  printf("implement me!\n");
  // XFillPolygon(CNFGDisplay, CNFGPixmap, CNFGGC, (XPoint *)points, 3, Convex,
  //              CoordModeOrigin);
}
