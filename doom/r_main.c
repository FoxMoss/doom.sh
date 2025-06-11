// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//
//-----------------------------------------------------------------------------

// static const char rcsid[] = "$Id: r_main.c,v 1.5 1997/02/03 22:45:12 b1 Exp
// $";

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "d_net.h"
#include "doomdef.h"

#include "m_bbox.h"

#include "r_local.h"
#include "r_sky.h"

// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW 2048

int viewangleoffset;

// increment every time a check is made
short validcount = 1;

lighttable_t *fixedcolormap;
extern lighttable_t **walllights;

int centerx;
int centery;

fixed_t centerxfrac;
fixed_t centeryfrac;
fixed_t projection;

// just for profiling purposes
int framecount;

int sscount;
int linecount;
int loopcount;

fixed_t viewx;
fixed_t viewy;
fixed_t viewz;

angle_t viewangle;

fixed_t viewcos;
fixed_t viewsin;

player_t *viewplayer;

// 0 = high, 1 = low
int detailshift;

//
// precalculated math tables
//
angle_t clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.
#ifdef GENERATE_BAKED
int viewangletox[FINEANGLES / 2];
// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t xtoviewangle[SCREENWIDTH + 1];
#else
extern const int viewangletox[FINEANGLES / 2];
extern const angle_t xtoviewangle[SCREENWIDTH + 1];
#endif

// UNUSED.
// The finetangentgent[angle+FINEANGLES/4] table
// holds the fixed_t tangent values for view angles,
// ranging from MININT to 0 to MAXINT.
// fixed_t		finetangent[FINEANGLES/2];

// fixed_t		finesine[5*FINEANGLES/4];
const fixed_t *finecosine = &finesine[FINEANGLES / 4];

lighttable_t *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t *scalelightfixed[MAXLIGHTSCALE];
lighttable_t *zlight[LIGHTLEVELS][MAXLIGHTZ];

// bumped light from gun blasts
int extralight;

void (*colfunc)(void);
void (*basecolfunc)(void);
void (*fuzzcolfunc)(void);
void (*transcolfunc)(void);
void (*spanfunc)(void);

//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
void R_AddPointToBox(int x, int y, fixed_t *box) {
  if (x < box[BOXLEFT])
    box[BOXLEFT] = x;
  if (x > box[BOXRIGHT])
    box[BOXRIGHT] = x;
  if (y < box[BOXBOTTOM])
    box[BOXBOTTOM] = y;
  if (y > box[BOXTOP])
    box[BOXTOP] = y;
}

//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int R_PointOnSide(fixed_t x, fixed_t y, node_t *node) {
  fixed_t dx;
  fixed_t dy;
  fixed_t left;
  fixed_t right;

  if (!node->dx) {
    if (x <= node->x)
      return node->dy > 0;

    return node->dy < 0;
  }
  if (!node->dy) {
    if (y <= node->y)
      return node->dx < 0;

    return node->dx > 0;
  }

  dx = (x - node->x);
  dy = (y - node->y);

  // Try to quickly decide by looking at sign bits.
  if ((node->dy ^ node->dx ^ dx ^ dy) & 0x80000000) {
    if ((node->dy ^ dx) & 0x80000000) {
      // (left is negative)
      return 1;
    }
    return 0;
  }

  left = FixedMul(node->dy >> FRACBITS, dx);
  right = FixedMul(dy, node->dx >> FRACBITS);

  if (right < left) {
    // front side
    return 0;
  }
  // back side
  return 1;
}

int R_PointOnSegSide(fixed_t x, fixed_t y, seg_t *line) {
  fixed_t lx;
  fixed_t ly;
  fixed_t ldx;
  fixed_t ldy;
  fixed_t dx;
  fixed_t dy;
  fixed_t left;
  fixed_t right;

  lx = line->v1->x;
  ly = line->v1->y;

  ldx = line->v2->x - lx;
  ldy = line->v2->y - ly;

  if (!ldx) {
    if (x <= lx)
      return ldy > 0;

    return ldy < 0;
  }
  if (!ldy) {
    if (y <= ly)
      return ldx < 0;

    return ldx > 0;
  }

  dx = (x - lx);
  dy = (y - ly);

  // Try to quickly decide by looking at sign bits.
  if ((ldy ^ ldx ^ dx ^ dy) & 0x80000000) {
    if ((ldy ^ dx) & 0x80000000) {
      // (left is negative)
      return 1;
    }
    return 0;
  }

  left = FixedMul(ldy >> FRACBITS, dx);
  right = FixedMul(dy, ldx >> FRACBITS);

  if (right < left) {
    // front side
    return 0;
  }
  // back side
  return 1;
}

