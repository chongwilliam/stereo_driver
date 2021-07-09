//#include "Utils.h"
#include "CameraMapper.h"
#include "RectificationMap.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <thread>
#include <opencv2/opencv.hpp> // Gregorij, 2/15/2019
#include <sys/types.h>

// #include "EPI-io-type-defs.h"
#include "epimoduleparamsenum.h"

bool gMapReferenceAsTexture = true;
bool gDoImageFileWriting = false;
bool gTimeReporting = false;

float gDxMapping = 0.0f, gDyMapping = 0.0f;
bool gDxyOn = false;
bool gSaveAdjMap = true;

int gUseThreads = 4;

#define maxval(a,b) (((a) > (b)) ? (a) : (b))
#define minval(a,b) (((a) < (b)) ? (a) : (b))

using namespace std;
using namespace cv;

// HHB resamplings
// the input image is a stacked set, so windowing is just in the row dimension
//

void RGGB_MONO(unsigned char *src, unsigned char *dst,  int width, int height)
{
    unsigned char *r0, *r1, *r2;
    unsigned char *mlim = src + (height*width), *lim;
    unsigned char lur, nur, ldr, ndr, lub, nub, ldb, ndb, lg, ng, ug, dg;

    // first row
    for (r1=src, r2=r1+width, lim=r2, nur=*r1++, dg=*r2++, ng=*r1++, ndb=*r2++, ldb=ndb, lg=ng; r1 < lim;) {
        *dst++ = ((nur)+ ((lg+ng+dg)/3) + ((ldb+ndb)>>1))/3;
        lur=nur; nur=*r1++; dg=*r2++;
        *dst++ = (((nur+lur)>>1)+ (ng) + (ndb))/3;
        lg=ng; ng=*r1++; ldb=ndb; ndb=*r2++;
    }
    // last 2 columns
    *dst++ = ((nur)+ ((lg+ng+dg)/3) + ((ldb+ndb)>>1))/3;
    *dst++ = ((nur)+ (ng) + (ndb))/3;

    // body
    for (r0 = src, r1 = r0 + width, r2 = r1 + width; r2 < mlim;) {
        // even rows
        for (lim = r1, nur = *r0++, ng = *r1++, ndr = *r2++, ug = *r0++, nub = *r1++, dg = *r2++, lub=nub; r0 < lim;) {
            *dst++ = (((nur+ndr)>>1)+ (ng) + ((lub+nub)>>1))/3;
            lur=nur; nur = *r0++; lg=ng; ng = *r1++; ldr=ndr; ndr = *r2++;
            *dst++ = (((lur+nur+ldr+ndr)>>2)+ ((lg+ug+dg+ng)>>2) + (nub))/3;
            lub=nub; ug = *r0++; nub = *r1++; dg = *r2++;
        }
        // last 2 columns
        *dst++ = (((nur+ndr)>>1)+ (ng) + ((lub+nub)>>1))/3;
        *dst++ = (((nur+ndr)>>1)+ ((ng+ug+dg)/3) + (nub))/3;

        // odd rows
        for (lim = r1, ug = *r0++, nur = *r1++, dg = *r2++, nub = *r0++, ng = *r1++, ndb = *r2++, lg=ng, lub=nub, ldb=ndb; r0 < lim;) {
            *dst++ = ((nur)+ ((lg+ug+dg+ng)>>2) + ((lub+ldb+nub+ndb)>>2))/3;
            ug = *r0++; lur=nur; nur = *r1++; dg = *r2++;
            *dst++ = (((lur+nur)>>1)+ (ng) + ((nub+ndb)>>1))/3;
            lub=nub; nub = *r0++; lg=ng; ng = *r1++; ldb=ndb; ndb = *r2++;
        }
        // last 2 columns
        *dst++ = ((nur)+ ((lg+ug+dg+ng)>>2) + ((lub+nub+ldb+ndb)>>2))/3;
        *dst++ = ((nur)+ (ng) + ((nub+ndb)>>1))/3;
    }

    // last row
    for (lim = r1, nur = *r0++, ng = *r1++, ug = *r0++, nub = *r1++, lub=nub; r0 < lim;) {
        *dst++ = ((nur)+ (ng) + ((lub+nub)>>1))/3;
        lur=nur; nur = *r0++; lg=ng; ng = *r1++;
        *dst++ = (((nur+lur)>>1)+ ((lg+ug+ng)/3) + (nub))/3;
        ug = *r0++; lub = nub; nub = *r1++;
    }
    // last 2 columns
    *dst++ = ((nur)+ (ng) + ((nub+lub)>>1))/3;
    *dst++ = ((nur)+ ((ng+ug)>>1) + (nub))/3;

}

void RGGB_MONO2(unsigned char *src, unsigned char *dst,  int width, int height)     // doubles blue
{
    unsigned char *r0, *r1, *r2;
    unsigned char *mlim = src + (height * width), *lim;
    unsigned char lur, nur, ldr, ndr, lub, nub, ldb, ndb, lg, ng, ug, dg;

    // first row
    for (r1=src, r2=r1+width, lim=r2, nur=*r1++, dg=*r2++, ng=*r1++, ndb=*r2++, ldb=ndb, lg=ng; r1 < lim;) {
        *dst++ = ((nur)+ ((lg+ng+dg)/3) + ((ldb+ndb)))/3;
        lur=nur; nur=*r1++; dg=*r2++;
        *dst++ = (((nur+lur)>>1)+ (ng) + (ndb<<1))/3;
        lg=ng; ng=*r1++; ldb=ndb; ndb=*r2++;
    }
    // last 2 columns
    *dst++ = ((nur)+ ((lg+ng+dg)/3) + ((ldb+ndb)))/3;
    *dst++ = ((nur)+ (ng) + (ndb<<1))/3;

    // body
    for (r0 = src, r1 = r0 + width, r2 = r1 + width; r2 < mlim;) {
        // even rows
        for (lim = r1, nur = *r0++, ng = *r1++, ndr = *r2++, ug = *r0++, nub = *r1++, dg = *r2++, lub=nub; r0 < lim;) {
            *dst++ = (((nur+ndr)>>1)+ (ng) + ((lub+nub)))/3;
            lur=nur; nur = *r0++; lg=ng; ng = *r1++; ldr=ndr; ndr = *r2++;
            *dst++ = (((lur+nur+ldr+ndr)>>2)+ ((lg+ug+dg+ng)>>2) + (nub<<1))/3;
            lub=nub; ug = *r0++; nub = *r1++; dg = *r2++;
        }
        // last 2 columns
        *dst++ = (((nur+ndr)>>1)+ (ng) + ((lub+nub)))/3;
        *dst++ = (((nur+ndr)>>1)+ ((ng+ug+dg)/3) + (nub<<1))/3;

        // odd rows
        for (lim = r1, ug = *r0++, nur = *r1++, dg = *r2++, nub = *r0++, ng = *r1++, ndb = *r2++, lg=ng, lub=nub, ldb=ndb; r0 < lim;) {
            *dst++ = ((nur)+ ((lg+ug+dg+ng)>>2) + ((lub+ldb+nub+ndb)>>1))/3;
            ug = *r0++; lur=nur; nur = *r1++; dg = *r2++;
            *dst++ = (((lur+nur)>>1)+ (ng) + ((nub+ndb)))/3;
            lub=nub; nub = *r0++; lg=ng; ng = *r1++; ldb=ndb; ndb = *r2++;
        }
        // last 2 columns
        *dst++ = ((nur)+ ((lg+ug+dg+ng)>>2) + ((lub+nub+ldb+ndb)>>1))/3;
        *dst++ = ((nur)+ (ng) + ((nub+ndb)))/3;
    }

    // last row
    for (lim = r1, nur = *r0++, ng = *r1++, ug = *r0++, nub = *r1++, lub=nub; r0 < lim;) {
        *dst++ = ((nur)+ (ng) + ((lub+nub)))/3;
        lur=nur; nur = *r0++; lg=ng; ng = *r1++;
        *dst++ = (((nur+lur)>>1)+ ((lg+ug+ng)/3) + (nub<<1))/3;
        ug = *r0++; lub = nub; nub = *r1++;
    }
    // last 2 columns
    *dst++ = ((nur)+ (ng) + ((nub+lub)))/3;
    *dst++ = ((nur)+ ((ng+ug)>>1) + (nub<<1))/3;

}

void xGGx_MONO_2x2(unsigned char *image, unsigned char *rety, int width, int height)
{

    int sum, pa, pb, pc, pd;
    bool even = true;
    int imagearea = width * height;
    unsigned char* ptr1, * ptr2, * hlim, * lim = image + imagearea;
    unsigned char *y0, *y = rety;

    ptr1 = image, ptr2 = image + width;
    hlim = ptr2;
    pb = *ptr1++;
    pd = *ptr2++;

    for (; ptr2 <= lim;)
    {
        pa = pb; pb = *ptr1++;
        pc = pd; pd = *ptr2++;
        if (even)
        {
            sum = ((pa + pd + ((pb + pc) >> 1)) / 3);
        }
        else
        {
            sum = ((pb + pc + ((pa + pd) >> 1)) / 3);
        }
        *y++ = sum;
        even = !even;

        if (ptr1 == hlim)
        {
            *y++ = sum; // repeat last pixel for full row
            hlim = ptr2;
            pb = *ptr1++;
            pd = *ptr2++;
        }
    }

    for (y0 = lim - width; y0 < lim;) // copy last row for full image
    {
        *y++=*y0++;
    }
}

// alternate Bayer pattern

void GxxG_MONO_2x2(unsigned char *image, unsigned char *rety, int width, int height)
{

    int sum, pa, pb, pc, pd;
    bool even = true;
    int imagearea = width * height;
    unsigned char* ptr1, * ptr2, * hlim, * lim = image + imagearea;
    unsigned char *y0, *y = rety;

    ptr1 = image, ptr2 = image + width;
    hlim = ptr2;
    pb = *ptr1++;
    pd = *ptr2++;

    for (; ptr2 <= lim;)
    {
        pa = pb; pb = *ptr1++;
        pc = pd; pd = *ptr2++;
        if (even)
        {
            sum = ((pb + pc + ((pa + pd) >> 1)) / 3);
         }
        else
        {
            sum = ((pa + pd + ((pb + pc) >> 1)) / 3);
        }
        *y++ = sum;
        even = !even;

        if (ptr1 == hlim)
        {
            *y++ = sum; // repeat last pixel for full row
            hlim = ptr2;
            pb = *ptr1++;
            pd = *ptr2++;
        }
    }

    for (y0 = lim - width; y0 < lim;) // copy last row for full image
    {
        *y++=*y0++;
    }
}

