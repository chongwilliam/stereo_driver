#include <ueye.h>
#include <iostream>
#include <sstream>
#include <fstream>

#include "cRedis.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace std;
using namespace cv;

// Globals
cRedis* m_redis;
cv::Mat image;
IplImage* tmpImg;
SENSORINFO sInfo;
const std::string chest_key = "chest::camera";
HIDS hCam = 0;  // automatically find available camera

// Function Prototypes
void writeJpgToRedis(cv::Mat& a_cv_img, const std::string& a_key);
void writePngToRedis(cv::Mat& a_cv_img);
// Added
void recordJpgRedis(std::string& dir, int& count, cv::Mat& a_cv_img);

int main() {
  m_redis = new cRedis();
  cout << "Success Code: " << IS_SUCCESS << endl;

	// camera initialization
	int returnedStatus = is_InitCamera(&hCam, NULL);
	cout << "Status Init: "<< returnedStatus << endl;

  // set image size
  is_GetSensorInfo(hCam, &sInfo);
  // int imgWidth = sInfo.nMaxWidth;
  // int imgHeight = sInfo.nMaxHeight;
  int imgWidth = 1280, imgHeight = 960;  // tune this parameter for lower bandwidth

  uint32_t entries;
  is_ImageFormat(hCam, IMGFRMT_CMD_GET_NUM_ENTRIES, &entries, sizeof(entries));

  // Query resolution option and set image quality
  char formats[sizeof(IMAGE_FORMAT_LIST) + (entries-1)*sizeof(IMAGE_FORMAT_INFO)];
  IMAGE_FORMAT_LIST *formatList = (IMAGE_FORMAT_LIST*)formats;
  formatList->nNumListElements = entries;
  formatList->nSizeOfListEntry = sizeof(IMAGE_FORMAT_INFO);
  is_ImageFormat(hCam, IMGFRMT_CMD_GET_LIST, formats, sizeof(formats));
  bool found = false;
  printf("Supported Resolutions: \n");
  for (int k=0; k<entries; k++) {
      IMAGE_FORMAT_INFO *info = &formatList->FormatInfo[k];
      printf("w: %d, h: %d\n", info->nWidth, info->nHeight);
      if (info->nWidth == imgWidth && info->nHeight == imgHeight) {
          is_ImageFormat(hCam, IMGFRMT_CMD_SET_FORMAT, &info->nFormatID, sizeof(info->nFormatID));
          found = true;
        }
    }
  if (!found) {
      std::cerr << "Unsupported resolution: " << imgWidth << "x" << imgHeight << std::endl;
  }

  // Need to find out the memory size of the pixel and the colour mode
  int nColorMode;
  int nBitsPerPixel = 24;

  if (sInfo.nColorMode == IS_COLORMODE_BAYER)
  {
      // For color camera models use RGB24 mode
      nColorMode = IS_CM_BGR8_PACKED;
      nBitsPerPixel = 24;
  }
  else if (sInfo.nColorMode == IS_COLORMODE_CBYCRY)
  {
      // For CBYCRY camera models use RGB32 mode
      nColorMode = IS_CM_BGRA8_PACKED;
      nBitsPerPixel = 32;
  }
  else
  {
      // For monochrome camera models use Y8 mode
      nColorMode = IS_CM_MONO8;
      nBitsPerPixel = 24;
  }

  // Test capture
	char *capturedImage = NULL;
	int memoryId = 0;

	// allocate memory for image capturing
	returnedStatus = is_AllocImageMem(hCam, imgWidth, imgHeight, nBitsPerPixel, &capturedImage, &memoryId);
	cout << "Status AllocImage: " << returnedStatus << endl;

	returnedStatus = is_SetImageMem(hCam, capturedImage, memoryId);
	cout << "Status SetImageMem: " << returnedStatus << endl;

	char c;
  cout << "Starting Stream: " << endl;
	for(;;){
		// capture frame
		is_CaptureVideo(hCam, 0);
		// IplImage* tmpImg;
		tmpImg = cvCreateImageHeader(cvSize(imgWidth, imgHeight), IPL_DEPTH_8U, 3);

		// assign image address to IplImage data pointer
		tmpImg->imageData = capturedImage;

		// create openCV Mat object with frame obtained form camera
		// cv::Mat image = cv::Mat(tmpImg, false);
    image = cv::cvarrToMat(tmpImg);

    // cout << "CV Stats: " << image.rows << "," << image.cols << endl;

    // write image to Redis
    writeJpgToRedis(image, chest_key);

	}

	is_ExitCamera(hCam);
  return 0;
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

// Records uncompressed L and R images in dir with timestamps
void recordJpgRedis(std::string& dir, int& count, cv::Mat& a_cv_img)
{
    std::string l_name = dir + "Chest/" + std::to_string(count) + ".jpg";
    cv::imwrite(l_name, a_cv_img);
}
