
#include "ofxARToolkitPlus.h"

#include "ARToolKitPlus/TrackerMultiMarkerImpl.h"

ARToolKitPlus::TrackerMultiMarker *tracker;

class MyLogger : public ARToolKitPlus::Logger
{
    void artLog(const char* nStr)
    {
        cout <<nStr;
    }
};

static MyLogger logger;

ofxARToolkitPlus::ofxARToolkitPlus() {
}

ofxARToolkitPlus::~ofxARToolkitPlus() {
	// This must be static so don't delete it when using multiple instances
	//if( tracker != NULL ) { delete tracker; }
}


//--------------------------------------------------
void ofxARToolkitPlus::setup(int w, int h) {
	// load std. ARToolKit camera file	
	// These need to be in the data folder
	setup(w, h, "LogitechPro4000.dat", "markerboard_480-499.cfg");
}

//--------------------------------------------------
void ofxARToolkitPlus::setup(int w, int h, string camParamFile, string multiFile) {
	
	width = w;
	height = h;	
	useBCH = true;
	markerWidth = 40.0;
	halfMarkerWidth = markerWidth/2;
	
	// ----------------------------------  AR TK+ STUFF - ripped from the single marker demo app
	
    // create a tracker that does:
    //  - 6x6 sized marker images
    //  - samples at a maximum of 6x6
    //  - works with luminance (gray) images
    //  - can load a maximum of 10 patterns
    //  - can detect a maximum of 10 patterns in one image
	tracker = new ARToolKitPlus::TrackerMultiMarkerImpl<6,6,6, ARToolKitPlus::PIXEL_FORMAT_LUM, 10>(width,height);
	
	const char* description = tracker->getDescription();
	printf("ARToolKitPlus compile-time information:\n%s\n\n", description);
	
    // set a logger so we can output error messages
    tracker->setLogger(&logger);
	tracker->setPixelFormat(ARToolKitPlus::PIXEL_FORMAT_LUM);	

	tracker->setImageProcessingMode(ARToolKitPlus::IMAGE_FULL_RES);
	
	// Initialize a multimarker tracker with
	// Camera and marker files
	// & near and far clipping values for the OpenGL projection matrix
    if( !tracker->init( (const char *)ofToDataPath(camParamFile).c_str(),
						(const char *)ofToDataPath(multiFile).c_str(),
						1.0f, 1000.0f) )	{

		printf("ERROR: init() failed\n");
		delete tracker;
	
		return;
	}
	
    // define size of the marker
    //tracker->setPatternWidth(80); // I'm not sure how to define the size with multimarkers since it doesnt seem to have this option.
	
	// the marker in the BCH test image has a thin border...
    tracker->setBorderWidth(useBCH ? 0.125f : 0.250f);
    //tracker->setBorderWidth(0.250f);	
	
    // set a threshold. alternatively we could also activate automatic thresholding
    tracker->setThreshold(85);

	// let's use lookup-table undistortion for high-speed
    // note: LUT only works with images up to 1024x1024
    tracker->setUndistortionMode(ARToolKitPlus::UNDIST_LUT);
	//tracker->setUndistortionMode(ARToolKitPlus::UNDIST_STD);
	
    // RPP is more robust than ARToolKit's standard pose estimator
    tracker->setPoseEstimator(ARToolKitPlus::POSE_ESTIMATOR_RPP);
	
    // switch to simple ID based markers
    // use the tool in tools/IdPatGen to generate markers
    tracker->setMarkerMode(useBCH ? ARToolKitPlus::MARKER_ID_BCH : ARToolKitPlus::MARKER_ID_SIMPLE);
	//tracker->setMarkerMode(ARToolKitPlus::MARKER_ID_SIMPLE);
	
	//tracker->activateVignettingCompensation(true);

	setupHomoSrc();
	
}

//--------------------------------------------------
int ofxARToolkitPlus::update(unsigned char *pixels) {
	return tracker->calc(pixels);
}

//--------------------------------------------------
void ofxARToolkitPlus::draw() {
	draw(0, 0, width, height);
}