void RGGB_RGB(unsigned char *src, unsigned char *dst,  int width, int height)
{
    unsigned char *r0, *r1, *r2;
    unsigned char *mlim = src + (height * width), *lim;
    unsigned char lur, nur, ldr, ndr, lub, nub, ldb, ndb, lg, ng, ug, dg;

    // first row
    for (r1=src, r2=r1+width, lim=r2, nur=*r1++, dg=*r2++, ng=*r1++, ndb=*r2++, ldb=ndb, lg=ng; r1 < lim;) {
        *dst++ = nur;                       // R
        *dst++ = (lg+ng+dg)/3;              // G
        *dst++ = (ldb+ndb)>>1;              // B
        lur=nur; nur=*r1++; dg=*r2++;
        *dst++ = (nur+lur)>>1;              // R
        *dst++ = ng;                        // G
        *dst++ = ndb;                       // B
        lg=ng; ng=*r1++; ldb=ndb; ndb=*r2++;
    }
    // last 2 columns
    *dst++ = nur;                           // R
    *dst++ = (lg+ng+dg)/3;                  // G
    *dst++ = (ldb+ndb)>>1;                  // B
    *dst++ = nur;                           // R
    *dst++ = ng;                            // G
    *dst++ = ndb;                           // B

    // body
    for (r0 = src, r1 = r0 + width, r2 = r1 + width; r2 < mlim;) {
        // even rows
        for (lim = r1, nur = *r0++, ng = *r1++, ndr = *r2++, ug = *r0++, nub = *r1++, dg = *r2++, lub=nub; r0 < lim;) {
            *dst++ = (nur+ndr)>>1;          // R
            *dst++ = ng;                    // G
            *dst++ = (lub+nub)>>1;          // B
            lur=nur; nur = *r0++; lg=ng; ng = *r1++; ldr=ndr; ndr = *r2++;
            *dst++ = (lur+nur+ldr+ndr)>>2;  // R
            *dst++ = (lg+ug+dg+ng)>>2;      // G
            *dst++ = nub;                   // B
            lub=nub; ug = *r0++; nub = *r1++; dg = *r2++;
        }
        // last 2 columns
        *dst++ = (nur+ndr)>>1;              // R
        *dst++ = ng;                        // G
        *dst++ = (lub+nub)>>1;              // B
        *dst++ = (nur+ndr)>>1;              // R
        *dst++ = (ng+ug+dg)/3;              // G
        *dst++ = nub;                       // B

        // odd rows
        for (lim = r1, ug = *r0++, nur = *r1++, dg = *r2++, nub = *r0++, ng = *r1++, ndb = *r2++, lg=ng, lub=nub, ldb=ndb; r0 < lim;) {
            *dst++ = nur;                   // R
            *dst++ = (lg+ug+dg+ng)>>2;      // G
            *dst++ = (lub+ldb+nub+ndb)>>2;  // B
            ug = *r0++; lur=nur; nur = *r1++; dg = *r2++;
            *dst++ = (lur+nur)>>1;          // R
            *dst++ = ng;                    // G
            *dst++ = (nub+ndb)>>1;          // B
            lub=nub; nub = *r0++; lg=ng; ng = *r1++; ldb=ndb; ndb = *r2++;
        }
        // last 2 columns
        *dst++ = nur;                       // R
        *dst++ = (lg+ug+dg+ng)>>2;          // G
        *dst++ = (lub+nub+ldb+ndb)>>2;      // B
        *dst++ = nur;                       // R
        *dst++ = ng;                        // G
        *dst++ = (nub+ndb)>>1;              // B
    }

    // last row
    for (lim = r1, nur = *r0++, ng = *r1++, ug = *r0++, nub = *r1++, lub=nub; r0 < lim;) {
        *dst++ = nur;                       // R
        *dst++ = ng;                        // G
        *dst++ = (lub+nub)>>1;              // B
        lur=nur; nur = *r0++; lg=ng; ng = *r1++;
        *dst++ = (nur+lur)>>1;              // R
        *dst++ = (lg+ug+ng)/3;              // G
        *dst++ = nub;                       // B
        ug = *r0++; lub = nub; nub = *r1++;
    }
    // last 2 columns
    *dst++ = nur;                           // R
    *dst++ = ng;                            // G
    *dst++ = (nub+lub)>>1;                  // B
    *dst++ = nur;                           // R
    *dst++ = (ng+ug)>>1;                    // G
    *dst++ = nub;                           // B
}

void RGGB_RGB2(unsigned char *src, unsigned char *dst,  int width, int height)      // doubles blue
{
    unsigned char *r0, *r1, *r2;
    unsigned char *mlim = src + (height * width), *lim;
    unsigned char lur, nur, ldr, ndr, lub, nub, ldb, ndb, lg, ng, ug, dg;

    // first row
    for (r1=src, r2=r1+width, lim=r2, nur=*r1++, dg=*r2++, ng=*r1++, ndb=*r2++, ldb=ndb, lg=ng; r1 < lim;) {
        *dst++ = nur;                       // R
        *dst++ = (lg+ng+dg)/3;              // G
        *dst++ = (ldb+ndb);                 // B
        lur=nur; nur=*r1++; dg=*r2++;
        *dst++ = (nur+lur)>>1;              // R
        *dst++ = ng;                        // G
        *dst++ = ndb<<1;                       // B
        lg=ng; ng=*r1++; ldb=ndb; ndb=*r2++;
    }
    // last 2 columns
    *dst++ = nur;                           // R
    *dst++ = (lg+ng+dg)/3;                  // G
    *dst++ = (ldb+ndb);                  // B
    *dst++ = nur;                           // R
    *dst++ = ng;                            // G
    *dst++ = ndb<<1;                           // B

    // body
    for (r0 = src, r1 = r0 + width, r2 = r1 + width; r2 < mlim;) {
        // even rows
        for (lim = r1, nur = *r0++, ng = *r1++, ndr = *r2++, ug = *r0++, nub = *r1++, dg = *r2++, lub=nub; r0 < lim;) {
            *dst++ = (nur+ndr)>>1;          // R
            *dst++ = ng;                    // G
            *dst++ = (lub+nub);          // B
            lur=nur; nur = *r0++; lg=ng; ng = *r1++; ldr=ndr; ndr = *r2++;
            *dst++ = (lur+nur+ldr+ndr)>>2;  // R
            *dst++ = (lg+ug+dg+ng)>>2;      // G
            *dst++ = nub<<1;                   // B
            lub=nub; ug = *r0++; nub = *r1++; dg = *r2++;
        }
        // last 2 columns
        *dst++ = (nur+ndr)>>1;              // R
        *dst++ = ng;                        // G
        *dst++ = (lub+nub);              // B
        *dst++ = (nur+ndr)>>1;              // R
        *dst++ = (ng+ug+dg)/3;              // G
        *dst++ = nub<<1;                       // B

        // odd rows
        for (lim = r1, ug = *r0++, nur = *r1++, dg = *r2++, nub = *r0++, ng = *r1++, ndb = *r2++, lg=ng, lub=nub, ldb=ndb; r0 < lim;) {
            *dst++ = nur;                   // R
            *dst++ = (lg+ug+dg+ng)>>2;      // G
            *dst++ = (lub+ldb+nub+ndb)>>1;  // B
            ug = *r0++; lur=nur; nur = *r1++; dg = *r2++;
            *dst++ = (lur+nur)>>1;          // R
            *dst++ = ng;                    // G
            *dst++ = (nub+ndb);          // B
            lub=nub; nub = *r0++; lg=ng; ng = *r1++; ldb=ndb; ndb = *r2++;
        }
        // last 2 columns
        *dst++ = nur;                       // R
        *dst++ = (lg+ug+dg+ng)>>1;          // G
        *dst++ = (lub+nub+ldb+ndb)>>2;      // B
        *dst++ = nur;                       // R
        *dst++ = ng;                        // G
        *dst++ = (nub+ndb);              // B
    }

    // last row
    for (lim = r1, nur = *r0++, ng = *r1++, ug = *r0++, nub = *r1++, lub=nub; r0 < lim;) {
        *dst++ = nur;                       // R
        *dst++ = ng;                        // G
        *dst++ = (lub+nub);              // B
        lur=nur; nur = *r0++; lg=ng; ng = *r1++;
        *dst++ = (nur+lur)>>1;              // R
        *dst++ = (lg+ug+ng)/3;              // G
        *dst++ = nub<<1;                       // B
        ug = *r0++; lub = nub; nub = *r1++;
    }
    // last 2 columns
    *dst++ = nur;                           // R
    *dst++ = ng;                            // G
    *dst++ = (nub+lub);                  // B
    *dst++ = nur;                           // R
    *dst++ = (ng+ug);                    // G
    *dst++ = nub;                           // B
}

void GRBG_MONO(unsigned char *src, unsigned char *dst,  int width, int height)
{
    unsigned char *r0, *r1, *r2;
    unsigned char *mlim = src + (height * width), *lim;
    unsigned char lur, nur, ldr, ndr, lub, nub, ldb, ndb, lg, ng, ug, dg;

    // first row
    for (r1 = src, r2 = r1 + width, lim = r2, ng = *r1++, ndr = *r2++, nub = *r1++, dg= *r2++, lub = nub; r1 < lim;) {
        *dst++ = (((nub+lub)>>1) + ng + ndr)/3;
        ldr = ndr; lg = ng; ng = *r1++; ndr = *r2++;
        *dst++ = (nub + (lg+dg+ng)/3 + ((ndr+ldr)>>1))/3;
        lub = nub; nub = *r1++; dg= *r2++;
    }
    // last 2 columns
    *dst++ = (((nub+lub)>>1) + ng + ndr)/3;
    *dst++ = (nub + ((ng+dg)>>1) + ndr)/3;

    // body
    for (r0 = src, r1 = r0 + width, r2 = r1 + width; r2 < mlim;) {
        // even rows
        for (lim = r1, ug = *r0++, nur = *r1++, dg = *r2++, nub = *r0++, ng = *r1++, ndb = *r2++, lub = nub, lg = ng, ldb = ndb; r0 < lim;) {
            *dst++ = (((lub+ldb+nub+ndb)>>2) + ((lg+ng+ug+dg)>>2) + nur)/3;
            ug = *r0++; lur = nur; nur = *r1++; dg = *r2++;
            *dst++ = (((nub+ndb)>>1) + ng + ((nur+lur)>>1))/3;
            lub = nub; lg = ng; ldb = ndb; nub = *r0++; ng = *r1++; ndb = *r2++;
        }
        // last 2 columns
        *dst++ = (((lub+ldb+nub+ndb)>>2) + ((lg+ng+ug+dg)>>2) + nur)/3;
        *dst++ = (((nub+ndb)>>1) + ng + nur)/3;

        // odd rows
        for (lim = r1, nur = *r0++, ng = *r1++, ndr = *r2++, ug = *r0++, nub = *r1++, dg = *r2++, lub = nub; r0 < lim;) {
            *dst++ = (((nub+lub)>>1) + ng + ((nur+ndr)>>1))/3;
            lur = nur; lg = ng; ldr = ndr; nur = *r0++; ng = *r1++; ndr = *r2++;
            *dst++ = (nub + ((lg+ng+dg+ug)>>2) + ((lur+ldr+nur+ndr)>>2))/3;
            ug = *r0++; lub = nub; nub = *r1++; dg = *r2++;
        }
        // last 2 columns
        *dst++ = (((nub+lub)>>1) + ng + ((ndr+nur)>>1))/3;
        *dst++ = (nub + (ng+dg+ug)/3 + ((ndr+nur)>>1))/3;
    }

    // last row
    for (lim = r1, ug = *r0++, nur = *r1++, nub = *r0++, ng = *r1++, lub = nub, lg = ng; r0 < lim;) {
        *dst++ = (((nub+lub)>>1) + (lg+ug+ng)/3 + nur)/3;
        ug = *r0++; lur = nur; nur = *r1++;
        *dst++ = (nub + ng + ((nur+lur)>>1))/3;
        lub = nub; nub = *r0++; lg = ng; ng = *r1++;
    }
    // last 2 columns
    *dst++ = (((nub+lub)>>1) + (lg+ug+ng)/3 + nur)/3;
    *dst++ = (nub + ((ng+ug)>>1) + nur)/3;
}

