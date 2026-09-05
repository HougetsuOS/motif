/* $XConsortium: DrTog.c /main/6 1995/10/25 20:00:29 cde-sun $ */
/*
 * Motif
 *
 * Copyright (c) 1987-2012, The Open Group. All rights reserved.
 *
 * These libraries and programs are free software; you can
 * redistribute them and/or modify them under the terms of the GNU
 * Lesser General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * These libraries and programs are distributed in the hope that
 * they will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with these librararies and programs; if not, write
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth
 * Floor, Boston, MA 02110-1301 USA
 * 
 */
/*
 * HISTORY
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "XmI.h"
#include "XmPlat/XmPlatP.h"
#include <Xm/DrawP.h>



/**************************** DrTog module ***************************
 *
 * Draw API used by Toggle Button only.
 *
 ***************************************************************************/




/********    Static Function Declarations    ********/
static void DrawCheckMark(Display *display,
			  Drawable d,
			  GC gc,
			  Position x,
			  Position y,
			  Dimension width,
			  Dimension height,
			  Dimension margin);

static void DrawCross(Display *display,
		      Drawable d,
		      GC gc,
		      Position x,
		      Position y,
		      Dimension width,
		      Dimension height,
		      Dimension margin);

/********    End Static Function Declarations    ********/


#define CHECK_TEMPLATE_WIDTH	32
#define CHECK_TEMPLATE_HEIGHT	32

static XmConst XPoint check_template[] = {
  {  0, 15 },
  {  6,  9 },
  { 14, 17 },
  { 31,  0 },
  { 31,  3 },
  { 21, 17 },
  { 16, 31 },
  {  0, 15 }
};

static void
DrawCheckMark(Display *display,
	      Drawable d,
	      GC gc,
	      Position x,
	      Position y,
	      Dimension width,
	      Dimension height,
	      Dimension margin)
{
  unsigned int old_width;
  XPoint check[XtNumber(check_template)];
  float scale_x = (width - 2 * margin - 1) / (float)CHECK_TEMPLATE_WIDTH;
  float scale_y = (height - 2 * margin - 1) / (float)CHECK_TEMPLATE_HEIGHT;
  int npoints = XtNumber(check_template);
  int i;

  /* Scale and translate the glyph to the desired area. */
  for (i = 0; i < npoints; i++)
    {
      check[i].x = (Position)(check_template[i].x*scale_x + 0.5) + x + margin;
      check[i].y = (Position)(check_template[i].y*scale_y + 0.5) + y + margin;
    }

  /* CR 9656: Force line_width so test results are not platform-dependent. */
  {
    XmPlatDrawCtx plat = _XmPlatCtx (display, d, gc) ;
    old_width = _XmPlatGetLineWidth (plat) ;
    _XmPlatSetLineWidth (plat, 1) ;
    _XmPlatCtxFree (plat) ;
  }

  /* Draw the check mark. */
  {
    XmPlatDrawCtx plat = _XmPlatCtx (display, d, gc) ;
    XmPlatPoint *ppts = (XmPlatPoint *) XtMalloc ((size_t)npoints *
						  sizeof (XmPlatPoint)) ;
    int pi ;
    for (pi = 0 ; pi < npoints ; pi++) {
      ppts[pi].x = check[pi].x ;
      ppts[pi].y = check[pi].y ;
    }
    _XmPlatFillPolygon (plat, ppts, npoints - 1, 0) ;
    _XmPlatDrawLines (plat, ppts, npoints, 0) ;
    _XmPlatCtxFree (plat) ;
    XtFree ((char *) ppts) ;
  }

  {
    XmPlatDrawCtx plat = _XmPlatCtx (display, d, gc) ;
    _XmPlatSetLineWidth (plat, old_width) ;
    _XmPlatCtxFree (plat) ;
  }
}

