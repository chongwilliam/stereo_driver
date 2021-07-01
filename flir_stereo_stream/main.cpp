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

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <fstream>

#include "cRedis.h"
#include "PoorMansHelper.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <chrono>
#include <thread>
#include <mutex>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;
using namespace cv;

// Globals
cRedis* m_redis;
std::mutex m_mutex;

cv::Mat m_cv_img_left;
cv::Mat m_cv_img_right;

// Function Prototypes
void writeJpgToRedis(cv::Mat& a_cv_img, const std::string& a_key);
void writePngToRedis(cv::Mat& a_cv_img);
void writeJpgToRedisStereo(cv::Mat& a_cv_img_L, const std::string& a_key_L, cv::Mat& a_cv_img_R, const std::string& a_key_R);
int ConvertToCVmat(ImagePtr pImage);
// Added
void recordJpgRedis(const std::string& dir);
const std::string capture_key = "stereo::capture::trigger";  // 1 for capture, 0 default
const std::string reset_capture_key = "stereo::capture::reset";  // 1 to reset (make new directory), 0 default
ImagePtr right_image;
ImagePtr left_image;

#ifdef _DEBUG
// Disables heartbeat on GEV cameras so debugging does not incur timeout errors
int DisableHeartbeat(CameraPtr pCam, INodeMap & nodeMap, INodeMap & nodeMapTLDevice)
{
    cout << "Checking device type to see if we need to disable the camera's heartbeat..." << endl << endl;
    //
    // Write to boolean node controlling the camera's heartbeat
    //
    // *** NOTES ***
    // This applies only to GEV cameras and only applies when in DEBUG mode.
    // GEV cameras have a heartbeat built in, but when debugging applications the
    // camera may time out due to its heartbeat. Disabling the heartbeat prevents
    // this timeout from occurring, enabling us to continue with any necessary debugging.
    // This procedure does not affect other types of cameras and will prematurely exit
    // if it determines the device in question is not a GEV camera.
    //
    // *** LATER ***
    // Since we only disable the heartbeat on GEV cameras during debug mode, it is better
    // to power cycle the camera after debugging. A power cycle will reset the camera
    // to its default settings.
    //

    CEnumerationPtr ptrDeviceType = nodeMapTLDevice.GetNode("DeviceType");
    if (!IsAvailable(ptrDeviceType) && !IsReadable(ptrDeviceType))
    {
        cout << "Error with reading the device's type. Aborting..." << endl << endl;
        return -1;
    }
    else
    {
        if (ptrDeviceType->GetIntValue() == DeviceType_GEV)
        {
            cout << "Working with a GigE camera. Attempting to disable heartbeat before continuing..." << endl << endl;
            CBooleanPtr ptrDeviceHeartbeat = nodeMap.GetNode("GevGVCPHeartbeatDisable");
            if (!IsAvailable(ptrDeviceHeartbeat) || !IsWritable(ptrDeviceHeartbeat))
            {
                cout << "Unable to disable heartbeat on camera. Continuing with execution as this may be non-fatal..." << endl << endl;
            }
            else
            {
                ptrDeviceHeartbeat->SetValue(true);
                cout << "WARNING: Heartbeat on GigE camera disabled for the rest of Debug Mode." << endl;
                cout << "         Power cycle camera when done debugging to re-enable the heartbeat..." << endl << endl;
            }
        }
        else
        {
            cout << "Camera does not use GigE interface. Resuming normal execution..." << endl << endl;
        }
    }
    return 0;
}
#endif