void GRBG_RGB(unsigned char *src, unsigned char *dst,  int width, int height)
{
    unsigned char *r0, *r1, *r2;
    unsigned char *mlim = src + (height * width), *lim;
    unsigned char lur, nur, ldr, ndr, lub, nub, ldb, ndb, lg, ng, ug, dg;

    // first row
    for (r1 = src, r2 = r1 + width, lim = r2, ng = *r1++, ndr = *r2++, nub = *r1++, dg= *r2++, lub = nub; r1 < lim;) {
        *dst++ = (nub+lub)>>1;               // R
        *dst++ = ng;                         // G
        *dst++ = ndr;                        // B
        ldr = ndr; lg = ng; ng = *r1++; ndr = *r2++;
        *dst++ = nub;                        // R
        *dst++ = (lg+dg+ng)/3;               // G
        *dst++ = (ndr+ldr)>>1;               // B
        lub = nub; nub = *r1++; dg= *r2++;
    }
    // last 2 columns
    *dst++ = (nub+lub)>>1;                   // R
    *dst++ = ng;                             // G
    *dst++ = ndr;                            // B
    *dst++ = nub;                            // R
    *dst++ = (ng+dg)>>1;                     // G
    *dst++ = ndr;                            // B

    // body
    for (r0 = src, r1 = r0 + width, r2 = r1 + width; r2 < mlim;) {
        // even rows
        for (lim = r1, ug = *r0++, nur = *r1++, dg = *r2++, nub = *r0++, ng = *r1++, ndb = *r2++, lub = nub, lg = ng, ldb = ndb; r0 < lim;) {
            *dst++ = (lub+ldb+nub+ndb)>>2;   // R
            *dst++ = (lg+ng+ug+dg)>>2;       // G
            *dst++ = nur;                    // B
            ug = *r0++; lur = nur; nur = *r1++; dg = *r2++;
            *dst++ = (nub+ndb)>>1;           // R
            *dst++ = ng;                     // G
            *dst++ = (nur+lur)>>1;           // B
            lub = nub; lg = ng; ldb = ndb; nub = *r0++; ng = *r1++; ndb = *r2++;
        }
        // last 2 columns
        *dst++ = (lub+ldb+nub+ndb)>>2;       // R
        *dst++ = (lg+ng+ug+dg)>>2;           // G
        *dst++ = nur;                        // B
        *dst++ = (nub+ndb)>>1;               // R
        *dst++ = ng;                         // G
        *dst++ = nur;                        // B

        // odd rows
        for (lim = r1, nur = *r0++, ng = *r1++, ndr = *r2++, ug = *r0++, nub = *r1++, dg = *r2++, lub = nub; r0 < lim;) {
            *dst++ = (nub+lub)>>1;           // R
            *dst++ = ng;                     // G
            *dst++ = (nur+ndr)>>1;           // B
            lur = nur; lg = ng; ldr = ndr; nur = *r0++; ng = *r1++; ndr = *r2++;
            *dst++ = nub;                    // R
            *dst++ = (lg+ng+dg+ug)>>2;       // G
            *dst++ = (lur+ldr+nur+ndr) >> 2; // B
            ug = *r0++; lub = nub; nub = *r1++; dg = *r2++;
        }
        // last 2 columns
        *dst++ = (nub+lub)>>1;               // R
        *dst++ = ng;                         // G
        *dst++ = (ndr+nur)>>1;               // B
        *dst++ = nub;                        // R
        *dst++ = (ng+dg+ug)/3;               // G
        *dst++ = (ndr+nur)>>1;               // B
    }

    // last row
    for (lim = r1, ug = *r0++, nur = *r1++, nub = *r0++, ng = *r1++, lub = nub, lg = ng; r0 < lim;) {
        *dst++ = (nub+lub)>>1;               // R
        *dst++ = (lg+ug+ng)/3;               // G
        *dst++ = nur;                        // B
        ug = *r0++; lur = nur; nur = *r1++;
        *dst++ = nub;                        // R
        *dst++ = ng;                         // G
        *dst++ = (nur+lur)>>1;               // B
        lub = nub; nub = *r0++; lg = ng; ng = *r1++;
    }
    // last 2 columns
    *dst++ = (nub+lub)>>1;                   // R
    *dst++ = (lg+ug+ng)/3;                   // G
    *dst++ = nur;                            // B
    *dst++ = nub;                            // R
    *dst++ = (ng+ug)>>1;                     // G
    *dst++ = nur;                            // B
}

// resampling via 3x4 transform
#define clip255(x) (x>255.0f) ? 255 : (x<0.0f) ? 0 : (unsigned char)x;

struct Point3
{
    float x, y, z;
};

/*
 * #define matmul (mat, vecin, vecout) \
    vecout.x = mat[0] * vecin.x + mat[1] * vecin.y + mat[2] * vecin.z + mat[3]; \
    vecout.y = mat[4] * vecin.x + mat[5] * vecin.y + mat[6] * vecin.z + mat[7]; \
    vecout.z = mat[8] * vecin.x + mat[9] * vecin.y + mat[10] * vecin.z + mat[11];
*/

#define COLORBYMATRIX
#ifdef COLORBYMATRIX
#define transform_and_store_mono(r, g, b, vin, vout, mat, dest) \
        vin.x = (float)(r); vin.y = (float)(g); vin.z = (float)(b); \
        vout.x = (mat[0] * vin.x) + (mat[1] * vin.y) + (mat[2] * vin.z) + mat[3]; \
        vout.y = mat[4] * vin.x + mat[5] * vin.y + mat[6] * vin.z + mat[7]; \
        vout.z = mat[8] * vin.x + mat[9] * vin.y + mat[10] * vin.z + mat[11]; \
        *dest++ = clip255((vout.x + vout.y + vout.z + 2) / 3)

#define transform_and_store_triple(r, g, b, vin, vout, mat, dest) \
        vin.x = (float)(r); vin.y = (float)(g); vin.z = (float)(b); \
        vout.x = mat[0] * vin.x + mat[1] * vin.y + mat[2] * vin.z + mat[3]; \
        vout.y = mat[4] * vin.x + mat[5] * vin.y + mat[6] * vin.z + mat[7]; \
        vout.z = mat[8] * vin.x + mat[9] * vin.y + mat[10] * vin.z + mat[11]; \
        *dest++ = clip255(vout.x); *dest++ = clip255(vout.y); *dest++ = clip255(vout.z)
#else
#define transform_and_store_mono(r, g, b, vin, vout, mat, dest) \
   *dest++ = clip255((r*mat[0]+ g*mat[1] + b*mat[2] + 2) / 3)

#define transform_and_store_triple(r, g, b, vin, vout, mat, dest) \
    *dest++ = clip255(r*mat[0]); *dest++ = clip255(g*mat[1]); *dest++ = clip255(b*mat[2])
#endif
// vout.x = r*mat[0]; vout.y = g*mat[1]; vout.z = b*mat[2]; \

static void RGGB_RGB_transforming_color(unsigned char *src, unsigned char *dst,  int width, int height, float *RGBmap)
{
    Point3 v, nv;
    unsigned char *r0, *r1, *r2;
    unsigned char *mlim = src + (height * width), *lim;
    unsigned char lur, nur, ldr, ndr, lub, nub, ldb, ndb, lg, ng, ug, dg;
    //        transform_and_store_triple(, , , v, nv, mat, dst);

    // first row
    for (r1=src, r2=r1+width, lim=r2, nur=*r1++, dg=*r2++, ng=*r1++, ndb=*r2++, ldb=ndb, lg=ng; r1 < lim;)
    {
        transform_and_store_triple(nur, (lg+ng+dg)/3, (ldb+ndb)>>1, v, nv, RGBmap, dst);
        lur=nur; nur=*r1++; dg=*r2++;
        transform_and_store_triple((nur+lur)>>1, ng, ndb, v, nv, RGBmap, dst);
        lg=ng; ng=*r1++; ldb=ndb; ndb=*r2++;
    }
    // last 2 columns
    transform_and_store_triple(nur, (lg+ng+dg)/3, (ldb+ndb)>>1, v, nv, RGBmap, dst);
    transform_and_store_triple(nur, ng, ndb, v, nv, RGBmap, dst);

    // body
    for (r0 = src, r1 = r0 + width, r2 = r1 + width; r2 < mlim;) {
        // even rows
        for (lim = r1, nur = *r0++, ng = *r1++, ndr = *r2++, ug = *r0++, nub = *r1++, dg = *r2++, lub=nub; r0 < lim;)
        {
            transform_and_store_triple((nur+ndr)>>1, ng, (lub+nub)>>1, v, nv, RGBmap, dst);
            lur=nur; nur = *r0++; lg=ng; ng = *r1++; ldr=ndr; ndr = *r2++;
            transform_and_store_triple((lur+nur+ldr+ndr)>>2, (lg+ug+dg+ng)>>2, nub, v, nv, RGBmap, dst);
            lub=nub; ug = *r0++; nub = *r1++; dg = *r2++;
        }
        // last 2 columns
        transform_and_store_triple((nur+ndr)>>1, ng, (lub+nub)>>1, v, nv, RGBmap, dst);
        transform_and_store_triple((nur+ndr)>>1, (ng+ug+dg)/3, nub, v, nv, RGBmap, dst);

        // odd rows
        for (lim = r1, ug = *r0++, nur = *r1++, dg = *r2++, nub = *r0++, ng = *r1++, ndb = *r2++, lg=ng, lub=nub, ldb=ndb; r0 < lim;)
        {
            transform_and_store_triple(nur, (lg+ug+dg+ng)>>2, (lub+ldb+nub+ndb)>>2, v, nv, RGBmap, dst);
            ug = *r0++; lur=nur; nur = *r1++; dg = *r2++;
            transform_and_store_triple((lur+nur)>>1, ng, (nub+ndb)>>1, v, nv, RGBmap, dst);
            lub=nub; nub = *r0++; lg=ng; ng = *r1++; ldb=ndb; ndb = *r2++;
        }
        // last 2 columns
        transform_and_store_triple(nur, (lg+ug+dg+ng)>>2, (lub+nub+ldb+ndb)>>2, v, nv, RGBmap, dst);
        transform_and_store_triple(nur, ng, (nub+ndb)>>1, v, nv, RGBmap, dst);
    }

    // last row
    for (lim = r1, nur = *r0++, ng = *r1++, ug = *r0++, nub = *r1++, lub=nub; r0 < lim;)
    {
        transform_and_store_triple(nur, ng, (lub+nub)>>1, v, nv, RGBmap, dst);
        lur=nur; nur = *r0++; lg=ng; ng = *r1++;
        transform_and_store_triple((nur+lur)>>1, (lg+ug+ng)/3, nub, v, nv, RGBmap, dst);
        ug = *r0++; lub = nub; nub = *r1++;
    }
    // last 2 columns
    transform_and_store_triple(nur, ng, (nub+lub)>>1, v, nv, RGBmap, dst);
    transform_and_store_triple(nur, (ng+ug)>>1, nub, v, nv, RGBmap, dst);
}

static void GRBG_RGB_transforming_color(unsigned char *src, unsigned char *dst,  int width, int height, float *RGBmap)
{
    Point3 v, nv;

    unsigned char *r0, *r1, *r2;
    unsigned char *mlim = src + (height * width), *lim;
    unsigned char lur, nur, ldr, ndr, lub, nub, ldb, ndb, lg, ng, ug, dg;

    // first row
    for (r1 = src, r2 = r1 + width, lim = r2, ng = *r1++, ndr = *r2++, nub = *r1++, dg= *r2++, lub = nub; r1 < lim;) {
        transform_and_store_triple((nub+lub)>>1, ng, ndr, v, nv, RGBmap, dst);
        ldr = ndr; lg = ng; ng = *r1++; ndr = *r2++;
        transform_and_store_triple(nub, (lg+dg+ng)/3, (ndr+ldr)>>1, v, nv, RGBmap, dst);
        lub = nub; nub = *r1++; dg= *r2++;
    }
    // last 2 columns
    transform_and_store_triple((nub+lub)>>1, ng, ndr, v, nv, RGBmap, dst);
    transform_and_store_triple(nub, (ng+dg)>>1, ndr, v, nv, RGBmap, dst);

    // body
    for (r0 = src, r1 = r0 + width, r2 = r1 + width; r2 < mlim;) {
        // even rows
        for (lim = r1, ug = *r0++, nur = *r1++, dg = *r2++, nub = *r0++, ng = *r1++, ndb = *r2++, lub = nub, lg = ng, ldb = ndb; r0 < lim;) {
            transform_and_store_triple((lub+ldb+nub+ndb)>>2, (lg+ng+ug+dg)>>2, nur, v, nv, RGBmap, dst);
            ug = *r0++; lur = nur; nur = *r1++; dg = *r2++;
            transform_and_store_triple((nub+ndb)>>1, ng, (nur+lur)>>1, v, nv, RGBmap, dst);
            lub = nub; lg = ng; ldb = ndb; nub = *r0++; ng = *r1++; ndb = *r2++;
        }
        // last 2 columns
        transform_and_store_triple((lub+ldb+nub+ndb)>>2, (lg+ng+ug+dg)>>2, nur, v, nv, RGBmap, dst);
        transform_and_store_triple((nub+ndb)>>1, ng, nur, v, nv, RGBmap, dst);

        // odd rows
        for (lim = r1, nur = *r0++, ng = *r1++, ndr = *r2++, ug = *r0++, nub = *r1++, dg = *r2++, lub = nub; r0 < lim;) {
            transform_and_store_triple((nub+lub)>>1, ng, (nur+ndr)>>1, v, nv, RGBmap, dst);
            lur = nur; lg = ng; ldr = ndr; nur = *r0++; ng = *r1++; ndr = *r2++;
            transform_and_store_triple(nub, (lg+ng+dg+ug)>>2, (lur+ldr+nur+ndr) >> 2, v, nv, RGBmap, dst);
            ug = *r0++; lub = nub; nub = *r1++; dg = *r2++;
        }
        // last 2 columns
        transform_and_store_triple((nub+lub)>>1, ng, (ndr+nur)>>1, v, nv, RGBmap, dst);
        transform_and_store_triple(nub, (ng+dg+ug)/3, (ndr+nur)>>1, v, nv, RGBmap, dst);
    }

    // last row
    for (lim = r1, ug = *r0++, nur = *r1++, nub = *r0++, ng = *r1++, lub = nub, lg = ng; r0 < lim;) {
        transform_and_store_triple((nub+lub)>>1, (lg+ug+ng)/3, nur, v, nv, RGBmap, dst);
        ug = *r0++; lur = nur; nur = *r1++;
        transform_and_store_triple(nub, ng, (nur+lur)>>1, v, nv, RGBmap, dst);
        lub = nub; nub = *r0++; lg = ng; ng = *r1++;
    }
    // last 2 columns
    transform_and_store_triple((nub+lub)>>1, (lg+ug+ng)/3, nur, v, nv, RGBmap, dst);
    transform_and_store_triple(nub, (ng+ug)>>1, nur, v, nv, RGBmap, dst);
}


