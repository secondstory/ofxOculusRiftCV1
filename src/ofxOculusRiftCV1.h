#pragma once

#include "ofMain.h"
#include "OVR_CAPI.h"
#include "Win32_GLAppUtil.h"

// Include the Oculus SDK
#include "OVR_CAPI_GL.h"
#include "CAPI_GLE.h"

#if defined(_WIN32)
#include <dxgi.h> // for GetDefaultAdapterLuid
#pragma comment(lib, "dxgi.lib")
#endif

#define BLIT_TEXTURE 0

class ofxOculusRiftCV1 {

public:

	ofxOculusRiftCV1();
	~ofxOculusRiftCV1();

	bool init();
	void close();
	void update();
	void begin(ovrEyeType whichEye);
	void end(ovrEyeType whichEye);

	void draw(float x, float y );
	void draw(float x, float y, float w, float h );
	void drawScene(); 

	bool getIsInitialized();

	ofRectangle getHMDSize();
	ovrHmdDesc & getHMD();

	void getHMDTrackingState(ofVec3f & position, ofQuaternion & orientation);
	ovrTrackingState getHMDTrackingState();
	ofMatrix4x4 getHMDOrientationMatrix();

protected:

	void logError();

	static ovrGraphicsLuid GetDefaultAdapterLuid();
	static int Compare(const ovrGraphicsLuid& lhs, const ovrGraphicsLuid& rhs);

	ovrSession session;
	ovrGraphicsLuid luid;
	ovrHmdDesc hmdDesc;
	double sensorSampleTime;

	ovrSizei windowSize;
	ovrPosef eyeRenderPose[2];
	ovrEyeRenderDesc eyeRenderDesc[2];
	ovrRecti eyeRenderViewport[2];

	TextureBuffer *		eyeRenderTexture[2];
	DepthBuffer   *		eyeDepthBuffer[2];
	ovrMirrorTexture	mirrorTexture;
	GLuint				mirrorTextureID;
	GLuint				mirrorFBO;
	long long			frameIndex;
	ofShader			mirrorShader;

	bool bOVRInitialized;
};