int ConfigureCustomImageSettings(INodeMap & nodeMap)
{
	int result = 0;

	cout << endl << endl << "*** CONFIGURING CUSTOM IMAGE SETTINGS ***" << endl << endl;

	try
	{
		//
		// Apply mono 8 pixel format
		//
		// *** NOTES ***
		// Enumeration nodes are slightly more complicated to set than other
		// nodes. This is because setting an enumeration node requires working
		// with two nodes instead of the usual one.
		//
		// As such, there are a number of steps to setting an enumeration node:
		// retrieve the enumeration node from the nodemap, retrieve the desired
		// entry node from the enumeration node, retrieve the integer value from
		// the entry node, and set the new value of the enumeration node with
		// the integer value from the entry node.
		//
		// Retrieve the enumeration node from the nodemap
		CEnumerationPtr ptrPixelFormat = nodeMap.GetNode("PixelFormat");
		if (IsAvailable(ptrPixelFormat) && IsWritable(ptrPixelFormat))
		{
			// Retrieve the desired entry node from the enumeration node
			CEnumEntryPtr ptrPixelFormatRGB8 = ptrPixelFormat->GetEntryByName("RGB8");
			if (IsAvailable(ptrPixelFormatRGB8) && IsReadable(ptrPixelFormatRGB8))
			{
				// Retrieve the integer value from the entry node
				int64_t pixelFormatRGB8 = ptrPixelFormatRGB8->GetValue();

				// Set integer as new value for enumeration node
				ptrPixelFormat->SetIntValue(pixelFormatRGB8);

				cout << "Pixel format set to " << ptrPixelFormat->GetCurrentEntry()->GetSymbolic() << "..." << endl;
			}
			else
			{
				cout << "Pixel format RGB 8 not available..." << endl;
			}
		}
		else
		{
			cout << "Pixel format not available..." << endl;
		}

	}
	catch (Spinnaker::Exception &e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}





// This function acquires and saves 10 images from each device.
int AcquireImages(CameraList camList)
{
    int result = 0;
    CameraPtr pCam = nullptr;

    m_redis = new cRedis();

    // Setup for recording
    cout << endl << "*** STEREO CAPTURE SETUP ***" << endl << endl;
    // time_t _tm = time(NULL);
    // struct tm* curtime = localtime(&_tm);
    // std::string rec_date_time = asctime(curtime);
    std::string dir = "/home/radiator2/stereo_images_recording/";
    std::string save_dir;
    int img_cnt = 0;
    // int r_img_cnt = 0;
    // int l_img_cnt = 0;
    m_redis->setRedis(capture_key, 0);  // set to default value
    // m_redis->setRedis(reset_capture_key, 0);  // set to default value

    cout << endl << "*** IMAGE ACQUISITION ***" << endl << endl;

    try
    {
        //
        // Prepare each camera to acquire images
        //
        // *** NOTES ***
        // For pseudo-simultaneous streaming, each camera is prepared as if it
        // were just one, but in a loop. Notice that cameras are selected with
        // an index. We demonstrate pseduo-simultaneous streaming because true
        // simultaneous streaming would require multiple process or threads,
        // which is too complex for an example.
        //
        // Serial numbers are the only persistent objects we gather in this
        // example, which is why a vector is created.
        //
        vector<gcstring> strSerialNumbers(camList.GetSize());

        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Set acquisition mode to continuous
            CEnumerationPtr ptrAcquisitionMode = pCam->GetNodeMap().GetNode("AcquisitionMode");
            if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
            {
                cout << "Unable to set acquisition mode to continuous (node retrieval; camera " << i << "). Aborting..." << endl << endl;
                return -1;
            }

            CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
            if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
            {
                cout << "Unable to set acquisition mode to continuous (entry 'continuous' retrieval " << i << "). Aborting..." << endl << endl;
                return -1;
            }

            int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

            ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

            cout << "Camera " << i << " acquisition mode set to continuous..." << endl;

#ifdef _DEBUG
            cout << endl << endl << "*** DEBUG ***" << endl << endl;

            // If using a GEV camera and debugging, should disable heartbeat first to prevent further issues
            if (DisableHeartbeat(pCam, pCam->GetNodeMap(), pCam->GetTLDeviceNodeMap()) != 0)
            {
                return -1;
            }

            cout << endl << endl << "*** END OF DEBUG ***" << endl << endl;
#endif

            // Retrieve device serial number for filename
            strSerialNumbers[i] = "";

            CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

            if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
            {
                strSerialNumbers[i] = ptrStringSerial->GetValue();
                cout << "Camera " << i << " serial number set to " << strSerialNumbers[i] << "..." << endl;
            }


            // -------------------------
            // Gerald: Set Buffer
            // -------------------------

            // Retrieve Stream Parameters device nodemap
            Spinnaker::GenApi::INodeMap & sNodeMap = pCam->GetTLStreamNodeMap();

            // Retrieve Buffer Handling Mode Information
            CEnumerationPtr ptrHandlingMode = sNodeMap.GetNode("StreamBufferHandlingMode");
            if (!IsAvailable(ptrHandlingMode) || !IsWritable(ptrHandlingMode))
            {
                cout << "Unable to set Buffer Handling mode (node retrieval). Aborting..." << endl << endl;
                return -1;
            }

            CEnumEntryPtr ptrHandlingModeEntry = ptrHandlingMode->GetCurrentEntry();
            if (!IsAvailable(ptrHandlingModeEntry) || !IsReadable(ptrHandlingModeEntry))
            {
                cout << "Unable to set Buffer Handling mode (Entry retrieval). Aborting..." << endl << endl;
                return -1;
            }

            // Set stream buffer Count Mode to manual
            CEnumerationPtr ptrStreamBufferCountMode = sNodeMap.GetNode("StreamBufferCountMode");
            if (!IsAvailable(ptrStreamBufferCountMode) || !IsWritable(ptrStreamBufferCountMode))
            {
                cout << "Unable to set Buffer Count Mode (node retrieval). Aborting..." << endl << endl;
                return -1;
            }

            CEnumEntryPtr ptrStreamBufferCountModeManual = ptrStreamBufferCountMode->GetEntryByName("Manual");
            if (!IsAvailable(ptrStreamBufferCountModeManual) || !IsReadable(ptrStreamBufferCountModeManual))
            {
                cout << "Unable to set Buffer Count Mode entry (Entry retrieval). Aborting..." << endl << endl;
                return -1;
            }

            ptrStreamBufferCountMode->SetIntValue(ptrStreamBufferCountModeManual->GetValue());

            cout << "Stream Buffer Count Mode set to manual..." << endl;

            // ----------------

            ptrHandlingModeEntry = ptrHandlingMode->GetEntryByName("NewestOnly");
            // ptrHandlingModeEntry = ptrHandlingMode->GetEntryByName("NewestFirst");
            ptrHandlingMode->SetIntValue(ptrHandlingModeEntry->GetValue());
            cout << endl << endl << "Buffer Handling Mode has been set to " << ptrHandlingModeEntry->GetDisplayName() << endl;

            cout << endl;

            // Begin acquiring images
            pCam->BeginAcquisition();

            cout << "Camera " << i << " started acquiring images..." << endl;

        }

        //
        // Retrieve, convert, and save images for each camera
        //
        // *** NOTES ***
        // In order to work with simultaneous camera streams, nested loops are
        // needed. It is important that the inner loop be the one iterating
        // through the cameras; otherwise, all images will be grabbed from a
        // single camera before grabbing any images from another.
        //
        const unsigned int k_numImages = 10;

        remove( "record.csv" );

        // for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
        int ctr = 0;
        chrono::time_point<chrono::high_resolution_clock>  timepoint_start_global, timepoint_start_loop, timepoint_cur;
        timepoint_start_global = chrono::high_resolution_clock::now();
        while (true)
        {
            std::cout << ctr << std::endl;
            if (ctr % 100 == 0)
            {
              timepoint_cur = chrono::high_resolution_clock::now();
              auto elapsed = chrono::duration_cast<chrono::milliseconds>(timepoint_cur - timepoint_start_loop);
              double elapsed_seconds = elapsed.count()/1000.;
              double Hz = 100./elapsed_seconds;

              auto elapsed_global = chrono::duration_cast<chrono::milliseconds>(timepoint_cur - timepoint_start_global);
              double elapsed_seconds_global = elapsed_global.count()/1000.;

              cout << "./StreamPtGreyToRedis: " << "ctr = " << ctr << " at " << Hz << " Hz (" << elapsed_seconds << " sec for 100 cycles). Total duration: " << elapsed_seconds_global << " sec." << endl;
              timepoint_start_loop = chrono::high_resolution_clock::now();

              // Get system temperature
              std::system("sensors | grep Package | awk '{print $4;}' > temperature_buf.txt"); // execute the UNIX command "ls -l >test.txt"
              // std::string temperature = std::ifstream("temperature_buf.txt").rdbuf();
              // std::cout << std::ifstream("temperature_buf.txt").rdbuf();
              std::ifstream t("temperature_buf.txt");
              std::string temperature((std::istreambuf_iterator<char>(t)),  std::istreambuf_iterator<char>());
              temperature.erase(std::remove(temperature.begin(), temperature.end(), '\n'), temperature.end());
              std::cout << "temperature = " << temperature << std::endl;

              // current date/time based on current system
              time_t now = time(0);

              // convert now to string form
              char* dt = ctime(&now);
              cout << "The local date and time is: " << dt << endl;
              std::string date_time(dt);
              date_time.erase(std::remove(date_time.begin(), date_time.end(), '\n'), date_time.end());

              // Record to CSV
              std::ofstream file("record.csv", std::ofstream::out | std::ofstream::app);
              if (file.is_open())
              {
                file << ctr << ", " <<  elapsed_seconds_global << ", " << Hz << ", " << temperature  << ", " << camList.GetSize() << ", " << date_time << "\n";
              }

            }

            for (unsigned int i = 0; i < camList.GetSize(); i++)
            {
                try
                {
                    // Select camera
                    pCam = camList.GetByIndex(i);

                    // Retrieve next received image and ensure image completion
                    // m_mutex.lock();
                    // cout << "Before pCam->GetNextImage" << endl;
                    ImagePtr pResultImage = pCam->GetNextImage();
                    // cout << "After pCam->GetNextImage" << endl;
                    // m_mutex.unlock();

                    if (pResultImage->IsIncomplete())
                    {
                        cout << "Image incomplete with image status " << pResultImage->GetImageStatus() << "..." << endl << endl;
                    }
                    else
                    {
                        // Print image information
                        if (ctr % 100 == 0)
                        {
                          cout << "Camera " << i  << ", serial: " << strSerialNumbers[i] << /* " grabbed image " << imageCnt << */ ", width = " << pResultImage->GetWidth() << ", height = " << pResultImage->GetHeight()  /* << "ctr = " << ctr << " at " << Hz << " Hz." */ << endl;
                        }

                        // Convert image to mono 8
                        // ImagePtr convertedImage = pResultImage->Convert(PixelFormat_Mono8, HQ_LINEAR);
                        ImagePtr convertedImage = pResultImage->Convert(PixelFormat_BGR8, HQ_LINEAR);
                        // ImagePtr convertedImage = pResultImage->Convert(PixelFormat_BayerBG8, HQ_LINEAR);
                        // ImagePtr convertedImage = pResultImage;

#if 0
                        // Create a unique filename
                        ostringstream filename;

                        filename << "AcquisitionMultipleCamera-";
                        if (strSerialNumbers[i] != "")
                        {
                            filename << strSerialNumbers[i].c_str();
                        }
                        else
                        {
                            filename << i;
                        }
                        filename << "-" << imageCnt << ".jpg";

                        // Save image
                        convertedImage->Save(filename.str().c_str());

                        cout << "Image saved at " << filename.str() << endl;
#endif

#if 1

                        int xPadding = convertedImage->GetXPadding();
                        int yPadding = convertedImage->GetYPadding();
                        int row_size = convertedImage->GetWidth();
                        int col_size = convertedImage->GetHeight();

                        //                std::chrono::high_resolution_clock::time_point start = PoorMansHelper::startTimer();
                        cv::Mat cv_img = cv::Mat(col_size + yPadding, row_size + xPadding, CV_8UC3, convertedImage->GetData(), convertedImage->GetStride() ); // TODO: Make vector to support multiple cameras

                        // std::cout << "cam idx = " << i << std::endl;
                        // std::cout << "strSerialNumbers[i] = " << strSerialNumbers[i] << std::endl;

                        if (strcmp("17175120", strSerialNumbers[i]) == 0  )
                        {
                          // writeJpgToRedis(cv_img, "ptgrey_left");
                          // cv::imshow("ptgrey_left", cv_img);
                          // cv::waitKey(1);
                          m_cv_img_left = cv_img.clone();
                          // std::cout << "store left" << std::endl;
                          left_image = pResultImage;
                //       		// save image if requested
	               //          if (m_redis->getRedisDouble(capture_key) == 1)
				            // {
			            	// 	save_dir = dir + "Left/" + std::to_string(r_img_cnt) + ".png";
			            	// 	pResultImage->Save(save_dir.c_str());
				            //     r_img_cnt += 1;
				            //     if (r_img_cnt == l_img_cnt)
				            //     {
				            //     	m_redis->setRedis(capture_key, 0);
				            //     }
				            // }
                        }
                        else if (strcmp("17084522", strSerialNumbers[i]) == 0  )
                        {
                          // writeJpgToRedis(cv_img, "ptgrey_right");
                          // cv::imshow("ptgrey_right", cv_img);
                          // cv::waitKey(1);
                          m_cv_img_right = cv_img.clone();
                          // std::cout << "store right" << std::endl;
                          right_image = pResultImage;
                    		// save image if requested
	               //          if (m_redis->getRedisDouble(capture_key) == 1)
				            // {
			            	// 	save_dir = dir + "Right/" + std::to_string(l_img_cnt) + ".png";
			            	// 	pResultImage->Save(save_dir.c_str());
				            //     l_img_cnt += 1;
				            //     if (r_img_cnt == l_img_cnt)
				            //     {
				            //     	m_redis->setRedis(capture_key, 0);
				            //     }				            }
                        }
                        // std::this_thread::sleep_for(std::chrono::milliseconds(10));

#endif

                        // ConvertToCVmat(pResultImage);

                    }

                    // Release image
                    pResultImage->Release();

                    // cout << endl;
                }
                catch (Spinnaker::Exception &e)
                {
                    cout << "Error: " << e.what() << endl;
                    result = -1;
                }
            }


            if (ctr > 10)
            {

              // --------------------------------------------
              // CONCATENATE AND WRITE IMAGE TO REDIS
              // --------------------------------------------

              // std::chrono::high_resolution_clock::time_point start = PoorMansHelper::startTimer();
              // cv::Mat cv_img_concat;
              // cv::hconcat(m_cv_img_left, m_cv_img_right, cv_img_concat); // 32900 microseconds.  Syntax-> hconcat(source1,source2,destination);
              // PoorMansHelper::endTimer(start, "hconcat");
              //
              // // m_mutex.lock();
              // std::chrono::high_resolution_clock::time_point start2 = PoorMansHelper::startTimer();
              // writeJpgToRedis(cv_img_concat,  "ptgrey_concat"); // 32793 microseconds
              // PoorMansHelper::endTimer(start2, "writeJpgToRedis Concat");
              // // m_mutex.unlock();

#if 1
              // --------------------------------------------
              // WRITE TWO SEPARATE IMAGES TO REDIS
              // --------------------------------------------

              // m_mutex.lock();
              // std::cout << "befor redis left" << std::endl;
              // std::chrono::high_resolution_clock::time_point start = PoorMansHelper::startTimer();
              writeJpgToRedis(m_cv_img_left,  "ptgrey_left"); // 17275 microseconds
              // PoorMansHelper::endTimer(start, "writeJpgToRedis Left");
              // m_mutex.unlock();
              // m_mutex.lock();
              // std::cout << "redis middle" << std::endl;
              writeJpgToRedis(m_cv_img_right, "ptgrey_right");
              // std::cout << "after redis right" << std::endl;
              // m_mutex.unlock();
              // std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Makes app run longer but still crashes.

      			// save image if requested
                if (m_redis->getRedisDouble(capture_key) == 1)
	            {
            		save_dir = dir + "Right/" + std::to_string(img_cnt) + "_right" + ".tiff";
            		right_image->Save(save_dir.c_str());
            		save_dir = dir + "Left/" + std::to_string(img_cnt) + "_left" + ".tiff";
            		left_image->Save(save_dir.c_str());
            		img_cnt += 1;
	    		    m_redis->setRedis(capture_key, 0);  // set to default value
            	}

            // // Query redis key to reset stereo image capture
            // if (m_redis->getRedisDouble(reset_capture_key) == 1)
            // {
            //     _tm = time(NULL);
            //     curtime = localtime(&_tm);
            //     rec_date_time = asctime(curtime);
            //     dir = "~/stereo_images_recording/" + rec_date_time;
            //     img_cnt = 0;
            //     m_redis->setRedis(reset_capture_key, 0);
            // }

            // // Query redis key for stereo image capture
            // if (m_redis->getRedisDouble(capture_key) == 1)
            // {
            //     recordJpgRedis(dir, img_cnt, m_cv_img_left, m_cv_img_right);
            //     img_cnt += 1;
            //     m_redis->setRedis(capture_key, 0);
            // }
#else
              // -----------------------------------------------------
              // WRITE TWO SEPARATE IMAGES TO REDIS USING PIPELINING
              // -----------------------------------------------------
              writeJpgToRedisStereo(m_cv_img_left,  "ptgrey_left", m_cv_img_right, "ptgrey_right");
#endif


            }
            ctr++;
        }

        //
        // End acquisition for each camera
        //
        // *** NOTES ***
        // Notice that what is usually a one-step process is now two steps
        // because of the additional step of selecting the camera. It is worth
        // repeating that camera selection needs to be done once per loop.
        //
        // It is possible to interact with cameras through the camera list with
        // GetByIndex(); this is an alternative to retrieving cameras as
        // CameraPtr objects that can be quick and easy for small tasks.
        //
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // End acquisition
            camList.GetByIndex(i)->EndAcquisition();
        }
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }
    return result;
}

// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap & nodeMap, unsigned int camNum)
{
    int result = 0;

    cout << "Printing device information for camera " << camNum << "..." << endl << endl;

    FeatureList_t features;
    CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
    if (IsAvailable(category) && IsReadable(category))
    {
        category->GetFeatures(features);

        FeatureList_t::const_iterator it;
        for (it = features.begin(); it != features.end(); ++it)
        {
            CNodePtr pfeatureNode = *it;
            cout << pfeatureNode->GetName() << " : ";
            CValuePtr pValue = (CValuePtr)pfeatureNode;
            cout << (IsReadable(pValue) ? pValue->ToString() : "Node not readable");
            cout << endl;
        }
    }
    else
    {
        cout << "Device control information not available." << endl;
    }
    cout << endl;

    return result;
}

// This function acts as the body of the example; please see NodeMapInfo example
// for more in-depth comments on setting up cameras.
int RunMultipleCameras(CameraList camList)
{
    int result = 0;
    int err = 0;
    CameraPtr pCam = nullptr;

    try
    {
        //
        // Retrieve transport layer nodemaps and print device information for
        // each camera
        //
        // *** NOTES ***
        // This example retrieves information from the transport layer nodemap
        // twice: once to print device information and once to grab the device
        // serial number. Rather than caching the nodemap, each nodemap is
        // retrieved both times as needed.
        //
        cout << endl << "*** DEVICE INFORMATION ***" << endl << endl;

        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Retrieve TL device nodemap
            INodeMap & nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

            // Print device information
            result = PrintDeviceInfo(nodeMapTLDevice, i);
        }

        //
        // Initialize each camera
        //
        // *** NOTES ***
        // You may notice that the steps in this function have more loops with
        // less steps per loop; this contrasts the AcquireImages() function
        // which has less loops but more steps per loop. This is done for
        // demonstrative purposes as both work equally well.
        //
        // *** LATER ***
        // Each camera needs to be deinitialized once all images have been
        // acquired.
        //
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Initialize camera
            pCam->Init();

            // ---------------------------------------------------------------------------------
            // Gerald Mod: Change image format here. Fix for greyscale issue w. Blackfly cams.
            // ---------------------------------------------------------------------------------

            // Retrieve GenICam nodemap
            INodeMap & nodeMap = pCam->GetNodeMap();

            // Configure custom image settings
            err = ConfigureCustomImageSettings(nodeMap);
            if (err < 0)
            {
              return err;
            }




        }

        // Acquire images on all cameras
        result = result | AcquireImages(camList);

        //
        // Deinitialize each camera
        //
        // *** NOTES ***
        // Again, each camera must be deinitialized separately by first
        // selecting the camera and then deinitializing it.
        //
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Deinitialize camera
            pCam->DeInit();
        }
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// Example entry point; please see Enumeration example for more in-depth
// comments on preparing and cleaning up the system.
int main(int /*argc*/, char** /*argv*/)
{
    // Since this application saves images in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.
    FILE *tempFile = fopen("test.txt", "w+");
    if (tempFile == nullptr)
    {
        cout << "Failed to create file in current folder.  Please check "
            "permissions."
            << endl;
        cout << "Press Enter to exit..." << endl;
        getchar();
        return -1;
    }
    fclose(tempFile);
    remove("test.txt");

    int result = 0;

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: "
        << spinnakerLibraryVersion.major << "."
        << spinnakerLibraryVersion.minor << "."
        << spinnakerLibraryVersion.type << "."
        << spinnakerLibraryVersion.build << endl << endl;

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    // Remove chest cam
    if (camList.GetBySerial("14468721") != NULL)
      camList.RemoveBySerial("14468721");

    unsigned int numCameras = camList.GetSize();

    cout << "Number of cameras detected: " << numCameras << endl << endl;

    // Finish if there are no cameras
    if (numCameras == 0)
    {
        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();

        cout << "Not enough cameras!" << endl;
        cout << "Done! Press Enter to exit..." << endl;
        getchar();

        return -1;
    }

    // Run example on all cameras
    cout << endl << "Running example for all cameras..." << endl;

    result = RunMultipleCameras(camList);

    cout << "Example complete..." << endl << endl;

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}




void writeJpgToRedis(cv::Mat& a_cv_img, const std::string& a_key)
{
    vector<int> params;
	@@ -882,25 +138,6 @@ void writePngToRedis(cv::Mat& a_cv_img)
    // PoorMansHelper::endTimer(start, "setJpegBinary to redis PNG");
}

void writeJpgToRedisStereo(cv::Mat& a_cv_img_L, const std::string& a_key_L, cv::Mat& a_cv_img_R, const std::string& a_key_R)
{
    vector<int> params;
    params.push_back(CV_IMWRITE_JPEG_QUALITY);
    params.push_back(50);


    // std::chrono::high_resolution_clock::time_point start = PoorMansHelper::startTimer();
    vector<uchar> buff_L;
    vector<uchar> buff_R;

    cv::imencode(".jpg", a_cv_img_L, buff_L, params); // 4978 microseconds
    cv::imencode(".jpg", a_cv_img_R, buff_R, params);

    m_redis->setJpegBinaryStereo(a_key_L, buff_L, a_key_R, buff_R); // 170 microseconds
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