static void RGGB_MONO_transforming_color(unsigned char *src, unsigned char *dst,  int width, int height, float *RGBmap)
{
    Point3 v, nv;

    unsigned char *r0, *r1, *r2;
    unsigned char *mlim = src + (height * width), *lim;
    unsigned char lur, nur, ldr, ndr, lub, nub, ldb, ndb, lg, ng, ug, dg;
    //        transform_and_store_mono(, , , v, nv, mat, dst);

    // first row
    for (r1=src, r2=r1+width, lim=r2, nur=*r1++, dg=*r2++, ng=*r1++, ndb=*r2++, ldb=ndb, lg=ng; r1 < lim;)
    {
        transform_and_store_mono(nur, (lg+ng+dg)/3, (ldb+ndb)>>1, v, nv, RGBmap, dst);
        lur=nur; nur=*r1++; dg=*r2++;
        transform_and_store_mono((nur+lur)>>1, ng, ndb, v, nv, RGBmap, dst);
        lg=ng; ng=*r1++; ldb=ndb; ndb=*r2++;
    }
    // last 2 columns
    transform_and_store_mono(nur, (lg+ng+dg)/3, (ldb+ndb)>>1, v, nv, RGBmap, dst);
    transform_and_store_mono(nur, ng, ndb, v, nv, RGBmap, dst);

    // body
    for (r0 = src, r1 = r0 + width, r2 = r1 + width; r2 < mlim;) {
        // even rows
        for (lim = r1, nur = *r0++, ng = *r1++, ndr = *r2++, ug = *r0++, nub = *r1++, dg = *r2++, lub=nub; r0 < lim;)
        {
            transform_and_store_mono((nur+ndr)>>1, ng, (lub+nub)>>1, v, nv, RGBmap, dst);
            lur=nur; nur = *r0++; lg=ng; ng = *r1++; ldr=ndr; ndr = *r2++;
            transform_and_store_mono((lur+nur+ldr+ndr)>>2, (lg+ug+dg+ng)>>2, nub, v, nv, RGBmap, dst);
            lub=nub; ug = *r0++; nub = *r1++; dg = *r2++;
        }
        // last 2 columns
        transform_and_store_mono((nur+ndr)>>1, ng, (lub+nub)>>1, v, nv, RGBmap, dst);
        transform_and_store_mono((nur+ndr)>>1, (ng+ug+dg)/3, nub, v, nv, RGBmap, dst);

        // odd rows
        for (lim = r1, ug = *r0++, nur = *r1++, dg = *r2++, nub = *r0++, ng = *r1++, ndb = *r2++, lg=ng, lub=nub, ldb=ndb; r0 < lim;)
        {
            transform_and_store_mono(nur, (lg+ug+dg+ng)>>2, (lub+ldb+nub+ndb)>>2, v, nv, RGBmap, dst);
            ug = *r0++; lur=nur; nur = *r1++; dg = *r2++;
            transform_and_store_mono((lur+nur)>>1, ng, (nub+ndb)>>1, v, nv, RGBmap, dst);
            lub=nub; nub = *r0++; lg=ng; ng = *r1++; ldb=ndb; ndb = *r2++;
        }
        // last 2 columns
        transform_and_store_mono(nur, (lg+ug+dg+ng)>>2, (lub+nub+ldb+ndb)>>2, v, nv, RGBmap, dst);
        transform_and_store_mono(nur, ng, (nub+ndb)>>1, v, nv, RGBmap, dst);
    }

    // last row
    for (lim = r1, nur = *r0++, ng = *r1++, ug = *r0++, nub = *r1++, lub=nub; r0 < lim;)
    {
        transform_and_store_mono(nur, ng, (lub+nub)>>1, v, nv, RGBmap, dst);
        lur=nur; nur = *r0++; lg=ng; ng = *r1++;
        transform_and_store_mono((nur+lur)>>1, (lg+ug+ng)/3, nub, v, nv, RGBmap, dst);
        ug = *r0++; lub = nub; nub = *r1++;
    }
    // last 2 columns
    transform_and_store_mono(nur, ng, (nub+lub)>>1, v, nv, RGBmap, dst);
    transform_and_store_mono(nur, (ng+ug)>>1, nub, v, nv, RGBmap, dst);
}



static void rectifyGRBGImage(const char *mapfile, Mat &bigImage, Mat &outImage)
{
    // Read rectification map from file:
    RectificationMap rmap;

    rmap.Load(mapfile);

    // use OpenCV to map
    std::vector<float> mapX = rmap.GetMapX();
    std::vector<float> mapY = rmap.GetMapY();
    Mat cvMapX = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapX.data());
    Mat cvMapY = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapY.data());

    // Remap using OpenCV
    //outImage = Mat(bigImage.rows, bigImage.cols, CV_8UC3);
    //outImage.setTo(Scalar(0.0, 255.0, 0.0));

    remap(bigImage, outImage, cvMapX, cvMapY, INTER_LINEAR);
}

int save_packed_image_to_ppm_file(char *file_name, unsigned char *image, int width, int height)
{
    FILE *fp;
    unsigned long image_size;
    int usewidth = ((width*3 + 3) / 4) * 4;

    if ((fp = fopen(file_name, "wb")) == NULL)
    {
        fprintf(stderr, "Couldn't open file %s", file_name);
        return (2);
    }
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", width, height);
    fprintf(fp, "255\n");

    image_size = usewidth * height;

    fwrite(image, 1, image_size, fp);
    if (fclose(fp) < 0)
    {
        fprintf(stderr, "Couldn't close file %s", file_name);
        return (1);
    }
    return (0);
}

int save_image_window_to_pgm_file_metadata(char *file_name, unsigned char *image, int rawwidth, int x0, int y0, int windowwidth, int windowheight, char *metadata)
{
    FILE *fp;
    unsigned long int yaccess, yaccesslim;
    unsigned char *reversebuffer;
    // int i;

    int usewidth = ((windowwidth + 3) / 4) * 4;
    reversebuffer = (unsigned char *)"";

    if ((fp = fopen(file_name, "wb")) == NULL)
    {
        fprintf(stderr, "Couldn't open file %s", file_name);
        return (2);
    }
    fprintf(fp, "P5\n");
    fprintf(fp, "# %s\n", metadata);
    fprintf(fp, "%d %d\n", usewidth, windowheight);
    fprintf(fp, "255\n");

    for (yaccess = y0 * rawwidth + x0, yaccesslim = (y0 + windowheight) * rawwidth + x0; yaccess < yaccesslim; yaccess += rawwidth)
    {
        fwrite(&image[yaccess], 1, usewidth, fp);
    }
    if (fclose(fp) < 0)
    {
        fprintf(stderr, "Couldn't close file %s", file_name);
        return (1);
    }
    return (0);
}

static void dispatchRemap(Mat src, Mat dst, Mat map1, Mat map2)
{
    remap(src, dst, map1, map2, INTER_LINEAR);      // this writes the rectified mono stack
}

void doRectify(Mat inBayerImageStack, Mat outRectifiedImageStack, Mat cvMapX, Mat cvMapY, char metadata[])
{
    int cols = inBayerImageStack.cols;
    int rows = inBayerImageStack.rows;
    GxxG_MONO_2x2(inBayerImageStack.data, inBayerImageStack.data, cols, rows);  // use resampler that does not have blue bias -- modify in place **Note the 0.5 pixel offset induced**

    Rect monoRectangle = Rect(0, 0, HSIZE, VSIZE * SENSORS);      // Mats are <row, col> but Rects are <width, height> // make this CV_8UC  // for output --trike height since wwant triple width
    Mat monoRectified = outRectifiedImageStack(monoRectangle);
    int threads = gUseThreads;
    if ((threads == 0) || (threads == 1))
        remap(inBayerImageStack, monoRectified, cvMapX, cvMapY, INTER_LINEAR);      // this writes the rectified mono stack
    else
    {
        std::vector<thread> t(threads);
        int vStart, vSize, vPos;
        int i;
        int stackHeightLimit = inBayerImageStack.rows - 1;
        fprintf(stderr, "%d threads\n", threads);

        for (i = 0; i < threads; i++)
        {
            vStart = ((i * SENSORS) / threads) * VSIZE;
            vPos = (((i + 1) * SENSORS) / threads) * VSIZE;
            vSize = minval(vPos, stackHeightLimit) - vStart;
            Rect useRect = Rect(0, vStart, HSIZE, vSize);
            t[i] = thread(dispatchRemap, inBayerImageStack(useRect), monoRectified(useRect), cvMapX(useRect), cvMapY(useRect));
        }
        for (int i = 0; i < threads; i++)
            t[i].join();
    }

    if (gDoImageFileWriting)
    {
        char bigRectRefname[] = "FullrectFrame.pgm";
        save_image_window_to_pgm_file_metadata(bigRectRefname, &outRectifiedImageStack.at<unsigned char>(0, 0), outRectifiedImageStack.cols, 0, 0, outRectifiedImageStack.cols, outRectifiedImageStack.rows, metadata);        // save rectified color reference image  **this is okay shape etc but colors bayerish
    }
}