static void
DrawCross(Display *display,
	  Drawable d,
	  GC gc,
	  Position x,
	  Position y,
	  Dimension width,
	  Dimension height,
	  Dimension margin)
{
  Position left   = x + margin;
  Position right  = x + width - margin - 1;
  Position top    = y + margin;
  Position bottom = y + height - margin - 1;

  XSegment segs[6];
  Cardinal nsegs = 0;
    
  segs[nsegs].x1 = left;
  segs[nsegs].y1 = top + 1;
  segs[nsegs].x2 = right - 1;
  segs[nsegs].y2 = bottom;
  nsegs++;
	    
  segs[nsegs].x1 = left;
  segs[nsegs].y1 = top;
  segs[nsegs].x2 = right;
  segs[nsegs].y2 = bottom;
  nsegs++;

  segs[nsegs].x1 = left + 1;
  segs[nsegs].y1 = top;
  segs[nsegs].x2 = right;
  segs[nsegs].y2 = bottom - 1;
  nsegs++;
	    
  segs[nsegs].x1 = left;
  segs[nsegs].y1 = bottom - 1;
  segs[nsegs].x2 = right - 1;
  segs[nsegs].y2 = top;
  nsegs++;

  segs[nsegs].x1 = left;
  segs[nsegs].y1 = bottom;
  segs[nsegs].x2 = right;
  segs[nsegs].y2 = top;
  nsegs++;
	    
  segs[nsegs].x1 = left + 1;
  segs[nsegs].y1 = bottom;
  segs[nsegs].x2 = right;
  segs[nsegs].y2 = top + 1;
  nsegs++;

  assert(nsegs <= XtNumber(segs));

  {
    XmPlatDrawCtx plat = _XmPlatCtx (display, d, gc) ;
    XmPlatSegment psegs[6] ;
    unsigned int psi ;

    for (psi = 0 ; psi < nsegs ; psi++) {
      psegs[psi].x1 = segs[psi].x1 ; psegs[psi].y1 = segs[psi].y1 ;
      psegs[psi].x2 = segs[psi].x2 ; psegs[psi].y2 = segs[psi].y2 ;
    }
    _XmPlatDrawSegments (plat, psegs, (int)nsegs) ;
    _XmPlatCtxFree (plat) ;
  }
}


/***********************XmeDrawDiamond**********************************/
/*ARGSUSED*/
void XmeDrawDiamond(Display *display, Drawable d, 
                    GC top_gc, GC bottom_gc, GC center_gc, 
#if NeedWidePrototypes
                    int x, int y, 
                    int width, 
		    int height, /* unused */
                    int shadow_thick,
		    int margin)
#else
                    Position x, Position y, 
                    Dimension width, 
       		    Dimension height, /* unused */
                    Dimension shadow_thick,
	            Dimension margin)
