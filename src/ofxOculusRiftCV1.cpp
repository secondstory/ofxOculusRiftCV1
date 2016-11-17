
#include "ofxOculusRiftCV1.h"

#define STRINGIFY(x) #x

ofQuaternion toOf(const Quatf& q) {
	return ofQuaternion(q.x, q.y, q.z, q.w);
}

ofMatrix4x4 toOf(const Matrix4f& m) {
	return ofMatrix4x4(m.M[0][0], m.M[1][0], m.M[2][0], m.M[3][0],
		m.M[0][1], m.M[1][1], m.M[2][1], m.M[3][1],
		m.M[0][2], m.M[1][2], m.M[2][2], m.M[3][2],
		m.M[0][3], m.M[1][3], m.M[2][3], m.M[3][3]);
}

Matrix4f toOVR(const ofMatrix4x4& m) {
	const float* cm = m.getPtr();
	return Matrix4f(cm[0], cm[1], cm[2], cm[3],
		cm[4], cm[5], cm[6], cm[7],
		cm[8], cm[9], cm[10], cm[11],
		cm[12], cm[13], cm[14], cm[15]);
}

ofRectangle toOf(const ovrRecti& vp) {
	return ofRectangle(vp.Pos.x, vp.Pos.y, vp.Size.w, vp.Size.h);
}

ofVec3f toOf(const ovrVector3f& v) {
	return ofVec3f(v.x, v.y, v.z);
}

ovrVector3f toOVR(const ofVec3f& v) {
	ovrVector3f ov;
	ov.x = v.x;
	ov.y = v.y;
	ov.z = v.z;
	return ov;
}

using namespace OVR;

ofxOculusRiftCV1::ofxOculusRiftCV1() {

	bOVRInitialized = false;

	eyeRenderTexture[0] = nullptr;
	eyeRenderTexture[1] = nullptr;
	eyeDepthBuffer[0] = nullptr;
	eyeDepthBuffer[1] = nullptr;
	mirrorTexture = nullptr;
	mirrorFBO = 0;
	frameIndex = 0;
}

ofxOculusRiftCV1::~ofxOculusRiftCV1() {

	close();
}