// static void rectifyImages(const char *mapfile, Mat &bigImage, Mat &outrectImages)
// modifying to create single result bin file that has color reference image at end
static void rectifyImagesDual(const char* mapfile, Mat& bigImage, Mat& outrectImages, Mat& outrefImage, int greyOut)
{
	// Read rectification map from file:
	RectificationMap rmap;

	char metadata[] = "data";
	// float colortransform[] = {1.05, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,   0.0, 0.0, 1.3, 0.0};    // dummy scaling
	float colortransform[] = { 0.96183f, 0., 0., 0.,     0., 0.90947f, 0., 0.,     0., 0., 1.1869f, 0. };
	// float colortransform[] = {  0.96183, 0., 0., 0.,     0., 0.90947, 0., 0.,     0., 0., 1.5, 0.}; // changed blue
	// float colortransform [] = {0.96183, 0.90947, 1.1869};      // WB only - not full color correction

	rmap.Load(mapfile);

	// use OpenCV to map
	std::vector<float> mapX = rmap.GetMapX();
	std::vector<float> mapY = rmap.GetMapY();
    int numcamsinMosaic = rmap.GetNumberOfCameras();
    int newheight = (rmap.GetMosaicHeight() * 17) / numcamsinMosaic;
    Mat cvMapX = Mat(newheight, rmap.GetMosaicWidth(), CV_32FC1, mapX.data());
    Mat cvMapY = Mat(newheight, rmap.GetMosaicWidth(), CV_32FC1, mapY.data());
 //   Mat cvMapX = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapX.data());
 //   Mat cvMapY = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapY.data());
    if (gDxyOn)     // no need to shift other axis since reference imager is not being moved by either
	{
		static int TIndices[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12 };
		static int SIndices[] = { 0, 1, 2, 3, 8, 13, 14, 15, 16 };

		int numTImagersm1 = SENSORS / 2;
		int numSImagersm1 = SENSORS / 2;
		// REFERENCEIMAGEINDEX
		if (gDyMapping != 0.0f)     // T axis says del is 0 minus 8, so + means mapper selects -
		{
			float del, useDy = gDyMapping / numTImagersm1;
			fprintf(stderr, "Adjusting T by %f :", useDy);
			float* tData;
			int tLoc, atLoc, index;
			int tRrefLoc = numTImagersm1 / 2;
			for (tLoc = 0; tLoc <= numTImagersm1; tLoc++)
			{
				del = (tLoc - tRrefLoc) * useDy;
				fprintf(stderr, " %f", del);
				atLoc = TIndices[tLoc];
				tData = (float*)cvMapY.ptr(atLoc * VSIZE);       // T should adjust the vertical EPI alignments
				for (index = 0; index < HSIZE * VSIZE; index++)
					tData[index] -= del;
			}
			fprintf(stderr, "\n");
		}
		if (gDxMapping != 0.0f)    // S axis says del is 0 minus 8, so + means mapper selcted -
		{
			float del, useDx = gDxMapping / numSImagersm1;
			fprintf(stderr, "Adjusting S by %f :", useDx);
			float* sData;
			int sLoc, atLoc, index;
			int sRefLoc = numSImagersm1 / 2;
			for (sLoc = 0; sLoc <= numSImagersm1; sLoc++)
			{
				del = (sLoc - sRefLoc) * useDx;
				fprintf(stderr, " %f", del);
				atLoc = SIndices[sLoc];
				sData = (float*)cvMapX.ptr(atLoc * VSIZE);   // S should adjust the horizontal EPI alignments
				for (index = 0; index < HSIZE * VSIZE; index++)
					sData[index] -= del;
			}
			fprintf(stderr, "\n");
		}
	}


	fprintf(stderr, "Mappers are (r,c) = %d x %d\n", rmap.GetMosaicHeight(), rmap.GetMosaicWidth());
	fprintf(stderr, "bigImage is (r,c) = %d x %d\n", bigImage.rows, bigImage.cols);
	// Remap using OpenCV
	outrectImages = Mat(bigImage.rows, bigImage.cols, CV_8UC1);      // was UC3 == the rectified image should be monochrome
	// fprintf(stderr, "bigImage: (rows, cols) = (%d, %d) = %d\n", bigImage.rows, bigImage.cols, bigImage.rows * bigImage.cols);
	// printf(stderr, "outrectImages: (rows, cols) = (%d, %d) = %d\n", outrectImages.rows, outrectImages.cols, outrectImages.rows * outrectImages.cols);
	// outrectImages.setTo(Scalar(0.0, 255.0, 0.0));
	outrectImages.setTo(Scalar(0));

	Size matsize = outrectImages.size();
	fprintf(stderr, "Size outrectImages = (%d x %d)\n", matsize.height, matsize.width);
	// fprintf(stderr, "Size outrefImage = %d\n", outrefImage.size());

	if (gMapReferenceAsTexture)
	{
		int refy0 = VSIZE * REFERENCEIMAGEINDEX;
		Mat rgbRef = Mat(VSIZE, HSIZE, CV_8UC3);
		outrefImage = Mat(VSIZE, HSIZE, CV_8UC3);
		fprintf(stderr, "at setTo\n");
		outrefImage.setTo(Scalar(0));
		fprintf(stderr, "after setTo\n");

		// now create the windowed rectifying mappers and source region
		//
		Rect refRect = Rect(0, refy0, HSIZE, VSIZE);        // Mats are <row, col> but Rects are <width, height>
		Mat cvRefMapX = Mat(VSIZE, HSIZE, CV_32FC1);
		Mat cvRefMapY = Mat(VSIZE, HSIZE, CV_32FC1);

		cvMapX(refRect).copyTo(cvRefMapX);      //
		cvMapY(refRect).copyTo(cvRefMapY);
		Mat bayerRefpreRect = bigImage(refRect);
		if (gDoImageFileWriting)
		{
            char bayerRefpreRectname[] = "bayerRefpreRect.pgm";
            char bayerRefpreRectname2[] = "bayerRefpreRect2.pgm";
            fprintf(stderr, "bayerRefpreRect: (rows, cols) = (%d, %d) = %d, steps = (%d, %d)\n", bayerRefpreRect.rows, bayerRefpreRect.cols, bayerRefpreRect.rows * bayerRefpreRect.cols, (int)bayerRefpreRect.step[0], (int)bayerRefpreRect.step[1]);
			fprintf(stderr, "Windowed XMappers is (r,c) = %d x %d\n", cvRefMapX.rows, cvRefMapX.cols);
			fprintf(stderr, "Windowed YMappers is (r,c) = %d x %d\n", cvRefMapY.rows, cvRefMapY.cols);
			fprintf(stderr, "Windowed Color is (r,c) = %d x %d\n", outrefImage.rows, outrefImage.cols);
			fprintf(stderr, "X-Y Mappers at reference image (0,0) = (%f, %f)\n", cvMapX.at<float>(0, 0), cvMapY.at<float>(0, 0));
			fprintf(stderr, "X-Y Mappers at reference image (refy0,c) = (%f, %f)\n", cvMapX.at<float>(refy0, 0), cvMapY.at<float>(refy0, 0));

			imwrite(bayerRefpreRectname, bayerRefpreRect);        // save unrectified color reference image
			save_image_window_to_pgm_file_metadata(bayerRefpreRectname2, &bayerRefpreRect.at<unsigned char>(0, 0), bayerRefpreRect.cols, 0, 0, bayerRefpreRect.cols, bayerRefpreRect.rows, metadata);        // save unrectified monochrome reference image
		}

		GRBG_RGB_transforming_color(bayerRefpreRect.data, rgbRef.data, HSIZE, VSIZE, colortransform);        // getting color frame so can rectify the reference image for texturing

		// adjust mappers for reference window
		for (int maprow = 0; maprow < cvRefMapX.rows; maprow++)
		{
			float* mapYline = cvRefMapY.ptr<float>(maprow);
			for (int mapcol = 0; mapcol < cvRefMapX.cols; mapcol++)
			{
				mapYline[mapcol] -= refy0;
			}
		}

		remap(rgbRef, outrefImage, cvRefMapX, cvRefMapY, INTER_LINEAR);
		if (gDoImageFileWriting)
		{
            char rgbRefname[] = "colorReference.ppm";
            char outrefImagename[] = "rectColorReference.ppm";
            fprintf(stderr, "Saving color images\n unrect: %s\n rect: %s\n", rgbRefname, outrefImagename);
            save_packed_image_to_ppm_file(rgbRefname, rgbRef.data, rgbRef.cols, rgbRef.rows);        // save unrectified color reference image; imwrite alters the color bands
			save_packed_image_to_ppm_file(outrefImagename, outrefImage.data, outrefImage.cols, outrefImage.rows);        // save unrectified color reference image; imwrite alters the color bands
		}
	}

	if (greyOut)
	{
		GxxG_MONO_2x2(bigImage.data, bigImage.data, bigImage.cols, bigImage.rows);  // use resampler that does not have blue bias -- modify in place **Note the 0.5 pixel offset induced**
		remap(bigImage, outrectImages, cvMapX, cvMapY, INTER_LINEAR);
	}
	else
	{
		Mat tmpRef = Mat(VSIZE * SENSORS, HSIZE, CV_8UC3);
		GRBG_RGB_transforming_color(bigImage.data, tmpRef.data, HSIZE, VSIZE * SENSORS, colortransform);        // getting color frame so can rectify the reference image for texturing
		remap(tmpRef, outrectImages, cvMapX, cvMapY, INTER_LINEAR);
	}

	if (gDoImageFileWriting)
	{
        char bigRectRefname[] = "FullrectFrame.pgm";
		save_image_window_to_pgm_file_metadata(bigRectRefname, &outrectImages.at<unsigned char>(0, 0), bigImage.cols, 0, 0, bigImage.cols, bigImage.rows, metadata);        // save rectified color reference image  **this is okay shape etc but colors bayerish
	}
}

// static void rectifyImages(const char *mapfile, Mat &inBayerImageStack, Mat &outRectifiedImageStacktack)
// modifying to create single result that has color ref at end
//  outRectifiedImageStack come in as reference, are allocated here
//  inBayerImageStack sets the size -- SENSORS says the number although there are many global constants implied
// note the two references need to be deallocated in caller
//



