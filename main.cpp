#include<iostream>
using namespace std;

#include "V4L2Camera.h"

int main()
{
	
	V4L2Camera *cam = new V4L2Camera();

	cam->Open("/dev/video1");
	cam->Init();
cout<<"Init: ok"<<endl;
	//cam->setParameters(640,480,V4L2_PIX_FMT_MJPEG);
	cam->StartStreaming ();
	cout<<"StartStreaming: ok"<<endl;
	cam->start();
	//cam->GrabPreviewFrame();

	cout<<"hello world"<<endl;
	return 0;
}