bool ofxOculusRiftCV1::init() {

	ofLogError("ofxOculusRiftCV1") << "init()";

	// Initializes LibOVR, and the Rift
	ovrInitParams initParams = { ovrInit_RequestVersion, OVR_MINOR_VERSION, NULL, 0, 0 };
	ovrResult result = ovr_Initialize(&initParams);

	if (!OVR_SUCCESS(result)) {

		ofLogError("ofxOculusRiftCV1") << "Failed to initialize libOVR";
		logError();

		return false;
	}

	result = ovr_Create(&session, &luid);

	if (!OVR_SUCCESS(result)){

		ofLogWarning("ofxOculusRiftCV1") << "Failed to create OVR session";
		logError();
		
		return false;
	}

	if (Compare(luid, GetDefaultAdapterLuid())) // If luid that the Rift is on is not the default adapter LUID...
	{
		VALIDATE(false, "OpenGL supports only the default graphics adapter.");
	}

	hmdDesc = ovr_GetHmdDesc(session);

	// Setup Window and Graphics
	// Note: the mirror window can be any size, for this sample we use 1/2 the HMD resolution
	windowSize = { hmdDesc.Resolution.w /2 , hmdDesc.Resolution.h /2 };

	/*
	if (!Platform.InitDevice(windowSize.w, windowSize.h, reinterpret_cast<LUID*>(&luid)))
		close();
	*/

	// Make eye render buffers
	for (int eye = 0; eye < 2; ++eye)
	{
		ovrSizei idealTextureSize = ovr_GetFovTextureSize(session, ovrEyeType(eye), hmdDesc.DefaultEyeFov[eye], 1);
		eyeRenderTexture[eye] = new TextureBuffer(session, true, true, idealTextureSize, 1, NULL, 1);
		eyeDepthBuffer[eye] = new DepthBuffer(eyeRenderTexture[eye]->GetSize(), 0);

		if (!eyeRenderTexture[eye]->TextureChain)
		{
			close();
			VALIDATE(false, "Failed to create texture.");
		}
	}

	eyeRenderViewport[0].Pos = Vector2i(0, 0);
	eyeRenderViewport[0].Size = Sizei(hmdDesc.Resolution.w / 2, windowSize.h);
	eyeRenderViewport[1].Pos = Vector2i((hmdDesc.Resolution.w + 1) / 2, 0);
	eyeRenderViewport[1].Size = eyeRenderViewport[0].Size;

	ovrMirrorTextureDesc desc;
	memset(&desc, 0, sizeof(desc));
	desc.Width = windowSize.w;
	desc.Height = windowSize.h;
	desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

	// Create mirror texture and an FBO used to copy mirror texture to back buffer
	result = ovr_CreateMirrorTextureGL(session, &desc, &mirrorTexture);
	if (!OVR_SUCCESS(result))
	{
		close();
		VALIDATE(false, "Failed to create mirror texture.");
	}

	// Configure the mirror read buffer
	ovr_GetMirrorTextureBufferGL(session, mirrorTexture, &mirrorTextureID);

	glGenFramebuffers(1, &mirrorFBO);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTextureID, 0);
	glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	// Turn off vsync to let the compositor do its magic
	wglSwapIntervalEXT(0);

	// FloorLevel will give tracking poses where the floor height is 0
	ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel);

	const string version = "#version 150\n";
	const string vertexShader = version+STRINGIFY(

		in vec4 position;
		in vec2 texcoord;

		uniform mat4 modelViewMatrix;
		uniform mat4 projectionMatrix;
		uniform mat4 textureMatrix;
		uniform mat4 modelViewProjectionMatrix;
		uniform vec4 globalColor;

		out vec2 texCoordVarying;
		out vec4 colorVarying;

		void main() {

			//get our current vertex position so we can modify it
			vec4 pos = modelViewProjectionMatrix * position;
			colorVarying = globalColor;
			texCoordVarying = texcoord;
			gl_Position = pos;
		}
	);

	const string fragmentShader = version+STRINGIFY(

		uniform sampler2D src_tex_unit0;

		in vec2 texCoordVarying;
		in vec4 colorVarying;

		out vec4 fragColor;

		void main() {

			vec3 src = texture(src_tex_unit0, texCoordVarying).rgb;
			//fragColor = colorVarying * vec4( src, 1.0) ;
			fragColor = vec4(src, 1.0);
		}
	);

	mirrorShader.setupShaderFromSource(GL_VERTEX_SHADER, vertexShader);
	mirrorShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragmentShader);
	mirrorShader.bindDefaults();
	mirrorShader.linkProgram();

	bOVRInitialized = true;

	return bOVRInitialized;
}

void ofxOculusRiftCV1::close() {

	ofLogNotice("ofxOculusRiftCV1") << "close()";

	if( bOVRInitialized ){

		if (mirrorFBO) glDeleteFramebuffers(1, &mirrorFBO);
		if (mirrorTexture) ovr_DestroyMirrorTexture(session, mirrorTexture);

		for (int eye = 0; eye < 2; ++eye)
		{
			delete eyeRenderTexture[eye];
			delete eyeDepthBuffer[eye];
		}
		
		ovr_Destroy(session);
		ovr_Shutdown();

		bOVRInitialized = false;
	}
}

void ofxOculusRiftCV1::update() {

	if (!bOVRInitialized) return;

	// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyeOffset) may change at runtime.
	eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

	// Get eye poses, feeding in correct IPD offset
	ovrVector3f HmdToEyeOffset[2] = { eyeRenderDesc[0].HmdToEyeOffset, eyeRenderDesc[1].HmdToEyeOffset };

	// sensorSampleTime is fed into the layer later
	ovr_GetEyePoses(session, frameIndex, ovrTrue, HmdToEyeOffset, eyeRenderPose, &sensorSampleTime);
}