void ofxARToolkitPlus::draw(int x, int y) {
	draw(x, y, width, height);
}

void ofxARToolkitPlus::draw(int x, int y, int w, int h) {
	
	float scaleX = (float)w / width;
	float scaleY = (float)h / height;
	
	glPushMatrix();
	glTranslatef(x, y, 0);
	glScalef(scaleX, scaleY, 1);
	ofSetLineWidth(1);
	
	int numDetected = tracker->getNumDetectedMarkers();
	
	for(int i=0; i<numDetected; i++) {
		
		ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(i);
		
		// Draw the center point
		ofFill();
		ofSetColor(255, 0, 255 );
		ofRect(marker.pos[0]-1, marker.pos[1]-1, 2, 2);
		
		// Draw the inner rectangle
		ofNoFill();
		ofSetColor(255, 255, 0 );		
		ofBeginShape();
		ofVertex(marker.vertex[0][0], marker.vertex[0][1]);
		ofVertex(marker.vertex[1][0], marker.vertex[1][1]);
		ofVertex(marker.vertex[2][0], marker.vertex[2][1]);
		ofVertex(marker.vertex[3][0], marker.vertex[3][1]);
		ofEndShape(true);
		
		// Draw the marker ID
		ofSetColor( 255, 255, 255 );	
		ofDrawBitmapString(ofToString(marker.id), marker.pos[0], marker.pos[1]);
		
		// Draw the outer rectangle
		ofPoint center(marker.pos[0], marker.pos[1]);
		ofNoFill();
		ofSetColor(255, 0, 0 );		
		ofBeginShape();
		for (int j=0; j<4; j++) {
			ofPoint corner(marker.vertex[j][0], marker.vertex[j][1]);
			corner -= center;
			corner *= BORDER_SCALE;
			corner += center;
			ofVertex(corner.x, corner.y);
		}
		ofEndShape(true);
		
	}
	
	glPopMatrix();
	
}

//--------------------------------------------------
void ofxARToolkitPlus::applyProjectionMatrix(int viewportWidth, int viewportHeight){
	glViewport(0, 0, viewportWidth, viewportHeight );
	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf(tracker->getProjectionMatrix());
	
}

//--------------------------------------------------
void ofxARToolkitPlus::applyProjectionMatrix(){
	glViewport(0, 0, width, height );
	glMatrixMode( GL_PROJECTION );
	glLoadMatrixf(tracker->getProjectionMatrix());
	
}

void ofxARToolkitPlus::applyModelMatrix(int markerIndex) {

	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	
	float m34[ 3 ][ 4 ];
	float c[ 2 ] = { 0.0f, 0.0f };
	float m[ 16 ]; 
	
	tracker->rppGetTransMat( &marker, c, markerWidth, m34 );
	
	// Convert from ARTK matrix to OpenGL format
	m[0] = m34[0][0];
	m[1] = m34[1][0];	
	m[2] = m34[2][0];		
	m[3] = 0;			

	m[4] = m34[0][1];
	m[5] = m34[1][1];	
	m[6] = m34[2][1];		
	m[7] = 0;	

	m[8] = m34[0][2];
	m[9] = m34[1][2];	
	m[10] = m34[2][2];		
	m[11] = 0;	
	
	m[12] = m34[0][3];
	m[13] = m34[1][3];	
	m[14] = m34[2][3];		
	m[15] = 1;	
		
	glMatrixMode( GL_MODELVIEW );
	glLoadMatrixf( m );
	
}

ofxMatrix4x4 ofxARToolkitPlus::getMatrix(int markerIndex) {
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	
	float m34[ 3 ][ 4 ];
	float c[ 2 ] = { 0.0f, 0.0f };
	
	tracker->rppGetTransMat( &marker, c, markerWidth, m34 );

	ofxMatrix4x4 matrix(m34[0][0], m34[0][1], m34[0][2], m34[0][3],
						m34[1][0], m34[1][1], m34[1][2], m34[1][3],
						m34[2][0], m34[2][1], m34[2][2], m34[2][3],
						0, 0, 0, 1);
	return matrix;
}