//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.

//

angle_t R_PointToAngle(fixed_t x, fixed_t y) {
  x -= viewx;
  y -= viewy;

  if ((!x) && (!y))
    return 0;

  if (x >= 0) {
    // x >=0
    if (y >= 0) {
      // y>= 0

      if (x > y) {
        // octant 0
        return tantoangle[SlopeDiv(y, x)];
      } else {
        // octant 1
        return ANG90 - 1 - tantoangle[SlopeDiv(x, y)];
      }
    } else {
      // y<0
      y = -y;

      if (x > y) {
        // octant 8
        return -tantoangle[SlopeDiv(y, x)];
      } else {
        // octant 7
        return ANG270 + tantoangle[SlopeDiv(x, y)];
      }
    }
  } else {
    // x<0
    x = -x;

    if (y >= 0) {
      // y>= 0
      if (x > y) {
        // octant 3
        return ANG180 - 1 - tantoangle[SlopeDiv(y, x)];
      } else {
        // octant 2
        return ANG90 + tantoangle[SlopeDiv(x, y)];
      }
    } else {
      // y<0
      y = -y;

      if (x > y) {
        // octant 4
        return ANG180 + tantoangle[SlopeDiv(y, x)];
      } else {
        // octant 5
        return ANG270 - 1 - tantoangle[SlopeDiv(x, y)];
      }
    }
  }
  return 0;
}

angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2) {
  viewx = x1;
  viewy = y1;

  return R_PointToAngle(x2, y2);
}