static void rectifyImagesRefColor(const char* mapfile, Mat& inBayerImageStack, Mat& outRectifiedImageStack)
{
    clock_t execstart; // execend;
    double exectime;
    // Read rectification map from file:
    RectificationMap rmap;

    char metadata[] = "data";
    float colortransform[] = { 0.96183f, 0., 0., 0.,     0., 0.90947f, 0., 0.,     0., 0., 1.1869f, 0. };

    if (gTimeReporting) execstart = clock();

    rmap.Load(mapfile);
    if (gTimeReporting)
    {
        exectime = difftime(clock(), execstart);
        cout << "Load map LUTs time: " << ((float)(exectime) / CLOCKS_PER_SEC) << "s" << endl;
    }

    // use OpenCV to map
    std::vector<float> mapX = rmap.GetMapX();
    std::vector<float> mapY = rmap.GetMapY();
    Mat cvMapX = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapX.data());
    Mat cvMapY = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapY.data());
    if (gDxyOn)     // no need to shift other axis since reference imager is not being moved by either
    {
        static int TIndices[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12 };
        static int SIndices[] = { 0, 1, 2, 3, 8, 13, 14, 15, 16 };

        int numTImagersm1 = SENSORS / 2;
        int numSImagersm1 = SENSORS / 2;
        if (gDyMapping != 0.0f)     // T axis says del is 0 minus 8, so + means mapper selects -
        {
            float del, useDy = gDyMapping / numTImagersm1;
            fprintf(stderr, "Adjusting T by %f :", useDy);
            float* tData;
            int tLoc, atLoc, index;
            int tRrefLoc = numTImagersm1 / 2;
            for (tLoc = 0; tLoc <= numTImagersm1; tLoc++)
            {
                del = (tLoc - tRrefLoc) * useDy;
                fprintf(stderr, " %f", del);
                atLoc = TIndices[tLoc];
                tData = (float*)cvMapY.ptr(atLoc * VSIZE);       // T should adjust the vertical EPI alignments
                for (index = 0; index < HSIZE * VSIZE; index++)
                    tData[index] -= del;
            }
            fprintf(stderr, "\n");
        }
        if (gDxMapping != 0.0f)    // S axis says del is 0 minus 8, so + means mapper selcted -
        {
            float del, useDx = gDxMapping / numSImagersm1;
            fprintf(stderr, "Adjusting S by %f :", useDx);
            float* sData;
            int sLoc, atLoc, index;
            int sRefLoc = numSImagersm1 / 2;
            for (sLoc = 0; sLoc <= numSImagersm1; sLoc++)
            {
                del = (sLoc - sRefLoc) * useDx;
                fprintf(stderr, " %f", del);
                atLoc = SIndices[sLoc];
                sData = (float*)cvMapX.ptr(atLoc * VSIZE);   // S should adjust the horizontal EPI alignments
                for (index = 0; index < HSIZE * VSIZE; index++)
                    sData[index] -= del;
            }
            fprintf(stderr, "\n");
        }
    }

    fprintf(stderr, "Mappers are (r,c) = %d x %d\n", rmap.GetMosaicHeight(), rmap.GetMosaicWidth());
    fprintf(stderr, "inBayerImageStack is (r,c) = %d x %d\n", inBayerImageStack.rows, inBayerImageStack.cols);
    // Remap using OpenCV
    outRectifiedImageStack = Mat(inBayerImageStack.rows + VSIZE * 3, inBayerImageStack.cols, CV_8UC1);      // the rectified image is monochrome // add in 8UC3 = 3 * 8UC1 for rectified reference color
    // fprintf(stderr, "inBayerImageStack: (rows, cols) = (%d, %d) = %d\n", inBayerImageStack.rows, inBayerImageStack.cols, inBayerImageStack.rows * inBayerImageStack.cols);
    // printf(stderr, "outRectifiedImageStack: (rows, cols) = (%d, %d) = %d\n", outRectifiedImageStack.rows, outRectifiedImageStack.cols, outRectifiedImageStack.rows * outRectifiedImageStack.cols);
    outRectifiedImageStack.setTo(Scalar(0));     // expensive? necessary?

    Size matsize = outRectifiedImageStack.size();
    fprintf(stderr, "Size outRectifiedImageStack = (%d x %d) including ref RGB at end\n", matsize.height, matsize.width);

    // now create the windowed rectifying mappers and source region
    //
    int refypositionBayer = VSIZE * REFERENCEIMAGEINDEX;            // for input, where it is in the stack
    int refypositionRGB = VSIZE * SENSORS;                          // y position ref RGB 8UC3 result image at end of 8UC1 monochrome stack

    Rect refBayerRectangle = Rect(0, refypositionBayer, HSIZE, VSIZE);         // Mats are <row, col> but Rects are <width, height>  input from ref image position in stack
    Mat cvRefMapX = Mat(VSIZE, HSIZE, CV_32FC1);            // have to make copies of mapper to handle reference image
    Mat cvRefMapY = Mat(VSIZE, HSIZE, CV_32FC1);

    // copy mappers for reference so can chage their result offsets
    //
    cvMapX(refBayerRectangle).copyTo(cvRefMapX);      // copy these since need to construct a mapper to the RGB destination image
    cvMapY(refBayerRectangle).copyTo(cvRefMapY);

    // adjust Y mappers for reference window        // they begin pointing to reference position
    // could fix this need for copy by having refRGB point appropriate distance back (fancy calcs needed as mono not RGB)  -- use this for now
    for (int maprow = 0; maprow < cvRefMapY.rows; maprow++)
    {
        float* mapYline = cvRefMapY.ptr<float>(maprow);
        for (int mapcol = 0; mapcol < cvRefMapX.cols; mapcol++)
        {
            mapYline[mapcol] -= refypositionBayer;
        }
    }

    // would remap be faster if the source and destination were the same and the context was close for caching?
    // intermediate storage for Bayer to RGB reference image
    //
    Mat refRGB = Mat(VSIZE, HSIZE, CV_8UC3);                            // this is made from reference imager Bayer -> RGB  use as intermediate storage in RGB form; then gets rectified to top left corner of frame

    // set up image windows for result reference image  note width multple of 4 for full row by design...
    Rect refRGBRectangle = Rect(0, refypositionRGB, HSIZE, VSIZE * 3);      // Mats are <row, col> but Rects are <width, height> // make this CV_8UC  // for output --trike height since wwant triple width
    Mat refRGBRectified8UC1 = outRectifiedImageStack(refRGBRectangle);      // reference rectified goes at end of 8UC1 stack <may not get size correct since this is mapping from an 8UC1>
    Mat refRGBRectified = refRGBRectified8UC1.reshape(3, VSIZE);                // rectified RGB destination

    // refBayer is a single image at reference position in input stack
    //
    Mat refBayer = inBayerImageStack(refBayerRectangle);


    if (gDoImageFileWriting)
    {

        char bayerRefpreRectifiedname[] = "bayerRefpreRectified.pgm";
        char bayerRefpreRectifiedname2[] = "bayerRefpreRectified2.pgm";
        fprintf(stderr, "refBayer: (rows, cols) = (%d, %d) = %d, steps = (%d, %d)\n", refBayer.rows, refBayer.cols, refBayer.rows * refBayer.cols, (int)refBayer.step[0], (int)refBayer.step[1]);
        fprintf(stderr, "Windowed XMappers is (r,c) = %d x %d\n", cvRefMapX.rows, cvRefMapX.cols);
        fprintf(stderr, "Windowed YMappers is (r,c) = %d x %d\n", cvRefMapY.rows, cvRefMapY.cols);
        fprintf(stderr, "Windowed Color by V3 (r,c) = %d x %d\n", refRGBRectified8UC1.rows, refRGBRectified8UC1.cols);
        fprintf(stderr, "Windowed Color is (r,c) = %d x %d\n", refRGBRectified.rows, refRGBRectified.cols);
        imwrite(bayerRefpreRectifiedname, refBayer);        // save unrectified color reference image
        save_image_window_to_pgm_file_metadata(bayerRefpreRectifiedname2, &refBayer.at<unsigned char>(0, 0), refBayer.cols, 0, 0, refBayer.cols, refBayer.rows, metadata);        // save unrectified monochrome reference image
    }

    // map reference image from Bayer to RGB
    // do this first because mono conversion overwrites Bayer for color conversion
    //

    GRBG_RGB_transforming_color(refBayer.data, refRGB.data, HSIZE, VSIZE, colortransform);        // getting color frame so can rectify the reference image for texturing

    remap(refRGB, refRGBRectified, cvRefMapX, cvRefMapY, INTER_LINEAR);                 // this draws color image into end of allocated return rectified stack
    if (gDoImageFileWriting)
    {
        char refRGBname[] = "colorReference.ppm";
        char refRGBRectifiedname[] = "rectColorReference.ppm";
        save_packed_image_to_ppm_file(refRGBname, refRGB.data, refRGB.cols, refRGB.rows);           // save unrectified color reference image; imwrite alters the color bands
        save_packed_image_to_ppm_file(refRGBRectifiedname, refRGBRectified.data, refRGBRectified.cols, refRGBRectified.rows);        // save rectified color reference image
    }

    // overwrites itself since all move to top leftcorner, no chance to damage
    //

    doRectify(inBayerImageStack, outRectifiedImageStack, cvMapX, cvMapY, metadata);

   /*
   GxxG_MONO_2x2(inBayerImageStack.data, inBayerImageStack.data, inBayerImageStack.cols, inBayerImageStack.rows);  // use resampler that does not have blue bias -- modify in place **Note the 0.5 pixel offset induced**
 
    Rect monoRectangle = Rect(0, 0, HSIZE, VSIZE * SENSORS);      // Mats are <row, col> but Rects are <width, height> // make this CV_8UC  // for output --trike height since wwant triple width
    Mat monoRectified = outRectifiedImageStack(monoRectangle);
    int threads = gUseThreads;
    if ((threads == 0) || (threads == 1))
        remap(inBayerImageStack, monoRectified, cvMapX, cvMapY, INTER_LINEAR);      // this writes the rectified mono stack
    else
    {
        std::vector<thread> t(threads);
        int vStart, vSize, vPos;
        int i;
        int stackHeightLimit = inBayerImageStack.rows - 1;
        fprintf(stderr, "%d threads\n", threads);

        for (i = 0; i < threads; i++)
        {
            vStart = ((i * SENSORS) / threads) * VSIZE;
            vPos = (((i + 1) * SENSORS) / threads) * VSIZE;
            vSize = minval(vPos, stackHeightLimit) - vStart;
            Rect useRect = Rect(0, vStart, HSIZE, vSize);
            t[i] = thread(dispatchRemap, inBayerImageStack(useRect), monoRectified(useRect), cvMapX(useRect), cvMapY(useRect));
        }
        for (int i = 0; i < threads; i++)
            t[i].join();
    }
  
    if (gDoImageFileWriting)
    {
        char bigRectRefname[] = "FullrectFrame.pgm";
        save_image_window_to_pgm_file_metadata(bigRectRefname, &outRectifiedImageStack.at<unsigned char>(0, 0), outRectifiedImageStack.cols, 0, 0, outRectifiedImageStack.cols, outRectifiedImageStack.rows, metadata);        // save rectified color reference image  **this is okay shape etc but colors bayerish
    }
    */
}

bool gWriteDeltaCam= true;
int gWriteDeltaCamN = 0;