#endif /* NeedWidePrototypes */
{
   XSegment seg[12];
   XPoint   pt[4];
   int midX, midY;
   int delta;
   _XmDisplayToAppContext(display);

   if (!d || !width) return ;

   _XmAppLock(app);
   if (width % 2 == 0) width--;

   if (width == 1) {
       {
	 XmPlatDrawCtx plat = _XmPlatCtx (display, d, top_gc) ;
	 _XmPlatDrawPoint (plat, x, y) ;
	 _XmPlatCtxFree (plat) ;
       }
       _XmAppUnlock(app);
       return ;
   } else
   if (width == 3) {
       seg[0].x1 = x;                   
       seg[0].y1 = seg[0].y2 = y + 1;
       seg[0].x2 = x + 2;

       seg[1].x1 = seg[1].x2 = x + 1;           
       seg[1].y1 = y ;
       seg[1].y2 = y + 2;
       {
	 XmPlatDrawCtx plat = _XmPlatCtx (display, d, top_gc) ;
	 XmPlatSegment pseg[2] ;
	 int psi ;

	 for (psi = 0 ; psi < 2 ; psi++) {
	   pseg[psi].x1 = seg[psi].x1 ; pseg[psi].y1 = seg[psi].y1 ;
	   pseg[psi].x2 = seg[psi].x2 ; pseg[psi].y2 = seg[psi].y2 ;
	 }
	 _XmPlatDrawSegments (plat, pseg, 2) ;
	 _XmPlatCtxFree (plat) ;
       }
       _XmAppUnlock(app);
       return ;
   } else   {        /* NORMAL SIZED ToggleButtonS : initial width >= 5 */
       midX = x + (width + 1) / 2;
       midY = y + (width + 1) / 2;
       /*  The top shadow segments  */
       seg[0].x1 = x;                   /*  1  */
       seg[0].y1 = midY - 1;
       seg[0].x2 = midX - 1;            /*  2  */
       seg[0].y2 = y;

       seg[1].x1 = x + 1;               /*  3  */
       seg[1].y1 = midY - 1;
       seg[1].x2 = midX - 1;            /*  4  */
       seg[1].y2 = y + 1;

       seg[2].x1 = x + 2;               /*  3  */
       seg[2].y1 = midY - 1;
       seg[2].x2 = midX - 1;            /*  4  */
       seg[2].y2 = y + 2;

       seg[3].x1 = midX - 1;            /*  5  */
       seg[3].y1 = y;
       seg[3].x2 = x + width - 1;       /*  6  */
       seg[3].y2 = midY - 1;

       seg[4].x1 = midX - 1;            /*  7  */
       seg[4].y1 = y + 1;
       seg[4].x2 = x + width - 2;       /*  8  */
       seg[4].y2 = midY - 1;

       seg[5].x1 = midX - 1;            /*  7  */
       seg[5].y1 = y + 2;
       seg[5].x2 = x + width - 3;       /*  8  */
       seg[5].y2 = midY - 1;

       /*  The bottom shadow segments  */
       seg[6].x1 = x;                   /*  9  */
       seg[6].y1 = midY - 1;
       seg[6].x2 = midX - 1;            /*  10  */
       seg[6].y2 = y + width - 1;

       seg[7].x1 = x + 1;               /*  11  */
       seg[7].y1 = midY - 1;
       seg[7].x2 = midX - 1;            /*  12  */
       seg[7].y2 = y + width - 2;

       seg[8].x1 = x + 2;               /*  11  */
       seg[8].y1 = midY - 1;
       seg[8].x2 = midX - 1;            /*  12  */
       seg[8].y2 = y + width - 3;

       seg[9].x1 = midX - 1;            /*  13  */
       seg[9].y1 = y + width - 1;
       seg[9].x2 = x + width - 1;       /*  14  */
       seg[9].y2 = midY - 1;

       seg[10].x1 = midX - 1;           /*  15  */
       seg[10].y1 = y + width - 2;
       seg[10].x2 = x + width - 2;      /*  16  */
       seg[10].y2 = midY - 1;

       seg[11].x1 = midX - 1;           /*  15  */
       seg[11].y1 = y + width - 3;
       seg[11].x2 = x + width - 3;      /*  16  */
       seg[11].y2 = midY - 1;
   }

   {
     XmPlatDrawCtx plat ;
     XmPlatSegment pseg[12] ;
     int psi ;

     for (psi = 0 ; psi < 12 ; psi++) {
       pseg[psi].x1 = seg[psi].x1 ; pseg[psi].y1 = seg[psi].y1 ;
       pseg[psi].x2 = seg[psi].x2 ; pseg[psi].y2 = seg[psi].y2 ;
     }
     plat = _XmPlatCtx (display, d, top_gc) ;
     _XmPlatDrawSegments (plat, pseg + 3, 3) ;
     _XmPlatCtxFree (plat) ;
     plat = _XmPlatCtx (display, d, bottom_gc) ;
     _XmPlatDrawSegments (plat, pseg + 6, 6) ;
     _XmPlatCtxFree (plat) ;
     plat = _XmPlatCtx (display, d, top_gc) ;
     _XmPlatDrawSegments (plat, pseg + 0, 3) ;
     _XmPlatCtxFree (plat) ;
   }

   if (width == 5 || !center_gc) { _XmAppUnlock(app); return ; }   /* <= 5 in fact */

   if (shadow_thick == 0) 
     delta = -3 ;
   else if (shadow_thick == 1) 
     delta = -1 ;
   else 
     delta = margin;

   pt[0].x = x + 3 + delta;
   pt[0].y = pt[2].y = midY - 1;
   pt[1].x = pt[3].x = midX - 1 ;
   pt[1].y = y + 2 + delta;
   pt[2].x = x + width - 3 - delta;
   pt[3].y = y + width - 3 - delta;
   
   {
     XmPlatDrawCtx plat = _XmPlatCtx (display, d, center_gc) ;
     XmPlatPoint ppts[4] ;
     int psi ;

     for (psi = 0 ; psi < 4 ; psi++) {
       ppts[psi].x = pt[psi].x ; ppts[psi].y = pt[psi].y ;
     }
     _XmPlatFillPolygon (plat, ppts, 4, 1) ;
     _XmPlatCtxFree (plat) ;
   }
   _XmAppUnlock(app);
}



/******************************XmeDrawIndicator**********************/
void 
XmeDrawIndicator(Display *display, 
		 Drawable d, 
		 GC gc, 
#if NeedWidePrototypes
		 int x, int y, 
		 int width, int height, 
		 int margin,
		 int type)
#else
                 Position x, Position y, 
                 Dimension width, Dimension height, 
                 Dimension margin,
                 XtEnum type)
