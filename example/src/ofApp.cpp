#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){

	// initialize the device

	bool bOK = cv1.init();

	if (!bOK) 
	{
		ofLogNotice() << "OculusRiftCV1 failed to initialize";
	}
	else 
	{
		ofLogNotice() << "Initialized OculusRiftCV1";

		ofRectangle bounds = cv1.getHMDSize();
		ofSetWindowShape(bounds.width, bounds.height);
	}
}

//--------------------------------------------------------------
void ofApp::update(){

	cv1.update();
}

//--------------------------------------------------------------
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

//----------------------------------------------------------
void ofApp::drawScene() {

	ofSeedRandom(666);

	for (int i = 0; i<100; i++) {

		float theta = ofRandom(TWO_PI);
		float phi = ofRandom(PI);
		float dist = ofRandom(0.5, 5.0);
		float brightness = ofRandom(255);

		ofVec3f pos;
		pos.x = cos(theta) * sin(phi) * dist;
		pos.y = sin(theta) * sin(phi) * dist;
		pos.z = cos(phi) * dist;

		ofSetColor(brightness);

		ofPushMatrix();
		ofTranslate(pos);
		ofDrawBox(0.5);
		ofPopMatrix();
	}
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