// static void rectifyImages(const char *mapfile, Mat &bigImage, Mat &outrectImages)
static void rectifyImagesDualAdj(const char* mapfile, const char* adjmapfile, Mat& bigImage, Mat& outrectImages, Mat& outrefImage)
{
	// Read rectification map from file:
	RectificationMap rmap;
	RectificationMap adjrmap;

	char metadata[] = "data";
	// float colortransform[] = {1.05, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,   0.0, 0.0, 1.3, 0.0};    // dummy scaling
	float colortransform[] = { 0.96183f, 0., 0., 0.,     0., 0.90947f, 0., 0.,     0., 0., 1.1869f, 0. };
	// float colortransform[] = {  0.96183, 0., 0., 0.,     0., 0.90947, 0., 0.,     0., 0., 1.5, 0.}; // changed blue
	// float colortransform [] = {0.96183, 0.90947, 1.1869};      // WB only - not full color correction

	static int TIndices[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12 };
	static int SIndices[] = { 0, 1, 2, 3, 8, 13, 14, 15, 16 };
	int numTImagersm1 = SENSORS / 2;
	int numSImagersm1 = SENSORS / 2;

	fprintf(stderr, "LUT File: %s\n", mapfile);
	rmap.Load(mapfile);
	// use OpenCV to map
	std::vector<float> mapX = rmap.GetMapX();
	std::vector<float> mapY = rmap.GetMapY();
	Mat cvMapX = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapX.data());
	Mat cvMapY = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapY.data());

	fprintf(stderr, "adj File: %s\n", adjmapfile);
	adjrmap.Load(adjmapfile);
	std::vector<float> adjmapX = adjrmap.GetMapX();
	std::vector<float> adjmapY = adjrmap.GetMapY();
	Mat cvadjMapX = Mat(adjrmap.GetMosaicHeight(), adjrmap.GetMosaicWidth(), CV_32FC1, adjmapX.data());
	Mat cvadjMapY = Mat(adjrmap.GetMosaicHeight(), adjrmap.GetMosaicWidth(), CV_32FC1, adjmapY.data());
	// apply the deltas
	Size xsize = cvMapX.size();
	Size ysize = cvMapY.size();

	Size adjxsize = cvadjMapX.size();
	Size adjysize = cvadjMapY.size();

	fprintf(stderr, "base xMat() %d x %d); adjxmat (%d x %d)\n", xsize.height, xsize.width, adjxsize.height, adjxsize.width);
	fprintf(stderr, "base yMat() %d x %d); adjymat (%d x %d)\n", ysize.height, ysize.width, adjysize.height, adjysize.width);

	double min, max;
	minMaxLoc(cvadjMapX, &min, &max);
	fprintf(stderr, "Adj X range = %f, %f\n", min, max);
	minMaxLoc(cvadjMapY, &min, &max);
	fprintf(stderr, "Adj Y range = %f, %f\n", min, max);

	// deltas are reference-frame-posisition - matched-ellipse-position;  so if this value is positive it says the desired source is that much farther to the left or up
	// subtract the delta from the LUT value
	cvMapX = cvMapX + cvadjMapX;
	cvMapY = cvMapY + cvadjMapY;

	//    cvMapX = cvMapX - cvadjMapX;
	//    cvMapY = cvMapY - cvadjMapY;
	if (gSaveAdjMap)
	{
		rmap.SetMapX(mapX);
		rmap.SetMapY(mapY);
		rmap.Save("adjusted_maps.bin");
	}


	if (gWriteDeltaCam)
	{
		int camindex = gWriteDeltaCamN;
		int u, v;
		fprintf(stderr, "Writing Adj file %d\n", camindex);
		Rect deltasRect = Rect(0, camindex * VSIZE, HSIZE, VSIZE);
		Mat writeDeltasMat; // rects are as Point, (x,y)
		if ((camindex < 4) || (camindex > 13))             // S is x = 0, T is y = 1;
		{
			writeDeltasMat = cvadjMapX(deltasRect);
			// writeDeltasMat = cvMapX(deltasRect);
		}
		else
		{
			writeDeltasMat = cvadjMapY(deltasRect);
			// writeDeltasMat = cvMapY(deltasRect);
		}

		char adjwritefile[128];
		sprintf(adjwritefile, "adjFile%d.txt", camindex);
		// sprintf(adjwritefile, "lutFile%d.txt", camindex);
		FILE* adjfile = fopen(adjwritefile, "w");
		for (v = 0; v < VSIZE; v++)
		{
			fprintf(adjfile, "%d:\n(", v);
			for (u = 0; u < HSIZE; u++)
			{
				fprintf(adjfile, "%.3f ", writeDeltasMat.at<float>(u, v));
			}
			fprintf(adjfile, ")\n");
		}
		fclose(adjfile);
	}
	if (gDxyOn)     // no need to shift other axis since reference imager is not being moved by either
	{

		// REFERENCEIMAGEINDEX
		if (gDyMapping != 0.0f)     // T axis says del is 0 minus 8, so + means mapper selects -
		{
			float del, useDy = gDyMapping / numTImagersm1;
			fprintf(stderr, "Adjusting T by %f :", useDy);
			float* tData;
			int tLoc, atLoc, index;
			int tRrefLoc = numTImagersm1 / 2;
			for (tLoc = 0; tLoc <= numTImagersm1; tLoc++)
			{
				del = (tLoc - tRrefLoc) * useDy;
				fprintf(stderr, " %f", del);
				atLoc = TIndices[tLoc];
				tData = (float*)cvMapY.ptr(atLoc * VSIZE);       // T should adjust the vertical EPI alignments
				for (index = 0; index < HSIZE * VSIZE; index++)
					tData[index] -= del;
			}
			fprintf(stderr, "\n");
		}
		if (gDxMapping != 0.0f)    // S axis says del is 0 minus 8, so + means mapper selcted -
		{
			float del, useDx = gDxMapping / numSImagersm1;
			fprintf(stderr, "Adjusting S by %f :", useDx);
			float* sData;
			int sLoc, atLoc, index;
			int sRefLoc = numSImagersm1 / 2;
			for (sLoc = 0; sLoc <= numSImagersm1; sLoc++)
			{
				del = (sLoc - sRefLoc) * useDx;
				fprintf(stderr, " %f", del);
				atLoc = SIndices[sLoc];
				sData = (float*)cvMapX.ptr(atLoc * VSIZE);   // S should adjust the horizontal EPI alignments
				for (index = 0; index < HSIZE * VSIZE; index++)
					sData[index] -= del;
			}
			fprintf(stderr, "\n");
		}
	}


	fprintf(stderr, "Mappers are (r,c) = %d x %d\n", rmap.GetMosaicHeight(), rmap.GetMosaicWidth());
	fprintf(stderr, "bigImage is (r,c) = %d x %d\n", bigImage.rows, bigImage.cols);
	// Remap using OpenCV
	outrectImages = Mat(bigImage.rows, bigImage.cols, CV_8UC1);      // was UC3 == the rectified image should be monochrome
	// fprintf(stderr, "bigImage: (rows, cols) = (%d, %d) = %d\n", bigImage.rows, bigImage.cols, bigImage.rows * bigImage.cols);
	// printf(stderr, "outrectImages: (rows, cols) = (%d, %d) = %d\n", outrectImages.rows, outrectImages.cols, outrectImages.rows * outrectImages.cols);
	// outrectImages.setTo(Scalar(0.0, 255.0, 0.0));
	outrectImages.setTo(Scalar(0));

	Size outsize = outrectImages.size();
	fprintf(stderr, "Size outrectImages = (%dx%d)\n", outsize.height, outsize.width);
	// fprintf(stderr, "Size outrefImage = %d\n", outrefImage.size());

	if (gMapReferenceAsTexture)
	{
		int refy0 = VSIZE * REFERENCEIMAGEINDEX;
		Mat rgbRef = Mat(VSIZE, HSIZE, CV_8UC3);
		outrefImage = Mat(VSIZE, HSIZE, CV_8UC3);
		fprintf(stderr, "at setTo\n");
		outrefImage.setTo(Scalar(0));
		fprintf(stderr, "after setTo\n");

		// now create the windowed rectifying mappers and source region
		//
		Rect refRectRaw = Rect(0, refy0, HSIZE, VSIZE);        // Mats are <row, col> but Rects are <width, height>
		Mat cvRefMapX = Mat(VSIZE, HSIZE, CV_32FC1);
		Mat cvRefMapY = Mat(VSIZE, HSIZE, CV_32FC1);

		cvMapX(refRectRaw).copyTo(cvRefMapX);
		cvMapY(refRectRaw).copyTo(cvRefMapY);
		Mat bayerRefpreRect = bigImage(refRectRaw);


		if (gDoImageFileWriting)
		{
            char bayerRefpreRectname[] = "bayerRefpreRect.pgm";
            char bayerRefpreRectname2[] = "bayerRefpreRect2.pgm";
            fprintf(stderr, "bayerRefpreRect: (rows, cols) = (%d, %d) = %d, steps = (%d, %d)\n", bayerRefpreRect.rows, bayerRefpreRect.cols, bayerRefpreRect.rows * bayerRefpreRect.cols, (int)bayerRefpreRect.step[0], (int)bayerRefpreRect.step[1]);
			fprintf(stderr, "Windowed XMappers is (r,c) = %d x %d\n", cvRefMapX.rows, cvRefMapX.cols);
			fprintf(stderr, "Windowed YMappers is (r,c) = %d x %d\n", cvRefMapY.rows, cvRefMapY.cols);
			fprintf(stderr, "Windowed Color is (r,c) = %d x %d\n", outrefImage.rows, outrefImage.cols);
			fprintf(stderr, "X-Y Mappers at reference image (0,0) = (%f, %f)\n", cvMapX.at<float>(0, 0), cvMapY.at<float>(0, 0));
			fprintf(stderr, "X-Y Mappers at reference image (refy0,c) = (%f, %f)\n", cvMapX.at<float>(refy0, 0), cvMapY.at<float>(refy0, 0));

			imwrite(bayerRefpreRectname, bayerRefpreRect);        // save unrectified color reference image
			save_image_window_to_pgm_file_metadata(bayerRefpreRectname2, &bayerRefpreRect.at<unsigned char>(0, 0), bayerRefpreRect.cols, 0, 0, bayerRefpreRect.cols, bayerRefpreRect.rows, metadata);        // save unrectified monochrome reference image
		}

		//RGGB_RGB(&bigImage.at<unsigned char>(refy0, 0), rgbRef.data, HSIZE, VSIZE);        // getting color frame so can rectify the reference image for texturing
		GRBG_RGB_transforming_color(bayerRefpreRect.data, rgbRef.data, HSIZE, VSIZE, colortransform);        // getting color frame so can rectify the reference image for texturing

		// adjust mappers for reference window
		for (int maprow = 0; maprow < cvRefMapX.rows; maprow++)
		{
			float* mapYline = cvRefMapY.ptr<float>(maprow);
			for (int mapcol = 0; mapcol < cvRefMapX.cols; mapcol++)
			{
				mapYline[mapcol] -= refy0;
			}
		}

		// fprintf(stderr, "At remap regRef\n");
		remap(rgbRef, outrefImage, cvRefMapX, cvRefMapY, INTER_LINEAR);

		if (gDoImageFileWriting)
		{
            char rgbRefname[] = "colorReferenceGRBG.ppm";
            char outrefImagename[] = "rectColorReference.ppm";
            fprintf(stderr, "Saving color images\n unrect: %s\n rect: %s\n", rgbRefname, outrefImagename);
            save_packed_image_to_ppm_file(rgbRefname, rgbRef.data, rgbRef.cols, rgbRef.rows);        // save unrectified color reference image; imwrite alters the color bands
			save_packed_image_to_ppm_file(outrefImagename, outrefImage.data, outrefImage.cols, outrefImage.rows);        // save unrectified color reference image; imwrite alters the color bands
		}
		// save_packed_image_to_ppm_file(outrefImagename, outrefImage.data, outrefImage.cols, outrefImage.rows);    // and save rectified color reference image
		// remap(&bigImage.at<unsigned char>(refy0, 0), refImage, &cvMapX.at<float>(refy0, 0), &cvMapY.at<float>(refy0, 0), INTER_LINEAR);
		// save_packed_image_to_ppm_file(rectcolorrefname, refImage.data, HSIZE, VSIZE);    // and save rectified color reference image
	}

	GxxG_MONO_2x2(bigImage.data, bigImage.data, bigImage.cols, bigImage.rows);  // use resampler that does not have blue bias -- modify in place **Note the 0.5 pixel offset induced**
	remap(bigImage, outrectImages, cvMapX, cvMapY, INTER_LINEAR);
	if (gDoImageFileWriting)
	{
        char bigRectRefname[] = "FullrectFrame.pgm";
		save_image_window_to_pgm_file_metadata(bigRectRefname, &outrectImages.at<unsigned char>(0, 0), bigImage.cols, 0, 0, bigImage.cols, bigImage.rows, metadata);        // save rectified color reference image  **this is okay shape etc but colors bayerish
	}
}
// doRemapProcess(inBayerImageStack, outRectifiedImageStack


   // overwrites itself since all move to top leftcorner, no chance to damage
    //