void ofxOculusRiftCV1::begin(ovrEyeType whichEye) {

	if (!bOVRInitialized) return;

	ovrSessionStatus sessionStatus;
	ovr_GetSessionStatus(session, &sessionStatus);

	if (sessionStatus.ShouldQuit) {
		// Because the application is requested to quit, should not request retry
		return;
	}

	if (sessionStatus.ShouldRecenter)
		ovr_RecenterTrackingOrigin(session);

	if (true) //sessionStatus.IsVisible)
	{
		int eye = (whichEye == ovrEye_Left) ? 0 : 1;

		ofPushView();

		// Switch to eye render target
		eyeRenderTexture[eye]->SetAndClearRenderSurface(eyeDepthBuffer[eye]);

		Matrix4f proj = ovrMatrix4f_Projection(hmdDesc.DefaultEyeFov[eye], 0.2f, 1000.0f, ovrProjection_None);
		ofMatrix4x4 projectionMatrix = toOf(proj);

		Matrix4f eyeMatrix = Matrix4f(eyeRenderPose[eye].Orientation);
		Vector3f finalUp = eyeMatrix.Transform(Vector3f(0, 1, 0));
		Vector3f finalForward = eyeMatrix.Transform(Vector3f(0, 0, -1));
		Vector3f shiftedEyePos = (eyeRenderPose[eye].Position);

		ofVec3f eyePos = ofVec3f(shiftedEyePos.x, shiftedEyePos.y, shiftedEyePos.z);
		ofVec3f lookAtPos = eyePos + ofVec3f(finalForward.x, finalForward.y, finalForward.z);
		ofVec3f up = ofVec3f(finalUp.x, finalUp.y, finalUp.z);

		ofMatrix4x4 modelViewMatrix;
		modelViewMatrix.makeLookAtViewMatrix(eyePos, lookAtPos, up);

		ofPushMatrix();

		ofSetMatrixMode(OF_MATRIX_PROJECTION);
		ofLoadIdentityMatrix();
		projectionMatrix.scale(1, -1, 1);
		ofLoadMatrix(projectionMatrix);

		ofSetMatrixMode(OF_MATRIX_MODELVIEW);
		ofLoadIdentityMatrix();
		ofLoadMatrix(modelViewMatrix);
	}	
}

void ofxOculusRiftCV1::end(ovrEyeType whichEye) {

	if (!bOVRInitialized) return;

	int eye = (whichEye == ovrEye_Left) ? 0 : 1;

	// Avoids an error when calling SetAndClearRenderSurface during next iteration.
	// Without this, during the next while loop iteration SetAndClearRenderSurface
	// would bind a framebuffer with an invalid COLOR_ATTACHMENT0 because the texture ID
	// associated with COLOR_ATTACHMENT0 had been unlocked by calling wglDXUnlockObjectsNV.
	eyeRenderTexture[eye]->UnsetRenderSurface();

	// Commit changes to the textures so they get picked up frame
	eyeRenderTexture[eye]->Commit();

	ofPopMatrix();
	ofPopView();

	if (whichEye == ovrEye_Right) 
	{

		// Do distortion rendering, Present and flush/sync

		ovrLayerEyeFov ld;
		ld.Header.Type = ovrLayerType_EyeFov;
		ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;   // Because OpenGL.

		for (int eye = 0; eye < 2; ++eye)
		{
			ld.ColorTexture[eye] = eyeRenderTexture[eye]->TextureChain;
			ld.Viewport[eye] = Recti(eyeRenderTexture[eye]->GetSize());
			ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
			ld.RenderPose[eye] = eyeRenderPose[eye];
			ld.SensorSampleTime = sensorSampleTime;
		}

		ovrLayerHeader* layers = &ld.Header;
		ovrResult result = ovr_SubmitFrame(session, frameIndex, nullptr, &layers, 1);

		// exit the rendering loop if submit returns an error, will retry on ovrError_DisplayLost
		if (!OVR_SUCCESS(result)) {
			logError();
		}

		frameIndex++;
	}
}

void ofxOculusRiftCV1::draw(float x, float y) {

	draw(x, y, windowSize.w, windowSize.h);
}

void ofxOculusRiftCV1::draw(float x, float y, float w, float h) {

	if (!bOVRInitialized) return;

	
	ovrSessionStatus sessionStatus;
	ovr_GetSessionStatus(session, &sessionStatus);
	if (sessionStatus.ShouldQuit){
		// Because the application is requested to quit, should not request retry
		return;
	}
	if (sessionStatus.ShouldRecenter)
		ovr_RecenterTrackingOrigin(session);


#ifndef BLIT_TEXTURE

		GLint offsetY = ofGetHeight() - windowSize.h;

		// Blit mirror texture to back buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		glBlitFramebuffer(0, windowSize.h, windowSize.w, 0,
			0, offsetY, windowSize.w, windowSize.h+ offsetY,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

#else

		glClear(GL_DEPTH_BUFFER_BIT);
		
		ofMesh m;
		m.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
		m.addVertex(ofVec3f(x, y));
		m.addVertex(ofVec3f(x+w, y));
		m.addVertex(ofVec3f(x, y+h));
		m.addVertex(ofVec3f(x+w, y+h));
		m.addTexCoord(ofVec3f(0, 0));
		m.addTexCoord(ofVec3f(1.0, 0));
		m.addTexCoord(ofVec3f(0, 1.0));
		m.addTexCoord(ofVec3f(1.0, 1.0));

		mirrorShader.begin();
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mirrorTextureID);
		glUniform1i(glGetUniformLocation(mirrorShader.getProgram(), "src_tex_unit0"), 0);
		glActiveTexture(GL_TEXTURE0);
		
		m.draw();

		mirrorShader.end();