ofxMatrix4x4 ofxARToolkitPlus::getGLMatrix(int markerIndex) {
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	
	float m34[ 3 ][ 4 ];
	float c[ 2 ] = { 0.0f, 0.0f };
	
	tracker->rppGetTransMat( &marker, c, markerWidth, m34 );

	// OpenGL Order
	ofxMatrix4x4 matrix(m34[0][0], m34[1][0], m34[2][0], 0,
						m34[0][1], m34[1][1], m34[2][1], 0,
						m34[0][2], m34[1][2], m34[2][2], 0,
						m34[0][3], m34[1][3], m34[2][3], 1);
	return matrix;
}

ofxMatrix4x4 ofxARToolkitPlus::getHomography(int markerIndex) {
	vector<ofPoint> corners;
	getDetectedMarkerOrderedBorderCorners(markerIndex, corners);
	return findHomography(homoSrc, corners);
}

ofxMatrix4x4 ofxARToolkitPlus::getHomography(int markerIndex, vector<ofPoint> &src) {
	vector<ofPoint> corners;
	getDetectedMarkerOrderedBorderCorners(markerIndex, corners);
	return findHomography(src, corners);
}


ofxVec3f ofxARToolkitPlus::getTranslation(int markerIndex) {
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	
	float m34[ 3 ][ 4 ];
	float c[ 2 ] = { 0.0f, 0.0f };
	
	tracker->rppGetTransMat( &marker, c, markerWidth, m34 );
	
	ofxVec3f trans(m34[0][3], m34[1][3], m34[2][3]);
	return trans;
}

ofxMatrix4x4 ofxARToolkitPlus::getOrientationMatrix(int markerIndex) {
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	
	float m34[ 3 ][ 4 ];
	float c[ 2 ] = { 0.0f, 0.0f };
	
	tracker->rppGetTransMat( &marker, c, markerWidth, m34 );
	
	ofxMatrix4x4 matrix(m34[0][0], m34[0][1], m34[0][2], 0,
						m34[1][0], m34[1][1], m34[1][2], 0,
						m34[2][0], m34[2][1], m34[2][2], 0,
						0, 0, 0, 1);
	return matrix;
}

void ofxARToolkitPlus::getTranslationAndOrientation(int markerIndex, ofxVec3f &translation, ofxMatrix4x4 &orientation) {
	
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	
	float m34[ 3 ][ 4 ];
	float c[ 2 ] = { 0.0f, 0.0f };
	
	tracker->rppGetTransMat( &marker, c, markerWidth, m34 );
	
	// Translation
	translation.set(m34[0][3], m34[1][3], m34[2][3]);
	
	// Orientation
	orientation.set(m34[0][0], m34[0][1], m34[0][2], 0,
					m34[1][0], m34[1][1], m34[1][2], 0,
					m34[2][0], m34[2][1], m34[2][2], 0,
					0, 0, 0, 1);
}

ofxVec3f ofxARToolkitPlus::getCameraPosition(int markerIndex)  {

	// Translation
	ofxVec3f trans;
	// Orientation
	ofxMatrix4x4 orient;
	
	getTranslationAndOrientation(markerIndex, trans, orient);
	
	// Transpose of the camera orientation
	ofxMatrix4x4 tOrient = ofxMatrix4x4::getTransposedOf(orient);
	// Negative of the transpose
	ofxMatrix4x4 negtOrient = tOrient;
	negtOrient.scale(-1, -1, -1);
	
	// Camera Location
	// Location of the camera relative to the marker
	ofxVec3f loc = negtOrient * trans;
	
	return loc;
}

//--------------------------------------------------
void ofxARToolkitPlus::setThreshold(int threshold) {
	tracker->activateAutoThreshold(false);
	tracker->setThreshold(threshold);
}

void ofxARToolkitPlus::activateAutoThreshold(bool state) {
	tracker->activateAutoThreshold(state);
}

void ofxARToolkitPlus::setMarkerWidth(float mm) {
	markerWidth = mm;
	halfMarkerWidth = markerWidth/2;
	setupHomoSrc();
}

