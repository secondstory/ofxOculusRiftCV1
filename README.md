
*ofxOculusRiftCV1*

ofxOculusRiftCV1 is an addon for using the OculusRift consumer version headset (CV1) in openFrameworks.  

It uses some source code from James George's ofxOculusDKII addon.

*Requirements*

Install the latest Oculus Rift PC runtime.

*Usage*

In ofApp.h

```
ofxOculusRiftCV1 cv1;
```

In ofApp.cpp

```c++
void ofApp::setup(){

	cv1.init();
}

void ofApp::update(){

	cv1.update();
}

void ofApp::draw(){

	ofEnableDepthTest();

	// draw left eye first
	cv1.begin(ovrEye_Left);
	ofClear(0, 255, 255);
	drawScene();
	cv1.end(ovrEye_Left);

	// then right eye
	// fyi--the order is critical!
	cv1.begin(ovrEye_Right);
	ofClear(0, 255, 255);
	drawScene();
	cv1.end(ovrEye_Right);

	// display the stereo view in the OF window (optional)
	cv1.draw(0, 0);
}
```

*Notes*

* This is a work-in-progress. Please add any feature requests through the issues panel.

* This addon only works on Windows. 

* It does not currently support Oculus Touch Controllers. 
