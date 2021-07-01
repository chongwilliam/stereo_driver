# Python 3.6
# This program grabs the Point Grey's RGB image from redis and displays (the same images) side by side
# run with ~/anaconda3/bin/python3 Visualize_RGB_jpg.py
# need sudo apt-get install python3-tk
# pip 3 install redis

__author__ = 'Gerald'

import cv2
import numpy as np
import tkinter as tk
import redis
import threading
import time
import os
from pathlib import Path
import shutil


class GUI(object):
    def __init__(self, frame):
        # global variables
        self.overlap = 0
        self.fullscreen = False
        self.stereo = True
        self.drawCross = False
        # self.left_image_updown = 0
        # self.right_image_updown = 0
        # self.left_image_leftright = 0
        # self.right_image_leftright = 0 
        self.left_coord = [0, 0]  # current top left edge (row, col)
        self.right_coord = [0, 0]  # current top left edge (row, col)

        # sub-sample image parameters
        self.sample_height = 0  # temp init, don't change
        self.height_factor = 1.0
        self.sample_width = 0  # temp init, don't change 
        self.width_factor = 0.8
        self.pixel_step = 2

        hostIP = '192.168.1.103'
        # hostIP = 'localhost'
        hostPort = 6379
        self.r = redis.StrictRedis(host=hostIP, port=hostPort, db=0)
        self.keyLeft  = "ptgrey_left"
        self.keyRight = "ptgrey_right"
        self.savedOffsetsFilename = "./saved_offsets.txt"
        self.backupOffsetsFilename = "./last_offsets_backup.txt"

        # self.imgLeft = cv2.imread("Stereo_empire_left.jpg")
        # self.imgRight = cv2.imread("Stereo_empire_right.jpg")[:, :1844, :]

        self.imgSize = None

        self.root = frame #tk.Tk()
        self.root.geometry("600x80+100+100")
        self.screenWidth = self.root.winfo_screenwidth()
        self.screenHeight = self.root.winfo_screenheight()

        # threaded save flags
        self.img_params = [int(cv2.IMWRITE_JPEG_QUALITY), 95] # range from (0 to 100, default at 95)        
        self.img_available = 0  # 0 if false, 1 if true
        self.write_done = 1  # 0 if false, 1 if true
        self.img_to_write = None  # copy of image to be displayed if writing is done 
        self.compressed_img_to_write = None  # compressed image 
        self.f_cnt = 0  # file number to start 
        self.img_count = 0  # counter to only save every nth frame 

    def increaseOverlap(self):
        # self.overlap += 10
        # self.overlap = min(self.imgSize[1], self.overlap) 
        self.left_coord[1] -= self.pixel_step
        self.right_coord[1] += self.pixel_step

    def decreaseOverlap(self):
        # self.overlap -= 10
        # self.overlap = max(0, self.overlap)
        self.left_coord[1] += self.pixel_step
        self.right_coord[1] -= self.pixel_step

    def moveLeftUp(self):
        # self.left_image_updown += 10
        # self.left_image_updown = min(self.imgSize[0], self.left_image_updown)
        self.left_coord[0] -= self.pixel_step

    def moveLeftDown(self):
        # self.left_image_updown -= 10
        # self.left_image_updown = max(-self.imgSize[0], self.left_image_updown)
        self.left_coord[0] += self.pixel_step

    def moveRightUp(self):
        # self.right_image_updown += 10
        # self.right_image_updown = min(self.imgSize[0], self.right_image_updown)
        self.right_coord[0] -= self.pixel_step

    def moveRightDown(self):
        # self.right_image_updown -= 10
        # self.right_image_updown = max(-self.imgSize[0], self.right_image_updown)
        self.right_coord[0] += self.pixel_step 

    def setOverlap(self, value):
        self.overlap = min(self.imgSize[1], max(0, value))

    def setLeftUpdown(self, value):
        self.left_image_updown = value
        self.left_image_updown = min(self.imgSize[0], self.left_image_updown)
        self.left_image_updown = max(-self.imgSize[0], self.left_image_updown)

    def setRightUpdown(self, value):
        self.right_image_updown = value
        self.right_image_updown = min(self.imgSize[0], self.right_image_updown)
        self.right_image_updown = max(-self.imgSize[0], self.right_image_updown)

    def saveOffsets(self, filename):
        offsets_log_file = open(filename, "w")
        offsets_log_file.writelines([str(self.left_coord[0])+"\n", str(self.left_coord[1])+"\n", str(self.right_coord[0])+"\n", str(self.right_coord[1])+"\n"])
        offsets_log_file.close()

    def loadOffsets(self,filename):
        offsets_log_file = open(filename, "r")
        self.left_coord[0] = int(offsets_log_file.readline())
        self.left_coord[1] = int(offsets_log_file.readline())
        self.right_coord[0] = int(offsets_log_file.readline())
        self.right_coord[1] = int(offsets_log_file.readline())
        offsets_log_file.close()

    def toggleCrosshairs(self):
        self.drawCross = not self.drawCross 

    def toggleFullscreen(self):
        self.fullscreen = not self.fullscreen

    def setStereoView(self):
        self.stereo = True

    def setMonoView(self):
        self.stereo = False


    def videoLoop(self):
        cv2.namedWindow("GUI", cv2.WINDOW_NORMAL)




        # ----------------------------
        # Load Camera Calibration
        # ----------------------------

        # # Set camera mode
        # # self_cam_mode = 'mac'
        # self_cam_mode = 'ptgrey'
        #
        # if 'mac' == self_cam_mode:
        #     with np.load('calib_mac.npz') as X:
        #         mtx, dist, _, _ = [X[i] for i in ('mtx','dist','rvecs','tvecs')]
        #         # print ('mtx = \n', mtx)
        #         cap = cv2.VideoCapture(0)
        # elif 'ptgrey' == self_cam_mode:
        #     with np.load('calib_ptgrey.npz') as X:
        #         mtx, dist, _, _ = [X[i] for i in ('mtx','dist','rvecs','tvecs')]
        #         # print ('mtx = \n', mtx)
        #



        # cv2.createTrackbar('Overlap', "GUI", 0, self.imgSize[1], self.setOverlap)

        ctr = 0
        while True:

            start_time_loop = time.time()

            strLeft = self.r.get(self.keyLeft) # 0.01 sec
            # print('strLeft = ', strLeft)
            arrayLeft = np.fromstring(strLeft, np.uint8)
            imgLeft = cv2.imdecode(arrayLeft, cv2.IMREAD_COLOR) # 0.015 sec

            strRight = self.r.get(self.keyRight)
            arrayRight = np.fromstring(strRight, np.uint8)
            imgRight = cv2.imdecode(arrayRight, cv2.IMREAD_COLOR)

            # print ctr
            ctr += 1
            print('ctr = ', ctr)

            # print(str(imgLeft.shape) + "\n")

            #imgLeft = self.imgLeft.copy()

            self.imgSize = imgLeft.shape

            # strRight = self.r.get(self.keyRight)
            # arrayRight = np.fromstring(strRight, np.uint8)
            # imgRight = cv2.imdecode(arrayRight, cv2.IMREAD_COLOR)

            # imgRight = imgLeft

            #imgRight = self.imgRight.copy()

            # creating overlays for easier navigation
            alpha = 0.99
            offset = 100

            # overlayRight = np.zeros(imgRight.shape, imgRight.dtype)
            # cv2.line(overlayRight, (110, 200), (110, imgRight.shape[0] - 200), (0, 204, 0), 2)
            # cv2.line(overlayRight, (95, 200), (125, 200), (0, 204, 0), 4)
            # cv2.putText(overlayRight, "0", (60, 200 + 10), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 204, 0), 2)
            # cv2.line(overlayRight, (95, imgRight.shape[0]/2), (125, imgRight.shape[0]/2), (0, 204, 0), 4)
            # cv2.putText(overlayRight, "-50", (20, imgRight.shape[0]/2 + 10), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 204, 0), 2)
            # cv2.line(overlayRight, (100, 200 + (imgRight.shape[0] - 200 - 200)/4), (120, 200 + (imgRight.shape[0] - 200 - 200)/4), (0, 204, 0), 2)
            # cv2.line(overlayRight, (100, imgRight.shape[0] - 200 - (imgRight.shape[0] - 200 - 200)/4), (120, imgRight.shape[0] - 200 - (imgRight.shape[0] - 200 - 200)/4), (0, 204, 0), 2)
            # cv2.line(overlayRight, (95, imgRight.shape[0] - 200), (125, imgRight.shape[0] - 200), (0, 204, 0), 4)
            # cv2.putText(overlayRight, "-100", (10, imgRight.shape[0] - 190), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 204, 0), 2)
            #
            # # current depth marker
            # cv2.fillPoly(overlayRight, np.array([[[140, 250], [120, 260], [140, 270]]], dtype=np.int32), (0, 204, 0))
            #
            # cv2.addWeighted(overlayRight, alpha, imgRight, 1, 0, imgRight)
            #
            # # copying right overlay
            # overlayLeft = np.zeros(imgLeft.shape, imgLeft.dtype)
            # overlayLeft[:, offset:, :] = overlayRight[:, :-offset, :]
            # cv2.addWeighted(overlayLeft, alpha, imgLeft, 1, 0, imgLeft)

            # Initialize the right image, top-right coordinate
            self.sample_height = int(self.height_factor * self.imgSize[0])
            self.sample_width = int(self.width_factor * self.imgSize[1])  
            if ctr == 1:  # initialize only at the start 
                self.right_coord[0] = int(self.imgSize[0]/2 - self.sample_height/2)
                # self.right_coord[1] = self.imgSize[1] - self.sample_width - 1  # top left corner of right image 
                self.right_coord[1] = int(self.imgSize[1]/2 - self.sample_width/2)
                self.left_coord[0] = int(self.imgSize[0]/2 - self.sample_height/2)
                # self.left_coord[1] = self.imgSize[1] - self.sample_width - 1  # top left corner of right image 
                self.left_coord[1] = int(self.imgSize[1]/2 - self.sample_width/2)

            if self.stereo:
            # #     resized_imgLeft = cv2.resize(imgLeft[max(0,self.left_image_updown):min(self.imgSize[0],self.imgSize[0]+self.left_image_updown), :, :], (imgLeft.shape[0], imgLeft.shape[1]))
            # #     resized_imgRight = cv2.resize(imgRight[max(0,self.right_image_updown):min(self.imgSize[0],self.imgSize[0]+self.right_image_updown), :, :], (imgRight.shape[0], imgRight.shape[1]))

            #     # Crop image in the up/down direction, and re-size back to a 1:1 resolution (why fx is also scaled by the same factor?)
            #     resized_imgLeft = cv2.resize(imgLeft[max(0,self.left_image_updown):min(self.imgSize[0],self.imgSize[0]+self.left_image_updown), :, :], (0, 0), \
            #                                     fx=self.imgSize[0]/(self.imgSize[0]-np.abs(self.left_image_updown)), fy=self.imgSize[0]/(self.imgSize[0]-np.abs(self.left_image_updown)))

            #     resized_imgRight = cv2.resize(imgRight[max(0,self.right_image_updown):min(self.imgSize[0],self.imgSize[0]+self.right_image_updown), :, :], (0, 0), \
            #                                     fx=self.imgSize[0]/(self.imgSize[0]-np.abs(self.right_image_updown)), fy=self.imgSize[0]/(self.imgSize[0]-np.abs(self.right_image_updown)))

            #     # Get the stereo image frames truncated from the resized images 
            #     # self.imgSize is the original single frame image from redis
            #     resized_imgLeft = resized_imgLeft[:,int((np.abs(resized_imgLeft.shape[1]-self.imgSize[1]))/2):int((self.imgSize[1]+np.abs(resized_imgLeft.shape[1]-self.imgSize[1])/2)),:]

            #     resized_imgRight = resized_imgRight[:,int((np.abs(resized_imgRight.shape[1]-self.imgSize[1]))/2):int((self.imgSize[1]+np.abs(resized_imgRight.shape[1]-self.imgSize[1])/2)):,:]

            #     # Concatenate the left and right images horizontally to form the stereo image with horizontal overlap. Problem if the self.overlap cuts off too much  
            #     stackedImg = np.hstack((resized_imgLeft[:, :-(self.overlap + 1), :], resized_imgRight[:, self.overlap:, :]))
            #     # stackedImg = np.hstack((imgLeft[:, :-(self.overlap + 1), :], imgRight[:, self.overlap:, :]))

                ### Windowing Method ###              
                sampled_imgLeft = imgLeft[max(0, self.left_coord[0]) : min(self.left_coord[0] + self.sample_height, self.imgSize[0] - 1), \
                                        max(0, self.left_coord[1]) : min(self.left_coord[1] + self.sample_width, self.imgSize[1] - 1), :]
                
                sampled_imgRight = imgRight[max(0, self.right_coord[0]) : min(self.right_coord[0] + self.sample_height, self.imgSize[0] - 1), \
                                        max(0, self.right_coord[1]) : min(self.right_coord[1] + self.sample_width, self.imgSize[1] - 1), :]

                #crosshairs
                if(self.drawCross == True):
                    cv2.drawMarker(sampled_imgLeft, (int(sampled_imgLeft.shape[1]/2), int(sampled_imgLeft.shape[0]/2)),  (0, 0, 255), cv2.MARKER_CROSS, 100, 2);
                    cv2.drawMarker(sampled_imgRight, (int(sampled_imgRight.shape[1]/2), int(sampled_imgRight.shape[0]/2)),  (0, 0, 255), cv2.MARKER_CROSS, 100, 2);

                sampled_imgLeft = cv2.resize(sampled_imgLeft, (self.imgSize[1], self.imgSize[0]))
                sampled_imgRight = cv2.resize(sampled_imgRight, (self.imgSize[1], self.imgSize[0]))

                stackedImg = np.hstack((sampled_imgLeft, sampled_imgRight))

            else:
                stackedImg = imgLeft
                stackedImg = imgRight

            displayImg = np.zeros((self.screenHeight, self.screenWidth, self.imgSize[2]), np.uint8)

            # Re-size to recover the displayImg resolution 
            if float(self.screenHeight)/self.screenWidth > float(stackedImg.shape[0])/stackedImg.shape[1]:
                stackedImg = cv2.resize(stackedImg, (0, 0), fx=self.screenWidth/float(stackedImg.shape[1]), fy=self.screenWidth/float(stackedImg.shape[1]))
                offset = int((self.screenHeight - stackedImg.shape[0])/2)
                displayImg[offset:offset+stackedImg.shape[0], :, :] = stackedImg
            else:
                stackedImg = cv2.resize(stackedImg, (0, 0), fx=self.screenHeight/float(stackedImg.shape[0]), fy=self.screenHeight/float(stackedImg.shape[0]))
                offset = int((self.screenWidth - stackedImg.shape[1])/2)
                displayImg[:, offset:offset+stackedImg.shape[1], :] = stackedImg

            # displayImg = imgLeft
            # img = displayImg

            # # -------------------------------
            # # Undistort
            # # -------------------------------
            # h, w = img.shape[:2]
            # newcameramtx, roi = cv2.getOptimalNewCameraMatrix(mtx,dist,(w,h),1,(w,h))
            # # undistort
            # undst = cv2.undistort(img, mtx, dist, None, newcameramtx)
            # # crop the image
            # # x,y,w,h = roi
            # # undst = undst[y:y+h, x:x+w]
            # displayImg = undst

            # -------------------------------
            # Draw cross-hair
            # -------------------------------
            if not self.stereo:
                h, w = displayImg.shape[:2]
                cv2.drawMarker(displayImg, (int(w/2), int(h/2)),  (0, 0, 255), cv2.MARKER_CROSS, 40, 2);

            # # -------------------------------
            # # Rotate Image
            # # -------------------------------
            # displayImg = np.rot90(displayImg)


            # print (displayImg.shape)
            start_time_show = time.time()
            cv2.imshow("GUI", displayImg) # 0.06 sec

            if self.fullscreen:
                cv2.setWindowProperty("GUI", cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)
            else:
                cv2.setWindowProperty("GUI", cv2.WINDOW_NORMAL, cv2.WINDOW_NORMAL)

            # trigger imwrite thread if displayImg is available (non-blocking)
            self.img_available = 1  # just to protect the first loop   
            if self.write_done == 1:
                self.img_to_write = displayImg                             

            key = cv2.waitKey(1)
            if key == 63234 or (key & 0xFF == ord('a')): # left arrow or 'a'
                self.increaseOverlap()
                # self.saveOffsets(self.backupOffsetsFilename)
            if key == 63235 or (key & 0xFF == ord('d')): # right arrow or 'd'
                self.decreaseOverlap()
                # self.saveOffsets(self.backupOffsetsFilename)
            if key & 0xFF == ord('y'):
                self.moveLeftUp()
                # self.saveOffsets(self.backupOffsetsFilename)
            if key & 0xFF == ord('h'):
                self.moveLeftDown()
                # self.saveOffsets(self.backupOffsetsFilename)
            if key & 0xFF == ord('i'):
                self.moveRightUp()
                # self.saveOffsets(self.backupOffsetsFilename)
            if key & 0xFF == ord('k'):
                self.moveRightDown()
                # self.saveOffsets(self.backupOffsetsFilename)
            if key & 0xFF == ord('f'):
                self.toggleFullscreen()
            if key & 0xFF == ord('s'):
                self.setStereoView()
            if key & 0xFF == ord('m'):
                self.setMonoView()
            if key & 0xFF == ord('r'):
                cv2.imwrite("./saved_images_stereo/o1k_stero_"+str(time.time())+".jpg", displayImg)
                print("image saved \n")
            if key & 0xFF == ord('l'):
                self.saveOffsets(self.savedOffsetsFilename)
            if key & 0xFF == ord('o'):
                self.loadOffsets(self.savedOffsetsFilename)
            if key & 0xFF == ord('c'):
                self.toggleCrosshairs()
            if key & 0xFF == ord('q') or key == 27: # 27 = esc
                break
            print("--- %s seconds show ---" % (time.time() - start_time_show))

            print("--- %s seconds loop ---" % (time.time() - start_time_loop))

        cv2.destroyAllWindows()
        cv2.waitKey(1)

    def save_jpg(self):
        while(True):
            if self.img_available == 1:            
                self.write_done = 0            
                self.f_name = '/home/vision/py-flask-video-stream/images/o1k_prev_2x1.jpg'
                # self.compressed_img_to_write = cv2.imencode('.jpg', self.img_to_write, self.img_params)            
                cv2.imwrite(self.f_name, self.img_to_write, self.img_params)  
                # self.f_cnt += 1
                # if self.f_cnt > 0:  # save buffer of size N 
                    # self.f_cnt = 0
                src_path = '/home/vision/py-flask-video-stream/images/o1k_prev_2x1.jpg'
                trg_path = '/home/vision/py-flask-video-stream/images/o1k_2x1.jpg'
                os.rename(src_path, trg_path)
                # shutil.copy(self.f_name, trg_path)
                # for src_file in Path(src_path).glob('*.*'):
                #     shutil.copy(src_file, trg_path)
                self.write_done = 1
                print("Saved Picture (Ctrl-C again to quit thread)")
                time.sleep(1/20)  # sleep for fps duration to avoid saving the same image repeatedly

def main():
    root = tk.Tk()
    app = GUI(root)
    # start thread for saving jpg to file 
    print("Started save thread")
    # dir = '/home/vision/py-flask-video-stream/images/'
    # for f in os.listdir(dir):
        # os.remove(os.path.join(dir, f))
    # dir = '/home/vision/py-flask-video-stream/images_to_write/'
    # for f in os.listdir(dir):
        # os.remove(os.path.join(dir, f))
    save_thread = threading.Thread(target=app.save_jpg)
    save_thread.start()
    print("Started video loop")
    app.videoLoop()


if __name__ == '__main__':
    main()