#endif /* NeedWidePrototypes */
{
  _XmDisplayToAppContext(display);

  _XmAppLock(app);
  switch(type & 0xf0)
    {
    case XmINDICATOR_CHECK:
      DrawCheckMark(display, d, gc, x, y, width, height, margin);
      break;
	    
    case XmINDICATOR_CROSS:
      DrawCross(display, d, gc, x, y, width, height, margin);
      break;
    }
  _XmAppUnlock(app);
}

void
XmeDrawCircle(Display *display,
	      Drawable d,
	      GC top_gc,
	      GC bottom_gc,
	      GC center_gc,
#if NeedWidePrototypes
	      int x,
	      int y,
	      int width,
	      int height,
	      int shadow_thick,
	      int margin)
#else
	      Position x,
	      Position y,
	      Dimension width,
	      Dimension height,
	      Dimension shadow_thick,
	      Dimension margin)
#endif /* NeedWidePrototypes */
{
  int line_width = MIN(shadow_thick, MIN(width, height) / 2);
  _XmDisplayToAppContext(display);

  if ((width <= 0) || (height <= 0))
    return;

  _XmAppLock(app);
  if (shadow_thick > 0)
    {
      /* Force the GCs to use our values. */
      XmPlatDrawCtx top_plat = _XmPlatCtx (display, d, top_gc) ;
      XmPlatDrawCtx bot_plat = _XmPlatCtx (display, d, bottom_gc) ;
      unsigned int top_old_w = _XmPlatGetLineWidth (top_plat) ;
      unsigned int bot_old_w = _XmPlatGetLineWidth (bot_plat) ;

      _XmPlatSetLineWidth (top_plat, (unsigned) line_width) ;
      _XmPlatSetLineWidth (bot_plat, (unsigned) line_width) ;

#ifdef FIX_1402
      if (center_gc != NULL) {
    	  int delta = MIN(line_width + margin, MIN(width, height) / 2) -1;
	  XmPlatDrawCtx plat = _XmPlatCtx (display, d, center_gc) ;
	  _XmPlatFillArc (plat,
			  x + delta, y + delta,
			  (unsigned) MAX(width - 2 * delta, 1),
			  (unsigned) MAX(height - 2 * delta, 1),
			  0, 360 * 64) ;
	  _XmPlatCtxFree (plat) ;
      }
#endif
      
      {
	XmPlatDrawCtx plat = _XmPlatCtx (display, d, top_gc) ;
	_XmPlatDrawArc (plat,
			x + line_width/2, y + line_width/2,
			(unsigned) MAX(width - line_width, 1),
			(unsigned) MAX(height - line_width, 1),
			45 * 64, 180 * 64) ;
	_XmPlatCtxFree (plat) ;
	plat = _XmPlatCtx (display, d, bottom_gc) ;
	_XmPlatDrawArc (plat,
			x + line_width/2, y + line_width/2,
			(unsigned) MAX(width - line_width, 1),
			(unsigned) MAX(height - line_width, 1),
			45 * 64, -180 * 64) ;
	_XmPlatCtxFree (plat) ;
      }

      _XmPlatSetLineWidth (top_plat, top_old_w) ;
      _XmPlatSetLineWidth (bot_plat, bot_old_w) ;
      _XmPlatCtxFree (top_plat) ;
      _XmPlatCtxFree (bot_plat) ;
    }

#ifdef FIX_1402
  else {
	  if (center_gc != NULL) {
		  int delta = MIN(line_width + margin, MIN(width, height) / 2);
		  XmPlatDrawCtx plat = _XmPlatCtx (display, d, center_gc) ;
		  _XmPlatFillArc (plat,
				  x + delta, y + delta,
				  (unsigned) MAX(width - 2 * delta, 1),
				  (unsigned) MAX(height - 2 * delta, 1),
				  0, 360 * 64) ;
		  _XmPlatCtxFree (plat) ;
	  }
  }
#else
  if (center_gc != NULL)
    {
      /* Fill the center of the circle. */
      int delta = MIN(line_width + margin, MIN(width, height) / 2);
      XmPlatDrawCtx plat = _XmPlatCtx (display, d, center_gc) ;
      _XmPlatFillArc (plat,
		      x + delta, y + delta,
		      (unsigned) MAX(width - 2 * delta, 1),
		      (unsigned) MAX(height - 2 * delta, 1),
		      0, 360 * 64) ;
      _XmPlatCtxFree (plat) ;
    }
#endif
  _XmAppUnlock(app);
}