fixed_t R_PointToDist(fixed_t x, fixed_t y) {
  int angle;
  fixed_t dx;
  fixed_t dy;
  fixed_t temp;
  fixed_t dist;

  dx = abs(x - viewx);
  dy = abs(y - viewy);

  if (dy > dx) {
    temp = dx;
    dx = dy;
    dy = temp;
  }

  angle = (tantoangle[FixedDiv(dy, dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;

  // use as cosine
  dist = FixedDiv(dx, finesine[angle]);

  return dist;
}

//
// R_InitPointToAngle
//
void R_InitPointToAngle(void) {
  // UNUSED - now getting from tables.c
#if 0
    int	i;
    long	t;
    float	f;
//
// slope (tangent) to angle lookup
//
    for (i=0 ; i<=SLOPERANGE ; i++)
    {
	f = atan( (float)i/SLOPERANGE )/(3.141592657*2);
	t = 0xffffffff*f;
	tantoangle[i] = t;
    }
#endif
}

//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
fixed_t R_ScaleFromGlobalAngle(angle_t visangle) {
  fixed_t scale;
  int anglea;
  int angleb;
  int sinea;
  int sineb;
  fixed_t num;
  int den;

  // UNUSED
#if 0
{
    fixed_t		dist;
    fixed_t		z;
    fixed_t		sinv;
    fixed_t		cosv;
	
    sinv = finesine[(visangle-rw_normalangle)>>ANGLETOFINESHIFT];	
    dist = FixedDiv (rw_distance, sinv);
    cosv = finecosine[(viewangle-visangle)>>ANGLETOFINESHIFT];
    z = abs(FixedMul (dist, cosv));
    scale = FixedDiv(projection, z);
    return scale;
}
#endif

  anglea = ANG90 + (visangle - viewangle);
  angleb = ANG90 + (visangle - rw_normalangle);

  // both sines are allways positive
  sinea = finesine[anglea >> ANGLETOFINESHIFT];
  sineb = finesine[angleb >> ANGLETOFINESHIFT];
  num = FixedMul(projection, sineb) << detailshift;
  den = FixedMul(rw_distance, sinea);

  if (den > num >> 16) {
    scale = FixedDiv(num, den);

    if (scale > 64 * FRACUNIT)
      scale = 64 * FRACUNIT;
    else if (scale < 256)
      scale = 256;
  } else
    scale = 64 * FRACUNIT;

  return scale;
}

//
// R_InitTables
//
void R_InitTables(void) {
  // UNUSED: now getting from tables.c
#if 0
    int		i;
    float	a;
    float	fv;
    int		t;
    
    // viewangle tangent table
    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
	a = (i-FINEANGLES/4+0.5)*PI*2/FINEANGLES;
	fv = FRACUNIT*tan (a);
	t = fv;
	finetangent[i] = t;
    }
    
    // finesine table
    for (i=0 ; i<5*FINEANGLES/4 ; i++)
    {
	// OPTIMIZE: mirror...
	a = (i+0.5)*PI*2/FINEANGLES;
	t = FRACUNIT*sin (a);
	finesine[i] = t;
    }
#endif
}

//
// R_InitTextureMapping
//
void R_InitTextureMapping(void) {
  int i;
  int x;
  int t;
  fixed_t focallength;

  // Use tangent table to generate viewangletox:
  //  viewangletox will give the next greatest x
  //  after the view angle.
  //
  // Calc focallength
  //  so FIELDOFVIEW angles covers SCREENWIDTH.
  focallength =
      FixedDiv(centerxfrac, finetangent[FINEANGLES / 4 + FIELDOFVIEW / 2]);

#ifdef GENERATE_BAKED

  for (i = 0; i < FINEANGLES / 2; i++) {
    if (finetangent[i] > FRACUNIT * 2)
      t = -1;
    else if (finetangent[i] < -FRACUNIT * 2)
      t = viewwidth + 1;
    else {
      t = FixedMul(finetangent[i], focallength);
      t = (centerxfrac - t + FRACUNIT - 1) >> FRACBITS;

      if (t < -1)
        t = -1;
      else if (t > viewwidth + 1)
        t = viewwidth + 1;
    }
    viewangletox[i] = t;
  }

  // Scan viewangletox[] to generate xtoviewangle[]:
  //  xtoviewangle will give the smallest view angle
  //  that maps to x.
  for (x = 0; x <= viewwidth; x++) {
    i = 0;
    while (viewangletox[i] > x)
      i++;
    xtoviewangle[x] = (i << ANGLETOFINESHIFT) - ANG90;
  }

  // Take out the fencepost cases from viewangletox.
  for (i = 0; i < FINEANGLES / 2; i++) {
    t = FixedMul(finetangent[i], focallength);
    t = centerx - t;

    if (viewangletox[i] == -1)
      viewangletox[i] = 0;
    else if (viewangletox[i] == viewwidth + 1)
      viewangletox[i] = viewwidth;
  }
#endif
  clipangle = xtoviewangle[0];
}

//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//
#define DISTMAP 2

void R_InitLightTables(void) {
  int i;
  int j;
  int level;
  int startmap;
  int scale;

  // Calculate the light levels to use
  //  for each level / distance combination.
  for (i = 0; i < LIGHTLEVELS; i++) {
    startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
    for (j = 0; j < MAXLIGHTZ; j++) {
      scale = FixedDiv((SCREENWIDTH / 2 * FRACUNIT), (j + 1) << LIGHTZSHIFT);
      scale >>= LIGHTSCALESHIFT;
      level = startmap - scale / DISTMAP;

      if (level < 0)
        level = 0;

      if (level >= NUMCOLORMAPS)
        level = NUMCOLORMAPS - 1;

      zlight[i][j] = colormaps + level * 256;
    }
  }
}

//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean setsizeneeded;
int setblocks;
int setdetail;

void R_SetViewSize(int blocks, int detail) {
  setsizeneeded = true;
  setblocks = blocks;
  setdetail = detail;
}

//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize(void) {
  fixed_t cosadj;
  fixed_t dy;
  int i;
  int j;
  int level;
  int startmap;

  setsizeneeded = false;

  if (setblocks == 11) {
    scaledviewwidth = SCREENWIDTH;
    viewheight = SCREENHEIGHT;
  } else {
    scaledviewwidth = setblocks * 32;
    viewheight = (setblocks * 168 / 10) & ~7;
  }

  detailshift = setdetail;
  viewwidth = scaledviewwidth >> detailshift;

  centery = viewheight / 2;
  centerx = viewwidth / 2;
  centerxfrac = centerx << FRACBITS;
  centeryfrac = centery << FRACBITS;
  projection = centerxfrac;

  if (!detailshift) {
    colfunc = basecolfunc = R_DrawColumn;
    fuzzcolfunc = R_DrawFuzzColumn;
    transcolfunc = R_DrawTranslatedColumn;
    spanfunc = R_DrawSpan;
  } else {
    colfunc = basecolfunc = R_DrawColumnLow;
    fuzzcolfunc = R_DrawFuzzColumn;
    transcolfunc = R_DrawTranslatedColumn;
    spanfunc = R_DrawSpanLow;
  }

  R_InitBuffer(scaledviewwidth, viewheight);

  R_InitTextureMapping();

  // psprite scales
  pspritescale = FRACUNIT * viewwidth / SCREENWIDTH;
  pspriteiscale = FRACUNIT * SCREENWIDTH / viewwidth;

  // thing clipping
  for (i = 0; i < viewwidth; i++)
    screenheightarray[i] = viewheight;

  // planes
  yslope[0] = 125577;
  yslope[1] = 127100;
  yslope[2] = 128659;
  yslope[3] = 130257;
  yslope[4] = 131896;
  yslope[5] = 133576;
  yslope[6] = 135300;
  yslope[7] = 137068;
  yslope[8] = 138884;
  yslope[9] = 140748;
  yslope[10] = 142663;
  yslope[11] = 144631;
  yslope[12] = 146653;
  yslope[13] = 148734;
  yslope[14] = 150874;
  yslope[15] = 153076;
  yslope[16] = 155344;
  yslope[17] = 157680;
  yslope[18] = 160087;
  yslope[19] = 162569;
  yslope[20] = 165130;
  yslope[21] = 167772;
  yslope[22] = 170500;
  yslope[23] = 173318;
  yslope[24] = 176231;
  yslope[25] = 179243;
  yslope[26] = 182361;
  yslope[27] = 185588;
  yslope[28] = 188932;
  yslope[29] = 192399;
  yslope[30] = 195995;
  yslope[31] = 199728;
  yslope[32] = 203606;
  yslope[33] = 207638;
  yslope[34] = 211833;
  yslope[35] = 216201;
  yslope[36] = 220752;
  yslope[37] = 225500;
  yslope[38] = 230456;
  yslope[39] = 235635;
  yslope[40] = 241051;
  yslope[41] = 246723;
  yslope[42] = 252668;
  yslope[43] = 258907;
  yslope[44] = 265462;
  yslope[45] = 272357;
  yslope[46] = 279620;
  yslope[47] = 287281;
  yslope[48] = 295373;
  yslope[49] = 303935;
  yslope[50] = 313007;
  yslope[51] = 322638;
  yslope[52] = 332881;
  yslope[53] = 343795;
  yslope[54] = 355449;
  yslope[55] = 367921;
  yslope[56] = 381300;
  yslope[57] = 395689;
  yslope[58] = 411206;
  yslope[59] = 427990;
  yslope[60] = 446202;
  yslope[61] = 466033;
  yslope[62] = 487709;
  yslope[63] = 511500;
  yslope[64] = 537731;
  yslope[65] = 566797;
  yslope[66] = 599186;
  yslope[67] = 635500;
  yslope[68] = 676500;
  yslope[69] = 723155;
  yslope[70] = 776722;
  yslope[71] = 838860;
  yslope[72] = 911805;
  yslope[73] = 998643;
  yslope[74] = 1103764;
  yslope[75] = 1233618;
  yslope[76] = 1398101;
  yslope[77] = 1613193;
  yslope[78] = 1906501;
  yslope[79] = 2330168;
  yslope[80] = 2995931;
  yslope[81] = 4194304;
  yslope[82] = 6990506;
  yslope[83] = 20971520;
  yslope[84] = 20971520;
  yslope[85] = 6990506;
  yslope[86] = 4194304;
  yslope[87] = 2995931;
  yslope[88] = 2330168;
  yslope[89] = 1906501;
  yslope[90] = 1613193;
  yslope[91] = 1398101;
  yslope[92] = 1233618;
  yslope[93] = 1103764;
  yslope[94] = 998643;
  yslope[95] = 911805;
  yslope[96] = 838860;
  yslope[97] = 776722;
  yslope[98] = 723155;
  yslope[99] = 676500;
  yslope[100] = 635500;
  yslope[101] = 599186;
  yslope[102] = 566797;
  yslope[103] = 537731;
  yslope[104] = 511500;
  yslope[105] = 487709;
  yslope[106] = 466033;
  yslope[107] = 446202;
  yslope[108] = 427990;
  yslope[109] = 411206;
  yslope[110] = 395689;
  yslope[111] = 381300;
  yslope[112] = 367921;
  yslope[113] = 355449;
  yslope[114] = 343795;
  yslope[115] = 332881;
  yslope[116] = 322638;
  yslope[117] = 313007;
  yslope[118] = 303935;
  yslope[119] = 295373;
  yslope[120] = 287281;
  yslope[121] = 279620;
  yslope[122] = 272357;
  yslope[123] = 265462;
  yslope[124] = 258907;
  yslope[125] = 252668;
  yslope[126] = 246723;
  yslope[127] = 241051;
  yslope[128] = 235635;
  yslope[129] = 230456;
  yslope[130] = 225500;
  yslope[131] = 220752;
  yslope[132] = 216201;
  yslope[133] = 211833;
  yslope[134] = 207638;
  yslope[135] = 203606;
  yslope[136] = 199728;
  yslope[137] = 195995;
  yslope[138] = 192399;
  yslope[139] = 188932;
  yslope[140] = 185588;
  yslope[141] = 182361;
  yslope[142] = 179243;
  yslope[143] = 176231;
  yslope[144] = 173318;
  yslope[145] = 170500;
  yslope[146] = 167772;
  yslope[147] = 165130;
  yslope[148] = 162569;
  yslope[149] = 160087;
  yslope[150] = 157680;
  yslope[151] = 155344;
  yslope[152] = 153076;
  yslope[153] = 150874;
  yslope[154] = 148734;
  yslope[155] = 146653;
  yslope[156] = 144631;
  yslope[157] = 142663;
  yslope[158] = 140748;
  yslope[159] = 138884;
  yslope[160] = 137068;
  yslope[161] = 135300;
  yslope[162] = 133576;
  yslope[163] = 131896;
  yslope[164] = 130257;
  yslope[165] = 128659;
  yslope[166] = 127100;
  yslope[167] = 125577;
  distscale[0] = 92789;
  distscale[1] = 92434;
  distscale[2] = 92154;
  distscale[3] = 91876;
  distscale[4] = 91600;
  distscale[5] = 91327;
  distscale[6] = 91056;
  distscale[7] = 90722;
  distscale[8] = 90456;
  distscale[9] = 90194;
  distscale[10] = 89867;
  distscale[11] = 89611;
  distscale[12] = 89355;
  distscale[13] = 89038;
  distscale[14] = 88790;
  distscale[15] = 88479;
  distscale[16] = 88235;
  distscale[17] = 87932;
  distscale[18] = 87691;
  distscale[19] = 87393;
  distscale[20] = 87157;
  distscale[21] = 86867;
  distscale[22] = 86578;
  distscale[23] = 86350;
  distscale[24] = 86068;
  distscale[25] = 85787;
  distscale[26] = 85512;
  distscale[27] = 85293;
  distscale[28] = 85021;
  distscale[29] = 84755;
  distscale[30] = 84490;
  distscale[31] = 84226;
  distscale[32] = 83968;
  distscale[33] = 83711;
  distscale[34] = 83457;
  distscale[35] = 83206;
  distscale[36] = 82957;
  distscale[37] = 82713;
  distscale[38] = 82470;
  distscale[39] = 82230;
  distscale[40] = 81944;
  distscale[41] = 81709;
  distscale[42] = 81478;
  distscale[43] = 81248;
  distscale[44] = 80976;
  distscale[45] = 80752;
  distscale[46] = 80529;
  distscale[47] = 80267;
  distscale[48] = 80050;
  distscale[49] = 79793;
  distscale[50] = 79582;
  distscale[51] = 79332;
  distscale[52] = 79126;
  distscale[53] = 78880;
  distscale[54] = 78639;
  distscale[55] = 78439;
  distscale[56] = 78204;
  distscale[57] = 77971;
  distscale[58] = 77742;
  distscale[59] = 77553;
  distscale[60] = 77328;
  distscale[61] = 77107;
  distscale[62] = 76888;
  distscale[63] = 76672;
  distscale[64] = 76459;
  distscale[65] = 76249;
  distscale[66] = 76042;
  distscale[67] = 75838;
  distscale[68] = 75635;
  distscale[69] = 75436;
  distscale[70] = 75207;
  distscale[71] = 75014;
  distscale[72] = 74822;
  distscale[73] = 74635;
  distscale[74] = 74418;
  distscale[75] = 74235;
  distscale[76] = 74054;
  distscale[77] = 73847;
  distscale[78] = 73671;
  distscale[79] = 73469;
  distscale[80] = 73300;
  distscale[81] = 73104;
  distscale[82] = 72939;
  distscale[83] = 72749;
  distscale[84] = 72589;
  distscale[85] = 72405;
  distscale[86] = 72224;
  distscale[87] = 72046;
  distscale[88] = 71895;
  distscale[89] = 71722;
  distscale[90] = 71552;
  distscale[91] = 71386;
  distscale[92] = 71221;
  distscale[93] = 71060;
  distscale[94] = 70902;
  distscale[95] = 70746;
  distscale[96] = 70593;
  distscale[97] = 70442;
  distscale[98] = 70295;
  distscale[99] = 70150;
  distscale[100] = 70007;
  distscale[101] = 69867;
  distscale[102] = 69730;
  distscale[103] = 69595;
  distscale[104] = 69445;
  distscale[105] = 69315;
  distscale[106] = 69188;
  distscale[107] = 69046;
  distscale[108] = 68924;
  distscale[109] = 68805;
  distscale[110] = 68672;
  distscale[111] = 68557;
  distscale[112] = 68430;
  distscale[113] = 68320;
  distscale[114] = 68198;
  distscale[115] = 68094;
  distscale[116] = 67977;
  distscale[117] = 67878;
  distscale[118] = 67767;
  distscale[119] = 67659;
  distscale[120] = 67568;
  distscale[121] = 67465;
  distscale[122] = 67365;
  distscale[123] = 67280;
  distscale[124] = 67185;
  distscale[125] = 67094;
  distscale[126] = 67005;
  distscale[127] = 66919;
  distscale[128] = 66845;
  distscale[129] = 66764;
  distscale[130] = 66686;
  distscale[131] = 66611;
  distscale[132] = 66538;
  distscale[133] = 66468;
  distscale[134] = 66400;
  distscale[135] = 66334;
  distscale[136] = 66272;
  distscale[137] = 66212;
  distscale[138] = 66155;
  distscale[139] = 66100;
  distscale[140] = 66048;
  distscale[141] = 65999;
  distscale[142] = 65952;
  distscale[143] = 65908;
  distscale[144] = 65866;
  distscale[145] = 65827;
  distscale[146] = 65789;
  distscale[147] = 65755;
  distscale[148] = 65723;
  distscale[149] = 65694;
  distscale[150] = 65664;
  distscale[151] = 65641;
  distscale[152] = 65619;
  distscale[153] = 65600;
  distscale[154] = 65584;
  distscale[155] = 65570;
  distscale[156] = 65558;
  distscale[157] = 65548;
  distscale[158] = 65542;
  distscale[159] = 65538;
  distscale[160] = 65537;
  distscale[161] = 65538;
  distscale[162] = 65541;
  distscale[163] = 65547;
  distscale[164] = 65557;
  distscale[165] = 65568;
  distscale[166] = 65582;
  distscale[167] = 65598;
  distscale[168] = 65617;
  distscale[169] = 65638;
  distscale[170] = 65661;
  distscale[171] = 65691;
  distscale[172] = 65720;
  distscale[173] = 65751;
  distscale[174] = 65785;
  distscale[175] = 65822;
  distscale[176] = 65861;
  distscale[177] = 65903;
  distscale[178] = 65946;
  distscale[179] = 65993;
  distscale[180] = 66042;
  distscale[181] = 66094;
  distscale[182] = 66148;
  distscale[183] = 66205;
  distscale[184] = 66265;
  distscale[185] = 66327;
  distscale[186] = 66392;
  distscale[187] = 66458;
  distscale[188] = 66528;
  distscale[189] = 66602;
  distscale[190] = 66677;
  distscale[191] = 66755;
  distscale[192] = 66836;
  distscale[193] = 66908;
  distscale[194] = 66994;
  distscale[195] = 67082;
  distscale[196] = 67173;
  distscale[197] = 67268;
  distscale[198] = 67353;
  distscale[199] = 67452;
  distscale[200] = 67554;
  distscale[201] = 67646;
  distscale[202] = 67754;
  distscale[203] = 67864;
  distscale[204] = 67963;
  distscale[205] = 68080;
  distscale[206] = 68183;
  distscale[207] = 68305;
  distscale[208] = 68414;
  distscale[209] = 68541;
  distscale[210] = 68655;
  distscale[211] = 68788;
  distscale[212] = 68908;
  distscale[213] = 69028;
  distscale[214] = 69171;
  distscale[215] = 69297;
  distscale[216] = 69425;
  distscale[217] = 69576;
  distscale[218] = 69711;
  distscale[219] = 69848;
  distscale[220] = 69988;
  distscale[221] = 70129;
  distscale[222] = 70274;
  distscale[223] = 70421;
  distscale[224] = 70572;
  distscale[225] = 70724;
  distscale[226] = 70879;
  distscale[227] = 71038;
  distscale[228] = 71199;
  distscale[229] = 71362;
  distscale[230] = 71529;
  distscale[231] = 71698;
  distscale[232] = 71871;
  distscale[233] = 72020;
  distscale[234] = 72198;
  distscale[235] = 72378;
  distscale[236] = 72562;
  distscale[237] = 72723;
  distscale[238] = 72912;
  distscale[239] = 73077;
  distscale[240] = 73271;
  distscale[241] = 73442;
  distscale[242] = 73642;
  distscale[243] = 73818;
  distscale[244] = 74024;
  distscale[245] = 74204;
  distscale[246] = 74387;
  distscale[247] = 74604;
  distscale[248] = 74791;
  distscale[249] = 74981;
  distscale[250] = 75174;
  distscale[251] = 75404;
  distscale[252] = 75602;
  distscale[253] = 75803;
  distscale[254] = 76007;
  distscale[255] = 76215;
  distscale[256] = 76424;
  distscale[257] = 76636;
  distscale[258] = 76852;
  distscale[259] = 77070;
  distscale[260] = 77290;
  distscale[261] = 77515;
  distscale[262] = 77703;
  distscale[263] = 77933;
  distscale[264] = 78165;
  distscale[265] = 78401;
  distscale[266] = 78599;
  distscale[267] = 78840;
  distscale[268] = 79083;
  distscale[269] = 79291;
  distscale[270] = 79540;
  distscale[271] = 79752;
  distscale[272] = 80007;
  distscale[273] = 80224;
  distscale[274] = 80485;
  distscale[275] = 80708;
  distscale[276] = 80931;
  distscale[277] = 81202;
  distscale[278] = 81431;
  distscale[279] = 81662;
  distscale[280] = 81897;
  distscale[281] = 82181;
  distscale[282] = 82421;
  distscale[283] = 82663;
  distscale[284] = 82909;
  distscale[285] = 83156;
  distscale[286] = 83407;
  distscale[287] = 83660;
  distscale[288] = 83915;
  distscale[289] = 84175;
  distscale[290] = 84436;
  distscale[291] = 84701;
  distscale[292] = 84968;
  distscale[293] = 85239;
  distscale[294] = 85456;
  distscale[295] = 85733;
  distscale[296] = 86011;
  distscale[297] = 86294;
  distscale[298] = 86522;
  distscale[299] = 86809;
  distscale[300] = 87099;
  distscale[301] = 87335;
  distscale[302] = 87630;
  distscale[303] = 87871;
  distscale[304] = 88174;
  distscale[305] = 88419;
  distscale[306] = 88727;
  distscale[307] = 88976;
  distscale[308] = 89292;
  distscale[309] = 89547;
  distscale[310] = 89804;
  distscale[311] = 90128;
  distscale[312] = 90389;
  distscale[313] = 90655;
  distscale[314] = 90989;
  distscale[315] = 91259;
  distscale[316] = 91532;
  distscale[317] = 91806;
  distscale[318] = 92083;
  distscale[319] = 92364;

  // Calculate the light levels to use
  //  for each level / scale combination.
  for (i = 0; i < LIGHTLEVELS; i++) {
    startmap = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
    for (j = 0; j < MAXLIGHTSCALE; j++) {
      level = startmap - j * SCREENWIDTH / (viewwidth << detailshift) / DISTMAP;

      if (level < 0)
        level = 0;

      if (level >= NUMCOLORMAPS)
        level = NUMCOLORMAPS - 1;

      scalelight[i][j] = colormaps + level * 256;
    }
  }
}

//
// R_Init
//
extern int detailLevel;
extern int screenblocks;

void R_Init(void) {
  R_InitData();
  printf("\nR_InitData");
  R_InitPointToAngle();
  printf("\nR_InitPointToAngle");
  R_InitTables();
  // viewwidth / viewheight / detailLevel are set by the defaults
  printf("\nR_InitTables");

  R_SetViewSize(screenblocks, detailLevel);
  R_InitPlanes();
  printf("\nR_InitPlanes");
  R_InitLightTables();
  printf("\nR_InitLightTables");
  R_InitSkyMap();
  printf("\nR_InitSkyMap");
  R_InitTranslationTables();
  printf("\nR_InitTranslationsTables");

  framecount = 0;
}

//
// R_PointInSubsector
//
subsector_t *R_PointInSubsector(fixed_t x, fixed_t y) {
  node_t *node;
  int side;
  int nodenum;

  // single subsector is a special case
  if (!numnodes)
    return subsectors;

  nodenum = numnodes - 1;

  while (!(nodenum & NF_SUBSECTOR)) {
    node = &nodes[nodenum];
    side = R_PointOnSide(x, y, node);
    nodenum = node->children[side];
  }

  return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// R_SetupFrame
//
void R_SetupFrame(player_t *player) {
  int i;

  viewplayer = player;
  viewx = player->mo->x;
  viewy = player->mo->y;
  viewangle = player->mo->angle + viewangleoffset;
  extralight = player->extralight;

  viewz = player->viewz;

  viewsin = finesine[viewangle >> ANGLETOFINESHIFT];
  viewcos = finecosine[viewangle >> ANGLETOFINESHIFT];

  sscount = 0;

  if (player->fixedcolormap) {
    fixedcolormap =
        colormaps + player->fixedcolormap * 256 * sizeof(lighttable_t);

    walllights = scalelightfixed;

    for (i = 0; i < MAXLIGHTSCALE; i++)
      scalelightfixed[i] = fixedcolormap;
  } else
    fixedcolormap = 0;

  framecount++;
  validcount++;
}

//
// R_RenderView
//
void R_RenderPlayerView(player_t *player) {
  R_SetupFrame(player);

  // Clear buffers.
  R_ClearClipSegs();
  R_ClearDrawSegs();
  R_ClearPlanes();
  R_ClearSprites();

  // check for new console commands.
  NetUpdate();

  // The head node is the last node output.
  R_RenderBSPNode(numnodes - 1);

  // Check for new console commands.
  NetUpdate();

  R_DrawPlanes();

  // Check for new console commands.
  NetUpdate();

  R_DrawMasked();

  // Check for new console commands.
  NetUpdate();
}
