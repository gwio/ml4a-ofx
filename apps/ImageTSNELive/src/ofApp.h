#pragma once

#include "ofMain.h"
#include "ofxCcv.h"
#include "ofxTSNE.h"
#include "ofxAssignment.h"
#include "ofxGui.h"
#include "ofxJSON.h"

#define THUMB_SIZE 256
#define RELEASE


struct ImageThumb {
public:
    ofImage image;
    ofPoint point;
    ofPoint gridPoint;
    string path;
    int idx;
};

class ofApp : public ofBaseApp{
public:
    void setup();
    void update();    
    void draw();
    void drawThumbs();
    
    void updateAnalysis();
    void runTsne();
    void solveToGrid();
    void analyzeDirectory(string imagesPath);
    void loadJSON(string jsonPath);
    void saveJSON(string jsonPath);
    void saveScreenshot(string imgPath);
    
    void eLoadJSON();
    void eSaveJSON();
    void eSaveScreenshot();
    void eAnalyzeDirectory();
    void scanDirectoryRecursive(ofDirectory dir);
    
    void mouseDragged(int x, int y, int button);
    void mouseScrolled(ofMouseEventArgs &evt);
    
    vector<ImageThumb> thumbs;
    vector<ofFile> imageFiles;

    ofxCcv ccv;
    ofxTSNE tsne;
    ofxAssignment solver;
    vector<vector<float> > encodings;
    vector<vector<double> > tsneVecs;
    vector<ofVec2f> solvedGrid;
    vector<ofVec2f> aniPos;
    float aniPct;
    
    ofxPanel gui;
    ofxButton bAnalyzeNew, bSave, bLoad, bSaveScreenshot;
    ofParameter<bool> tViewGrid;
    void gridBtnEvent(bool & grid_);
    ofParameter<float> scale, imageSize;
    ofParameter<float> spacingX, spacingY;
    ofParameter<float> perplexity, theta;
    ofParameter<int> numImages;
    ofParameter<float> animationSpeed;
    
    int numGridRows, numGridCols;
    ofVec2f position;
    string ccvPath;
    bool isAnalyzing;
    string progressMsg;
    
    float easeOutCubic(float) ;
};


