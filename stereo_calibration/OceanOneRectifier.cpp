#include "RectificationMap.h"
#include <fstream>
#include <iostream>

#include "cRedis.h"
#include <opencv2/core/core.hpp>

#define HSIZE 1280
#define VSIZE 960
// completely incomplete and sketchy code
// meant to suggest what is needed

bool gWritefile = true;

// Globals
const std::string img_left = "ptgrey_left";
const std::string img_right = "ptgrey_right";
cRedis* m_redis;


// make these functions members of a class
// It will need internal state of
//		cvMapX, cvMapY, height, width
// and perhaps also a result image of the stacked dimensions
//		resultImage

/*
* The procedure will be:
* 1) read the mapping lookup table
*		it provides the index values (x and y float coordinates) for filling a destination image from a source image
*		the destination and source will be the left and right images stack vertically
*		These will be the same size -- as the originals and the resulting image intended to be displayed will a subset of this -- 
*		    positioned in the top left corner of the destination image by the mappers
* 2) Resample the images using remap()
*		Once the resampling is completed you will window the two desired frames from the larger destination image
*			and stack these for transmission to the host (if done on the head) or to the display
*			Likely horizontal stacking if the latter, and either vertical stacking if the former 9on head) of sent as separate images
* 3) Transmit or display the results
*		The display code will determine how to window and perhaps zoom the resulting destination images for stereo viewing.
* To save on constant reallocations you may want to set up the buffers and reuse them on each new frame
* framerect (the overlap window on the Left and Right images) is not know to the mapper -- the calibrater determines the overlap region and it is pssed separately
* 
*/

void initializemapframe(std::string mapfile, Rect framerect)
{
	// mapfile = name of maps.bin mapping file
	// use OpenCV to map
	int xlo = framerect.x;
	int ylo = framerect.y;
	int xhi = framerect.width;
	int yhi = framerect.height;	
	int reswidth = xhi - xlo + 1;
	int resheight = (yhi - ylo + 1) * 2;		// yhi is twice the image height -- frames will be delivered stacked vertically

	RectificationMap rmap;
	rmap.Load(mapfile);
	height = rmap.mosaicHeight;
	width = rmap.mosaicWidth;

	std::vector<float> mapX = rmap.GetMapX();
	std::vector<float> mapY = rmap.GetMapY();
	Mat cvMapX = Mat(height, width, CV_32FC1, mapX.data());
	Mat cvMapY = Mat(height, width, CV_32FC1, mapY.data());
	Mat intermediateImage = Mat(height * 2, width, CV_8UC3);
	Mat resultImage = Mat(resheight, reswidth, CV_8UC3);		// make a single image for output
}

void mapframe(Mat inimage, Mat& outimage)
{
	remap(inimage, outImage, cvMapX, cvMapY, INTER_LINEAR);
}

void testmapper(std::string mapfile, Mat left, Mat right, Rect framerect, Mat &resimage)
{
	initializemapframe(mapfile, framerect);
	// Rect framerect = Rect(xlo, ylo, xhi, yhi);
	// left and right = Mat(960, 1280, CV_8UC3)

	Mat inimagepair = vconcat(left, right);
	mapframe(inimagepair, intermediateImage);

	intermediateImage(outrect).copyTo(resultImage);
	if (gWritefile) imwrite("resultimage.jpg, resultImage);
		// do something with the resultimage -- separate into dest-left and dest-right and send to host or hconcat(destleft, destright) for stereo display
}