#endif
}

void ofxOculusRiftCV1::drawScene() {

	if (!bOVRInitialized) return;

	ovrSessionStatus sessionStatus;
	ovr_GetSessionStatus(session, &sessionStatus);
	if (sessionStatus.ShouldQuit) {
		// Because the application is requested to quit, should not request retry
		return;
	}
	if (sessionStatus.ShouldRecenter)
		ovr_RecenterTrackingOrigin(session);

	if (sessionStatus.IsVisible)
	{

		// Keyboard inputs to adjust player orientation
		static float Yaw(3.141592f);
		//if (Platform.Key[VK_LEFT])  Yaw += 0.02f;
		//if (Platform.Key[VK_RIGHT]) Yaw -= 0.02f;

		// Keyboard inputs to adjust player position
		static Vector3f Pos2(0.0f, 0.0f, -5.0f);
		//if (Platform.Key['W'] || Platform.Key[VK_UP])     Pos2 += Matrix4f::RotationY(Yaw).Transform(Vector3f(0, 0, -0.05f));
		//if (Platform.Key['S'] || Platform.Key[VK_DOWN])   Pos2 += Matrix4f::RotationY(Yaw).Transform(Vector3f(0, 0, +0.05f));
		//if (Platform.Key['D'])                            Pos2 += Matrix4f::RotationY(Yaw).Transform(Vector3f(+0.05f, 0, 0));
		//if (Platform.Key['A'])                            Pos2 += Matrix4f::RotationY(Yaw).Transform(Vector3f(-0.05f, 0, 0));

		// Animate the cube
		static float cubeClock = 0;
		//roomScene->Models[0]->Pos = Vector3f(9 * (float)sin(cubeClock), 3, 9 * (float)cos(cubeClock += 0.015f));

		// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyeOffset) may change at runtime.
		ovrEyeRenderDesc eyeRenderDesc[2];
		eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
		eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

		// Get eye poses, feeding in correct IPD offset
		ovrPosef                  EyeRenderPose[2];
		ovrVector3f               HmdToEyeOffset[2] = { eyeRenderDesc[0].HmdToEyeOffset, eyeRenderDesc[1].HmdToEyeOffset };

		double sensorSampleTime;    // sensorSampleTime is fed into the layer later
		ovr_GetEyePoses(session, frameIndex, ovrTrue, HmdToEyeOffset, EyeRenderPose, &sensorSampleTime);

		// Render Scene to Eye Buffers
		for (int eye = 0; eye < 2; ++eye)
		{
			// Switch to eye render target
			eyeRenderTexture[eye]->SetAndClearRenderSurface(eyeDepthBuffer[eye]);

			// Get view and projection matrices
			/*
			Matrix4f rollPitchYaw = Matrix4f::RotationY(Yaw);
			Matrix4f finalRollPitchYaw = rollPitchYaw * Matrix4f(EyeRenderPose[eye].Orientation);
			Vector3f finalUp = finalRollPitchYaw.Transform(Vector3f(0, 1, 0));
			Vector3f finalForward = finalRollPitchYaw.Transform(Vector3f(0, 0, -1));
			Vector3f shiftedEyePos = Pos2 + rollPitchYaw.Transform(EyeRenderPose[eye].Position);*/
			
			Vector3f eyePos = (EyeRenderPose[eye].Position);
			Vector3f eyeForward = Vector3f(0, 0, -1);
			Vector3f eyeUp = Vector3f(0, 1, 0);

			Matrix4f view = Matrix4f::LookAtRH(eyePos, eyePos + eyePos, eyeUp);
			Matrix4f proj = ovrMatrix4f_Projection(hmdDesc.DefaultEyeFov[eye], 0.2f, 1000.0f, ovrProjection_None);

			// Render world
			//roomScene->Render(view, proj);

			// Avoids an error when calling SetAndClearRenderSurface during next iteration.
			// Without this, during the next while loop iteration SetAndClearRenderSurface
			// would bind a framebuffer with an invalid COLOR_ATTACHMENT0 because the texture ID
			// associated with COLOR_ATTACHMENT0 had been unlocked by calling wglDXUnlockObjectsNV.
			eyeRenderTexture[eye]->UnsetRenderSurface();

			// Commit changes to the textures so they get picked up frame
			eyeRenderTexture[eye]->Commit();
		}

		// Do distortion rendering, Present and flush/sync

		ovrLayerEyeFov ld;
		ld.Header.Type = ovrLayerType_EyeFov;
		ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;   // Because OpenGL.

		for (int eye = 0; eye < 2; ++eye)
		{
			ld.ColorTexture[eye] = eyeRenderTexture[eye]->TextureChain;
			ld.Viewport[eye] = Recti(eyeRenderTexture[eye]->GetSize());
			ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
			ld.RenderPose[eye] = EyeRenderPose[eye];
			ld.SensorSampleTime = sensorSampleTime;
		}

		ovrLayerHeader* layers = &ld.Header;
		ovrResult result = ovr_SubmitFrame(session, frameIndex, nullptr, &layers, 1);

		// exit the rendering loop if submit returns an error, will retry on ovrError_DisplayLost
		if (!OVR_SUCCESS(result)) {
			logError();
		}

		frameIndex++;
	}

	/*
	// Blit mirror texture to back buffer
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	GLint w = windowSize.w;
	GLint h = windowSize.h;

	
	glBlitFramebuffer(0, h, w, 0,
		0, 0, w, h,
		GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	*/

	GLint w = windowSize.w;
	GLint h = windowSize.h;

	

	//SwapBuffers(Platform.hDC); // ?
}

