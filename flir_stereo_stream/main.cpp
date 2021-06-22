
// Camera sn..22 has failed.



//=============================================================================
// Copyright (c) 2001-2018 FLIR Systems, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of FLIR
// Integrated Imaging Solutions, Inc. ("Confidential Information"). You
// shall not disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with FLIR Integrated Imaging Solutions, Inc. (FLIR).
//
// FLIR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. FLIR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================

/**
 *  @example AcquisitionMultipleCamera.cpp
 *
 *  @brief AcquisitionMultipleCamera.cpp shows how to capture images from
 *  multiple cameras simultaneously. It relies on information provided in the
 *  Enumeration, Acquisition, and NodeMapInfo examples.
 *
 *  This example reads similarly to the Acquisition example, except that loops
 *  and vectors are used to allow for simultaneous acquisitions.
 */

// #include "Spinnaker.h"
// #include "SpinGenApi/SpinnakerGenApi.h"
#include <ueye.h>
#include <iostream>
#include <sstream>
#include <fstream>

#include "cRedis.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <chrono>

using namespace std;
using namespace cv;

// Globals
cRedis* m_redis;
cv::Mat image;
HIDS hCam = 1;  // camera id
const std::string chest_camera_key = "chest::camera";

// Function Prototypes
void writeJpgToRedis(cv::Mat& a_cv_img, const std::string& a_key);
void writePngToRedis(cv::Mat& a_cv_img);
int ConvertToCVmat(ImagePtr pImage);
// Added
void recordJpgRedis(const std::string& dir);

int main() {
    cout << "Starting Chest Camera Stream" << endl;
    cout << "----------------------------" << endl;

    // camera init
    int returned_status = is_InitCamera(&hCam, NULL);
    cout << "Camera Initialization Status: " << returned_status;

    // set image size 
    int img_width = 1280;
    int img_height = 720;

    // memory allocation
    char *captured_image = NULL;
    int memory_id = 0;

    returned_status = is_AllocImageMem(hCam, img_width, img_height, 24, &captured_image, &memory_id);
    cout << "Image Allocation Status: " << returned_status;
    returned_status = is_SetImageMem(hCam, captured_image, memory_id);
    cout << "Image Memory Set Status: " << returned_status;

    char c;
    while(1) {
        // capture frame
        is_CaptureVideo(hCam, 10);
        IplImage* tmp_img;
        tmp_img = cvCreateImageHeader(cvSize(img_width, img_height), IPL_DEPTH_8U, 3);

        // assign image address to IplImage data pointer
        tmp_img->imageData = captured_image;

        // create openCV Mat object with frame obtained form camera
        image = cv::Mat(tmp_img, false);

        // send image to redis 
        writeJpgToRedis(image, camera_key);
    }

    is_ExitCamera(hCam);

}

void writeJpgToRedis(cv::Mat& a_cv_img, const std::string& a_key)
{
    vector<int> params;
    params.push_back(CV_IMWRITE_JPEG_QUALITY);  // applies compression factor
    params.push_back(50);


    // std::chrono::high_resolution_clock::time_point start = PoorMansHelper::startTimer();
    vector<uchar> buff;
    cv::imencode(".jpg", a_cv_img, buff, params); // 4978 microseconds
    // PoorMansHelper::endTimer(start, "imencode jpg");


    // start = PoorMansHelper::startTimer();
    m_redis->setJpegBinary(a_key, buff); // 170 microseconds
    // PoorMansHelper::endTimer(start, "setJpegBinary to redis");
}

void writePngToRedis(cv::Mat& a_cv_img)
{
    vector<int> params;
    params.push_back(CV_IMWRITE_PNG_COMPRESSION);
    params.push_back(100);


    // std::chrono::high_resolution_clock::time_point start = PoorMansHelper::startTimer();
    vector<uchar> buff;
    cv::imencode(".png", a_cv_img, buff, params); //81262 microseconds
    // PoorMansHelper::endTimer(start, "imencode png");


    // start = PoorMansHelper::startTimer();
    m_redis->setJpegBinary("pt_grey_png", buff); //  microseconds
    // PoorMansHelper::endTimer(start, "setJpegBinary to redis PNG");
}

int ConvertToCVmat(ImagePtr pImage)
{
	int result = 0;
	ImagePtr convertedImage = pImage->Convert(PixelFormat_BGR8, NEAREST_NEIGHBOR);

	unsigned int XPadding = convertedImage->GetXPadding();
	unsigned int YPadding = convertedImage->GetYPadding();
	unsigned int rowsize = convertedImage->GetWidth();
	unsigned int colsize = convertedImage->GetHeight();

	//image data contains padding. When allocating Mat container size, you need to account for the X,Y image data padding.
	Mat cvimg = cv::Mat(colsize + YPadding, rowsize + XPadding, CV_8UC3, convertedImage->GetData(), convertedImage->GetStride());
	namedWindow("current Image", CV_WINDOW_AUTOSIZE);
	imshow("current Image", cvimg);
	resizeWindow("current Image", rowsize / 2, colsize / 2);
	waitKey(1);//otherwise the image will not display...

	return result;
}

// Records uncompressed L and R images in dir with timestamps
void recordJpgRedis(std::string& dir, int& count, cv::Mat& a_cv_img_L, cv::Mat& a_cv_img_R) 
{
    std::string l_name = dir + "Left/" + std::to_string(count) + ".jpg";
    std::string r_name = dir + "Right/" + std::to_string(count) + ".jpg";  
    cv::imwrite(l_name, a_cv_img_L);
    cv::imwrite(r_name, a_cv_img_R);
}
