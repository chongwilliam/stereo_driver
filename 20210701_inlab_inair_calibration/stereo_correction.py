# imports
import numpy as np
import cv2
import glob
import argparse
import sys
import os
import pdb 

# size calib array
numEdgeX = 8
numEdgeY = 5

# preface
exitCode = 0

# get directories
pathDir = str(os.path.dirname(os.path.realpath(__file__)))
# pathDir = pathDir[:-17]
# pathCalib = pathDir + "\\CalibData" + "\\chess"
pathCalib = pathDir

try:
    # define pair
    p = 1
    # cal_path = pathCalib + "\\pair" + str(p)
    # cal_path = pathCalib 

    # images_right = glob.glob(cal_path + '/Right/*.jpg')
    # images_left = glob.glob(cal_path + '/Left/*.jpg')
    images_right = sorted(glob.glob('./Right/*.tiff'))
    images_left = sorted(glob.glob('./Left/*.tiff'))
    # images_left.sort()
    # images_right.sort()

    # termination criteria
    criteria = (cv2.TermCriteria_EPS +
                        cv2.TermCriteria_MAX_ITER, 100, 0.001)
    criteria_cal = (cv2.TermCriteria_EPS +
                        cv2.TermCriteria_MAX_ITER, 100, 1e-5)

    # prepare object points, like (0,0,0); (1,0,0); ...; (6,5,0)
    objp = np.zeros((numEdgeX*numEdgeY, 3), np.float32)
    objp[:, :2] = np.mgrid[0:numEdgeX, 0:numEdgeY].T.reshape(-1, 2)

    objpoints = []     # 3d points in real world space
    imgpoints_l = []   # 2d points in image plane for calibration
    imgpoints_r = []   # 2d points in image plane for calibration

    for i, fname in enumerate(images_right):
        print(str(i+1) + " out of " + str(len(images_right)))
        img_l = cv2.imread(images_left[i], -1)
        img_r = cv2.imread(images_right[i], -1)

        # convert to cv2
        img_l = cv2.cvtColor(img_l, cv2.COLOR_BGR2GRAY)
        img_r = cv2.cvtColor(img_r, cv2.COLOR_BGR2GRAY)

        # find the chess board corners
        ret_l, corners_l = cv2.findChessboardCorners(img_l, (numEdgeX, numEdgeY), None)
        ret_r, corners_r = cv2.findChessboardCorners(img_r, (numEdgeX, numEdgeY), None)

        objpoints.append(objp)

        if ret_l is True:
            print("image " + str(i+1) + "left - io")
            rt = cv2.cornerSubPix(img_l, corners_l, (11, 11),
                                  (-1, -1), criteria)
            imgpoints_l.append(corners_l)

        if ret_r is True:
            print("image " + str(i+1) + "right - io")
            rt = cv2.cornerSubPix(img_r, corners_r, (11, 11),
                                  (-1, -1), criteria)
            imgpoints_r.append(corners_r)

        # get shape
        img_shape = img_l.shape[::-1]


    ### CALIBRATION ###
    # calibrate left camera
    rt, M1, d1, r1, t1 = cv2.calibrateCamera(
    objpoints, imgpoints_l, img_shape, None, None)

    # calibrate right camera
    rt, M2, d2, r2, t2 = cv2.calibrateCamera(
    objpoints, imgpoints_r, img_shape, None, None)

    # stereo calibration
    flags = (cv2.CALIB_FIX_K5 + cv2.CALIB_FIX_K6)

    stereocalib_criteria = (cv2.TERM_CRITERIA_MAX_ITER +
                        cv2.TERM_CRITERIA_EPS, 100, 1e-5)

    #flags = 0
    #flags = cv2.CALIB_USE_INTRINSIC_GUESS
    #flags = cv2.CALIB_FIX_PRINCIPAL_POINT
    #flags = cv2.CALIB_FIX_ASPECT_RATIO
    #flags = cv2.CALIB_ZERO_TANGENT_DIST
    #flags = cv2.CALIB_FIX_INTRINSIC   
    #flags = cv2.CALIB_FIX_FOCAL_LENGTH
    #flags = cv2.CALIB_FIX_K1...6
    #flags = cv2.CALIB_RATIONAL_MODEL
    #flags = cv2.CALIB_THIN_PRISM_MODEL
    #flags = cv2.CALIB_SAME_FOCAL_LENGTH
    #flags = cv2.CALIB_FIX_S1_S2_S3_S4

    # flags = (cv2.CALIB_FIX_PRINCIPAL_POINT | cv2.CALIB_FIX_ASPECT_RATIO | cv2.CALIB_FIX_FOCAL_LENGTH |
    #          cv2.CALIB_FIX_INTRINSIC | cv2.CALIB_FIX_K3 | cv2.CALIB_FIX_K4 | cv2.CALIB_FIX_K5 |
    #          cv2.CALIB_FIX_K6)

    flags = (cv2.CALIB_FIX_PRINCIPAL_POINT | cv2.CALIB_FIX_ASPECT_RATIO | cv2.CALIB_RATIONAL_MODEL
             | cv2.CALIB_FIX_K3 | cv2.CALIB_FIX_K4 | cv2.CALIB_FIX_K5 | cv2.CALIB_FIX_K6)
    T = np.zeros((3, 1), dtype=np.float64)
    R = np.eye(3, dtype=np.float64)

    ret, M1, d1, M2, d2, R, T, E, F = cv2.stereoCalibrate(
            objpoints, imgpoints_l,
            imgpoints_r, M1, d1, M2,
            d2, img_shape,
            criteria = stereocalib_criteria,
            flags=flags)

    # pdb.set_trace()

    # get new optimal camera matrix
    newCamMtx1, roi1 = cv2.getOptimalNewCameraMatrix(M1, d1, img_shape, 0, img_shape)
    newCamMtx2, roi2 = cv2.getOptimalNewCameraMatrix(M2, d2, img_shape, 0, img_shape)

    # rectification and undistortion maps which can be used directly to correct the stereo pair
    (rectification_l, rectification_r, projection_l,
        projection_r, disparityToDepthMap, ROI_l, ROI_r) = cv2.stereoRectify(
            M1, d1, M2, d2, img_shape, R, T,
            None, None, None, None, None,
            #cv2.CALIB_ZERO_DISPARITY,                  # principal points of each camera have the same pixel coordinates in rect views
            alpha=0)                                   # alpha=1 no pixels lost, alpha=0 pixels lost

    leftMapX, leftMapY = cv2.initUndistortRectifyMap(
        M1, d1, rectification_l, projection_l,
        img_shape, cv2.CV_32FC1)
    rightMapX, rightMapY = cv2.initUndistortRectifyMap(
        M2, d2, rectification_r, projection_r,
        img_shape, cv2.CV_32FC1)

    # pdb.set_trace()

    ### UNCALIBRATED RECTIFICATION ###
    imgpoints_l_undis = []
    imgpoints_r_undis = []

    for i, fname in enumerate(images_right):
        img_l = cv2.imread(images_left[i])
        img_r = cv2.imread(images_right[i])

        # convert to cv2
        img_l = cv2.cvtColor(img_l, cv2.COLOR_BGR2GRAY)
        img_r = cv2.cvtColor(img_r, cv2.COLOR_BGR2GRAY)

        # undistort
        img_l_undis = cv2.undistort(img_l, M1, d1, None, newCamMtx1)
        img_r_undis = cv2.undistort(img_r, M2, d2, None, newCamMtx2)

        # cv2.imshow('Left', cv2.cvtColor(img_l_undis, cv2.COLOR_RGB2BGR))
        # cv2.waitKey(0)
        # cv2.imshow('Right', cv2.cvtColor(img_r_undis, cv2.COLOR_RGB2BGR))
        # cv2.waitKey(0)

        # find the chess board corners in undistorted image
        ret_l_undis, corners_l_undis = cv2.findChessboardCorners(img_l_undis, (numEdgeX, numEdgeY), None)
        ret_r_undis, corners_r_undis = cv2.findChessboardCorners(img_r_undis, (numEdgeX, numEdgeY), None)

        if ret_l_undis is True and ret_r_undis is True:
            rt = cv2.cornerSubPix(img_l_undis, corners_l_undis, (11, 11), (-1, -1), criteria)
            for j in range(0, len(rt)):
                x = rt[j][0,:]
                imgpoints_l_undis.append(x)

        if ret_r_undis is True and ret_l_undis is True:
            rt = cv2.cornerSubPix(img_r_undis, corners_r_undis, (11, 11), (-1, -1), criteria)
            for j in range(0, len(rt)):
                x = rt[j][0,:]
                imgpoints_r_undis.append(x)

    # convert to np array
    imgpoints_l_undis = np.array(imgpoints_l_undis)  # (880, 2)
    imgpoints_r_undis = np.array(imgpoints_r_undis)  # (840, 2)
    # imgpoints_l_undis = imgpoints_l_undis[0:840, :]

    # compute rectification uncalibrated
    ret, uncRectMtx1, uncRectMtx2 = cv2.stereoRectifyUncalibrated(imgpoints_l_undis, imgpoints_r_undis, F, img_shape)

    ### SAVE RELEVANT VALUES ###
    with open('calibration.npy', 'wb') as f:
        np.save(f, M1)
        np.save(f, M2)
        np.save(f, d1)
        np.save(f, d2)
        np.save(f, newCamMtx1)        
        np.save(f, newCamMtx2)
        np.save(f, leftMapX)
        np.save(f, leftMapY)
        np.save(f, rightMapX)
        np.save(f, rightMapY)

    ### REMAPPING ###
    for jj in range(0, 23):
        # load images and convert to cv2 format
        img_l = cv2.imread(images_left[jj])
        # img_l = cv2.cvtColor(img_l, cv2.COLOR_BGR2GRAY)
        img_l_undis = cv2.undistort(img_l, M1, d1, None, newCamMtx1)
        img_r = cv2.imread(images_right[jj])
        # img_r = cv2.cvtColor(img_r, cv2.COLOR_BGR2GRAY)
        img_r_undis = cv2.undistort(img_r, M2, d2, None, newCamMtx2)

        # remap
        imglCalRect = cv2.remap(img_l, leftMapX, leftMapY, cv2.INTER_LINEAR)
        imgrCalRect = cv2.remap(img_r, rightMapX, rightMapY, cv2.INTER_LINEAR)
        numpyHorizontalCalibRect = np.hstack((imglCalRect, imgrCalRect))  

        # warp for uncalibrated rectification
        imglUncalRect = cv2.warpPerspective(img_l_undis, uncRectMtx1, img_shape)
        imgrUncalRect = cv2.warpPerspective(img_r_undis, uncRectMtx2, img_shape)
        numpyHorizontalUncalibRect = np.hstack((imglUncalRect, imgrUncalRect))  

        ### SHOW RESULTS ###
        # # calculate point arrays for epipolar lines
        # lineThickness = 5
        # lineColor = [0, 255, 0]
        # numLines = 20
        # interv = round(img_shape[0] / numLines)
        # x1 = np.zeros((numLines, 1))
        # y1 = np.zeros((numLines, 1))
        # x2 = np.full((numLines, 1), (3*img_shape[1]))
        # y2 = np.zeros((numLines, 1))
        # for jj in range(0, numLines):
        #     y1[jj] = jj * interv
        # y2 = y1

        # for jj in range(0, numLines):
        #     cv2.line(numpyHorizontalCalibRect, (int(x1[jj]), int(y1[jj])), (int(x2[jj]), int(y2[jj])),
        #              lineColor, lineThickness)
        #     cv2.line(numpyHorizontalUncalibRect, (int(x1[jj]), int(y1[jj])), (int(x2[jj]), int(y2[jj])),
        #              lineColor, lineThickness)
        # cv2.namedWindow("calibRect", cv2.WINDOW_NORMAL)
        # cv2.namedWindow("uncalibRect", cv2.WINDOW_NORMAL)
        # cv2.imshow("calibRect", numpyHorizontalCalibRect)
        # cv2.imshow("uncalibRect", numpyHorizontalUncalibRect)
        # cv2.waitKey()

        cv2.imwrite('./Left/' + str(jj).zfill(2) + '_left_cal.jpg', imglCalRect)
        cv2.imwrite('./Right/' + str(jj).zfill(2) + '_right_cal.jpg', imgrCalRect)


except (IOError, ValueError):
    print("An I/O error or a ValueError occurred")
except:
    print("An unexpected error occurred")
    raise
