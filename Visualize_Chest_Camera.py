import cv2
import redis
import numpy as np

if __name__ == '__main__':
	hostIP = '192.168.1.92'
	hostPort = 6379
	r = redis.StrictRedis(host=hostIP, port=hostPort, db=0)

	fullscreen = 0

	while True:
		data = r.get('chest::camera')
		array = np.fromstring(data, np.uint8)
		img = cv2.imdecode(array, cv2.IMREAD_COLOR)
		cv2.imshow("Image", cv2.rotate(img, cv2.cv2.ROTATE_180))
		if fullscreen:
			cv2.setWindowProperty("Image", cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)
		else:
			cv2.setWindowProperty("Image", cv2.WINDOW_NORMAL, cv2.WINDOW_NORMAL)

		key = cv2.waitKey(1)
		if key & 0xFF == ord('f'):
			if fullscreen == 0:
				fullscreen = 1
			else:
				fullscreen = 0
		elif key & 0xFF == ord('q'):
			cv2.destroyAllWindows()
			break