bool ofxOculusRiftCV1::getIsInitialized() {

	return bOVRInitialized;
}

ofRectangle ofxOculusRiftCV1::getHMDSize() {

	ofRectangle bounds;
	bounds.width = windowSize.w;
	bounds.height = windowSize.h;
	return bounds;
}

ovrHmdDesc & ofxOculusRiftCV1::getHMD() {

	return hmdDesc;
}

void ofxOculusRiftCV1::getHMDTrackingState(ofVec3f & position, ofQuaternion & orientation) {

	ovrTrackingState state = ovr_GetTrackingState(session, sensorSampleTime, ovrFalse);
	
	position = toOf(state.HeadPose.ThePose.Position);
	orientation = toOf(state.HeadPose.ThePose.Orientation);
}

ovrTrackingState ofxOculusRiftCV1::getHMDTrackingState() {

	ovrTrackingState state = ovr_GetTrackingState(session, sensorSampleTime, ovrFalse );
	return state;
}

ofMatrix4x4 ofxOculusRiftCV1::getHMDOrientationMatrix() {

	// @NOTE: todo
	//return toOf(Matrix4f(pFusionResult->GetPredictedOrientation()));

	ovrTrackingState ts = ovr_GetTrackingState(session, sensorSampleTime, ovrFalse);
	if (ts.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked)) {
		return toOf(Matrix4f(ts.HeadPose.ThePose.Orientation));
	}
	return ofMatrix4x4();
}

ovrGraphicsLuid ofxOculusRiftCV1::GetDefaultAdapterLuid(){

	ovrGraphicsLuid luid = ovrGraphicsLuid();

#if defined(_WIN32)
	IDXGIFactory* factory = nullptr;

	if (SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&factory))))
	{
		IDXGIAdapter* adapter = nullptr;

		if (SUCCEEDED(factory->EnumAdapters(0, &adapter)))
		{
			DXGI_ADAPTER_DESC desc;

			adapter->GetDesc(&desc);
			memcpy(&luid, &desc.AdapterLuid, sizeof(luid));
			adapter->Release();
		}

		factory->Release();
	}
#endif

	return luid;
}

int ofxOculusRiftCV1::Compare(const ovrGraphicsLuid& lhs, const ovrGraphicsLuid& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(ovrGraphicsLuid));
}

void ofxOculusRiftCV1::logError() {

	ovrErrorInfo error;
	ovr_GetLastErrorInfo(&error);
	ofLogError("ofxOculusRiftCV1") << "Error: " << error.Result << " " << error.ErrorString;
}