static void rectifyImages(const char* mapfile, Mat& bigImage, Mat& outImage)
{
	// Read rectification map from file:
	RectificationMap rmap;

	char metadata[] = "data";
	// float colortransform[] = {1.05, 0.0, 0.0, 0.0,   0.0, 1.0, 0.0, 0.0,   0.0, 0.0, 1.3, 0.0};    // dummy scaling
	float colortransform[] = { 0.96183f, 0., 0., 0.,     0., 0.90947f, 0., 0.,     0., 0., 1.1869f, 0. };
	// float colortransform [] = {0.96183, 0.90947, 1.1869};      // WB only - not full color correction

	rmap.Load(mapfile);

	// use OpenCV to map
	std::vector<float> mapX = rmap.GetMapX();
	std::vector<float> mapY = rmap.GetMapY();
	Mat cvMapX = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapX.data());
	Mat cvMapY = Mat(rmap.GetMosaicHeight(), rmap.GetMosaicWidth(), CV_32FC1, mapY.data());

	fprintf(stderr, "Mappers are (r,c) = %d x %d\n", rmap.GetMosaicHeight(), rmap.GetMosaicWidth());
	fprintf(stderr, "bigImage is (r,c) = %d x %d\n", bigImage.rows, bigImage.cols);
	// Remap using OpenCV
	outImage = Mat(bigImage.rows, bigImage.cols, CV_8UC1);      // was UC3 == the rectified image should be monochrome
	// outImage.setTo(Scalar(0.0, 255.0, 0.0));
	outImage.setTo(Scalar(0));

	if (gMapReferenceAsTexture)
	{
		int refy0 = VSIZE * REFERENCEIMAGEINDEX;
		Mat rgbRef = Mat(VSIZE, HSIZE, CV_8UC3);
		Mat rgbRefRect = Mat(VSIZE, HSIZE, CV_8UC3);

		Rect refRect = Rect(0, refy0, HSIZE, VSIZE);        // Mats are <row, col> but Rects are <width, height>
		Mat cvRefMapX = Mat(VSIZE, HSIZE, CV_32FC1);
		Mat cvRefMapY = Mat(VSIZE, HSIZE, CV_32FC1);

		cvMapX(refRect).copyTo(cvRefMapX);
		cvMapY(refRect).copyTo(cvRefMapY);
		Mat bayerRefpreRect = bigImage(refRect);

		if (gDoImageFileWriting)
		{
            char bayerRefpreRectname[] = "bayerRefpreRect.pgm";
            char bayerRefpreRectname2[] = "bayerRefpreRect2.pgm";
            fprintf(stderr, "Windowed XMappers is (r,c) = %d x %d\n", cvRefMapX.rows, cvRefMapX.cols);
			fprintf(stderr, "Windowed YMappers is (r,c) = %d x %d\n", cvRefMapY.rows, cvRefMapY.cols);
			fprintf(stderr, "Windowed Color is (r,c) = %d x %d\n", rgbRefRect.rows, rgbRefRect.cols);
			fprintf(stderr, "X-Y Mappers at reference image (0,0) = (%f, %f)\n", cvMapX.at<float>(0, 0), cvMapY.at<float>(0, 0));
			fprintf(stderr, "X-Y Mappers at reference image (refy0,c) = (%f, %f)\n", cvMapX.at<float>(refy0, 0), cvMapY.at<float>(refy0, 0));
			imwrite(bayerRefpreRectname, bayerRefpreRect);        // save unrectified monochrome reference image
			save_image_window_to_pgm_file_metadata(bayerRefpreRectname2, &bayerRefpreRect.at<unsigned char>(0, 0), bayerRefpreRect.cols, 0, 0, bayerRefpreRect.cols, bayerRefpreRect.rows, metadata);        // save unrectified monochrome reference image
		}

		GRBG_RGB_transforming_color(bayerRefpreRect.data, rgbRef.data, HSIZE, VSIZE, colortransform);        // getting color frame so can rectify the reference image for texturing
		//RGGB_RGB(&bigImage.at<unsigned char>(refy0, 0), rgbRef.data, HSIZE, VSIZE);        // getting color frame so can rectify the reference image for texturing

		// adjust mappers for reference window
		for (int maprow = 0; maprow < cvRefMapX.rows; maprow++)
		{
			float* mapYline = cvRefMapY.ptr<float>(maprow);
			for (int mapcol = 0; mapcol < cvRefMapX.cols; mapcol++)
			{
				mapYline[mapcol] -= refy0;
			}
		}
		remap(rgbRef, rgbRefRect, cvRefMapX, cvRefMapY, INTER_LINEAR);
		if (gDoImageFileWriting)
		{
            char rgbRefname[] = "colorReference.ppm";
            char rgbRefRectname[] = "rectColorReference.ppm";
            fprintf(stderr, "Saving color images\n unrect: %s\n rect: %s\n", rgbRefname, rgbRefRectname);
			save_packed_image_to_ppm_file(rgbRefname, rgbRef.data, rgbRef.cols, rgbRef.rows);        // save unrectified color reference image; imwrite alters the color bands
			save_packed_image_to_ppm_file(rgbRefRectname, rgbRefRect.data, rgbRefRect.cols, rgbRefRect.rows);        // save unrectified color reference image; imwrite alters the color bands
		}
		// save_packed_image_to_ppm_file(rgbRefRectname, rgbRefRect.data, rgbRefRect.cols, rgbRefRect.rows);    // and save rectified color reference image
		// remap(&bigImage.at<unsigned char>(refy0, 0), refImage, &cvMapX.at<float>(refy0, 0), &cvMapY.at<float>(refy0, 0), INTER_LINEAR);
		// save_packed_image_to_ppm_file(rectcolorrefname, refImage.data, HSIZE, VSIZE);    // and save rectified color reference image
	}

	GRBG_MONO(bigImage.data, bigImage.data, bigImage.cols, bigImage.rows);      // just use G to avoid B imbalance?  // I don't see this as doubling the blue band? Feb 2019
	remap(bigImage, outImage, cvMapX, cvMapY, INTER_LINEAR);
	if (gDoImageFileWriting)
	{
        char bigRectRefname[] = "FullrectFrame.pgm";
		save_image_window_to_pgm_file_metadata(bigRectRefname, &outImage.at<unsigned char>(0, 0), outImage.cols, 0, 0, outImage.cols, outImage.rows, metadata);        // save rectified color reference image  **this is okay shape etc but colors bayerish
	}
}


Rect CameraMapper::getBoundingBox()
{
    Rect bbox;
    return bbox;
    //return(CameraMapper::bounding_box);
}


// doit doesn't need image dimensions -- just calls the builder (here dual now)
// the problem is that it writes or sets for memory mapping a single buffer.  This buffer has to contain both rectified monochrome and rectified color reference images
// so have to set this up to write to separate buffers and have rectify code handle both
//
int doit(const char *mapfile, const char *indir, const char *outdir, const char *paramfile)
{
	try {
		// Load big image (stacked images)
		CameraMapper cmap;
		cmap.Initialize(paramfile);
        char outRectImagesfile[128];
        char outRefImagefile[128];

        sprintf(outRectImagesfile, "%s-rectstack", outdir);
        sprintf(outRefImagefile, "%s-refimage", outdir);

        fprintf(stderr, "Output mapping file is %s\n", outRectImagesfile);

        Rect bbox = cmap.getBoundingBox();
        fprintf(stderr, "[%d, %d] [%d, %d]\n", bbox.x, bbox.y, bbox.width, bbox.height);
		// load image frame example
		Mat bigImage = cmap.LoadImageFrame(indir, 0);
        Mat outRectImage;
        Mat outRefImage;

        // fprintf(stderr, "At call to rectifyImage\n");
        rectifyImagesDual(mapfile, bigImage, outRectImage, outRefImage, 1);

        fprintf(stderr, "After call to rectifyImagesDual\n");
        // cmap.SaveImageFrame(outdir, outRectImage, 0);
        cmap.SaveImageFrame(outRectImagesfile, outRectImage, 0);
        cmap.SaveImageFrame(outRefImagefile, outRefImage, 0);

	}
	catch (std::runtime_error* ex)
	{
		std::cout << "main(): Exception occured. " << ex->what() << std::endl;
		return -1;
	}
	catch (...)
	{
		std::cout << "main(): Failure. " << std::endl;
        return -2;
    }
    return 0;
}

int doitAdj(const char *mapfile, const char *adjmapfile, const char *indir, const char *outdir, const char *paramfile)
{
    try {
        // Load big image (stacked images)
        CameraMapper cmap;
        cmap.Initialize(paramfile);
        char outRectImagesfile[128];
        char outRefImagefile[128];

        sprintf(outRectImagesfile, "%s-rectstack", outdir);
        sprintf(outRefImagefile, "%s-refimage", outdir);

        fprintf(stderr, "Output mapping file is %s\n", outRectImagesfile);

        Rect bbox = cmap.getBoundingBox();
        fprintf(stderr, "[%d, %d] [%d, %d]\n", bbox.x, bbox.y, bbox.width, bbox.height);
        // load image frame example
        Mat bigImage = cmap.LoadImageFrame(indir, 0);
        Mat outRectImage;
        Mat outRefImage;

        fprintf(stderr, "At call to rectifyImage\n");
        rectifyImagesDualAdj(mapfile, adjmapfile, bigImage, outRectImage, outRefImage);

        fprintf(stderr, "After call to rectifyImagesDual\n");
        // cmap.SaveImageFrame(outdir, outRectImage, 0);
        cmap.SaveImageFrame(outRectImagesfile, outRectImage, 0);
        cmap.SaveImageFrame(outRefImagefile, outRefImage, 0);

    }
    catch (std::runtime_error* ex)
    {
        std::cout << "main(): Exception occured. " << ex->what() << std::endl;
        return -1;
    }
    catch (...)
    {
        std::cout << "main(): Failure. " << std::endl;
        return -2;
    }
    return 0;
}


extern int rectifyImagesDual(const char *mapfile, const IplImage *input, IplImage **outrectStack, IplImage **outrectRef, int greyOut)
{

    Mat bigImageMat = cvarrToMat(input);
    Mat outImageMat = cvarrToMat(*outrectStack);
    Mat outRefImageMat = cvarrToMat(*outrectRef);


    try {
        IplImage *iplRaw, ocvRaw;

        fprintf(stderr, "At extern call to rectifyImage\n");
        rectifyImagesDual(mapfile, bigImageMat, outImageMat, outRefImageMat, greyOut);

        fprintf(stderr, "After extern call to rectifyImagesDual\n");

        iplRaw = *outrectStack;
        ocvRaw = outImageMat;

        *outrectStack = cvCloneImage(&ocvRaw);
        cvReleaseImage(&iplRaw);

        iplRaw = *outrectRef;
        ocvRaw = outRefImageMat;

        *outrectRef = cvCloneImage(&ocvRaw);
        cvReleaseImage(&iplRaw);

    }
	catch (std::runtime_error* ex) {
		std::cout << "main(): Exception occured. " << ex->what() << std::endl;
		return -1;
	}
    catch (...) {
		std::cout << "main(): Failure. " << std::endl;
        return -2;
    }
    return 0;
}

extern int rectifyImagesDualAdj(const char *mapfile, const char *adjmapfile, const IplImage *input, IplImage **outrectStack, IplImage **outrectRef)
{

    Mat bigImageMat = cvarrToMat(input);
    Mat outImageMat = cvarrToMat(*outrectStack);
    Mat outRefImageMat = cvarrToMat(*outrectRef);


    try {
        IplImage *iplRaw, ocvRaw;

        fprintf(stderr, "At extern call to rectifyImage\n");
        rectifyImagesDualAdj(mapfile, adjmapfile, bigImageMat, outImageMat, outRefImageMat);

        fprintf(stderr, "After extern call to rectifyImagesDual\n");

        iplRaw = *outrectStack;
        ocvRaw = outImageMat;

        *outrectStack = cvCloneImage(&ocvRaw);
        cvReleaseImage(&iplRaw);

        iplRaw = *outrectRef;
        ocvRaw = outRefImageMat;

        *outrectRef = cvCloneImage(&ocvRaw);
        cvReleaseImage(&iplRaw);

    }
    catch (std::runtime_error* ex) {
        std::cout << "main(): Exception occured. " << ex->what() << std::endl;
        return -1;
    }
    catch (...) {
        std::cout << "main(): Failure. " << std::endl;
        return -2;
    }
    return 0;
}
extern int
rectifyImages(const char *mapfile, const IplImage *input, IplImage **output)
{
    Mat bigImageMat = cvarrToMat(input);
    Mat outImageMat = cvarrToMat(*output);

    try {
        IplImage *old, src;

        rectifyImages(mapfile, bigImageMat, outImageMat);
        old = *output;
        src = outImageMat;

        *output = cvCloneImage(&src);
        cvReleaseImage(&old);
    }
    catch (std::runtime_error* ex) {
        std::cout << "main(): Exception occured. " << ex->what() << std::endl;
        return -1;
    }
    catch (...) {
        std::cout << "main(): Failure. " << std::endl;
        return -2;
    }
    return 0;
}

extern int
rectifyImagesRefColor(const char *mapfile, const IplImage *input, IplImage **output)
{
    Mat bigImageMat = cvarrToMat(input);
    Mat outImageMat = cvarrToMat(*output);

    try {
        IplImage *old, src;

        rectifyImagesRefColor(mapfile, bigImageMat, outImageMat);
        fprintf(stderr, "%d cols x %d rows = %d bytes\n", outImageMat.cols, outImageMat.rows, outImageMat.cols * outImageMat.rows);
        old = *output;
        src = outImageMat;

        *output = cvCloneImage(&src);
        cvReleaseImage(&old);
    }
    catch (std::runtime_error* ex) {
        std::cout << "main(): Exception occured. " << ex->what() << std::endl;
        return -1;
    }
    catch (...) {
        std::cout << "main(): Failure. " << std::endl;
        return -2;
    }
    return 0;
}


extern int
rectifyImagesToDir(const char *mapfile, const IplImage *input, const char *outdir, const char *paramfile)
{
	try {
        Mat bigImageMat = cvarrToMat(input);
        Mat outImage;
		CameraMapper cmap;

		cmap.Initialize(paramfile);
        rectifyImages(mapfile, bigImageMat, outImage);
		cmap.SaveImageFrame(outdir, outImage, 0);
	}
	catch (std::runtime_error* ex)
	{
		std::cout << "main(): Exception occured. " << ex->what() << std::endl;
		return -1;
	}
	catch (...)
	{
		std::cout << "main(): Failure. " << std::endl;
        return -2;
    }
    return 0;
}

