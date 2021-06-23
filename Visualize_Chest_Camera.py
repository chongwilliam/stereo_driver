import cv2
import redis
import numpy as np

if __name__ == '__main__':
	r = redis.Redis(host='192.168.1.91', port=6379)

	while True:
		data = r.get('chest::camera')
		img = np.frombuffer(data, dtype=np.uint8).reshape(720,1280,3)  # need to specify the exact shape here

		cv2.imshow("Image", img)
		key = cv2.waitKey(1)