void ofxARToolkitPlus::setupHomoSrc() {
	
	homoSrc.clear();
	homoSrc.push_back(ofPoint(-halfMarkerWidth, -halfMarkerWidth));
	homoSrc.push_back(ofPoint(halfMarkerWidth, -halfMarkerWidth));		
	homoSrc.push_back(ofPoint(halfMarkerWidth, halfMarkerWidth));		
	homoSrc.push_back(ofPoint(-halfMarkerWidth, halfMarkerWidth));
	
}

//--------------------------------------------------
int ofxARToolkitPlus::getNumDetectedMarkers() {
	return tracker->getNumDetectedMarkers();
}

int ofxARToolkitPlus::getMarkerIndex(int markerID) {
	int numDetected = tracker->getNumDetectedMarkers();
	for(int i=0; i<numDetected; i++) {
		ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(i);
		if(marker.id == markerID) {
			return i;
		}
	}
	return -1;
}

int ofxARToolkitPlus::getMarkerID(int markerIndex) {
	int numDetected = tracker->getNumDetectedMarkers();
	if (markerIndex < 0 || markerIndex >= numDetected) {
		return -1;
	}
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	return marker.id;
}

//--------------------------------------------------
ofPoint ofxARToolkitPlus::getDetectedMarkerCenter(int markerIndex) {
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	return ofPoint(marker.pos[0], marker.pos[1]);
}

int ofxARToolkitPlus::getDetectedMarkerDirection(int markerIndex) {
	return tracker->getDetectedMarker(markerIndex).dir;
}

void ofxARToolkitPlus::getDetectedMarkerCorners(int markerIndex, vector<ofPoint> &corners) {
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	corners.clear();
	corners.push_back(ofPoint(marker.vertex[0][0], marker.vertex[0][1]));
	corners.push_back(ofPoint(marker.vertex[1][0], marker.vertex[1][1]));
	corners.push_back(ofPoint(marker.vertex[2][0], marker.vertex[2][1]));
	corners.push_back(ofPoint(marker.vertex[3][0], marker.vertex[3][1]));
}

void ofxARToolkitPlus::getDetectedMarkerOrderedCorners(int markerIndex, vector<ofPoint> &corners) {
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	corners.clear();
	
	int index = 4 - marker.dir;
	for (int i=0; i<4; i++) {		
		corners.push_back(ofPoint(marker.vertex[index][0], marker.vertex[index][1]));
		index++;
		if(index >= 4) {
			index = 0;
		}
	}	
}

void ofxARToolkitPlus::getDetectedMarkerBorderCorners(int markerIndex, vector<ofPoint> &corners) {
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	corners.clear();
	corners.push_back(ofPoint(marker.vertex[0][0], marker.vertex[0][1]));
	corners.push_back(ofPoint(marker.vertex[1][0], marker.vertex[1][1]));
	corners.push_back(ofPoint(marker.vertex[2][0], marker.vertex[2][1]));
	corners.push_back(ofPoint(marker.vertex[3][0], marker.vertex[3][1]));
	
	ofPoint center(marker.pos[0], marker.pos[1]);
	for (int j=0; j<4; j++) {
		corners[j] -= center;
		corners[j] *= BORDER_SCALE;
		corners[j] += center;
	}
}

void ofxARToolkitPlus::getDetectedMarkerOrderedBorderCorners(int markerIndex, vector<ofPoint> &corners) {
	ARToolKitPlus::ARMarkerInfo marker = tracker->getDetectedMarker(markerIndex);
	corners.clear();
	
	int index = 4 - marker.dir;
	for (int i=0; i<4; i++) {		
		if(index >= 4) {
			index = 0;
		}		
		corners.push_back(ofPoint(marker.vertex[index][0], marker.vertex[index][1]));
		index++;
	}
		
	ofPoint center(marker.pos[0], marker.pos[1]);
	for (int j=0; j<4; j++) {
		corners[j] -= center;
		corners[j] *= BORDER_SCALE;
		corners[j] += center;
	}
}


