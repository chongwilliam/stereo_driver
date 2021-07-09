#ifndef _RECTIFY_H
#define _RECTIFY_H
#include <opencv2/core/types_c.h>
int doit(const char *mapfile, const char *indir, const char *outdir, const char *paramfile);
int doitAdj(const char *mapfile,const char *adjmapfile, const char *indir, const char *outdir, const char *paramfile);
int rectifyImages(const char *mapfile, const IplImage *input, IplImage **output);
int rectifyImagesDual(const char *mapfile, const IplImage *input, IplImage **outputrectstack, IplImage **outputrectreferencecolor, int greyOut);
int rectifyImagesDualAdj(const char *mapfile, const char *adjmapfile, const IplImage *input, IplImage **outputrectstack, IplImage **outputrectreferencecolor);
int rectifyImagesToDir(const char *mapfile, const IplImage *input, const char *outdir, const char *paramfile);
int rectifyImagesRefColor(const char *mapfile, const IplImage *input, IplImage **output);
#endif /* _RECTIFY_H */
