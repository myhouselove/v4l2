all:
	g++ -g -o v4l2 main.cpp V4L2Camera.cpp \
	-ljpeg -L/usr/lib \
	-lpthread

