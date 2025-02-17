#include "ofApp.h"

const string allowed_ext[] = {"jpg", "png", "gif", "jpeg"};

void ofApp::scanDirectoryRecursive(ofDirectory dir) {
    ofDirectory new_dir;
    int size = dir.listDir();
    for (int i = 0; i < size; i++){
        if (dir.getFile(i).isDirectory()){
            new_dir = ofDirectory(dir.getFile(i).getAbsolutePath());
            new_dir.listDir();
            new_dir.sort();
            scanDirectoryRecursive(new_dir);
        }
        else if (find(begin(allowed_ext),
                      end(allowed_ext),
                      ofToLower(dir.getFile(i).getExtension())) != end(allowed_ext)) {
            imageFiles.push_back(dir.getFile(i));
        }
    }
}

//--------------------------------------------------------------
void ofApp::setup(){
    
#ifdef RELEASE
    ccvPath = "/Users/chris/of/ml4a-ofx/data/image-net-2012.sqlite3";
#else
    ccvPath = ofToDataPath("../../../../data/image-net-2012.sqlite3");
#endif

    // listen for scroll events, and save screenshot button press
    ofAddListener(ofEvents().mouseScrolled, this, &ofApp::mouseScrolled);
    bSaveScreenshot.addListener(this, &ofApp::eSaveScreenshot);
    bAnalyzeNew.addListener(this, &ofApp::eAnalyzeDirectory);
    bLoad.addListener(this, &ofApp::eLoadJSON);
    bSave.addListener(this, &ofApp::eSaveJSON);

    // setup gui
    ofParameterGroup gView, gAnalyze;
    gView.setName("view");
    gView.add(tViewGrid.set("view as grid", false));
    tViewGrid.addListener(this, &ofApp::gridBtnEvent);
    gView.add(scale.set("scale", 4.0, 1.0, 10.0));
    gView.add(animationSpeed.set("AniSpeed", 0.01, 0.001, 0.01));
    gView.add(imageSize.set("image size", 1.0, 0.0, 2.0));
    gView.add(spacingX.set("spacing/margin X", 0.85, 0.5, 1.0));
    gView.add(spacingY.set("spacing/margin Y", 1.0, 1.0, 2.0));
    gView.add(showLines.set("Show Lines", true));
    gAnalyze.setName("analyze");
    gAnalyze.add(numImages.set("max num images", 500, 1, 8000));
    gAnalyze.add(perplexity.set("perplexity", 50, 5, 80));
    gAnalyze.add(theta.set("theta", 0.3, 0.1, 0.7));
    gui.setup();
    gui.setName("Image t-SNE");
    gui.add(gView);
    gui.add(gAnalyze);
    gui.add(bAnalyzeNew.setup("analyze new directory"));
    gui.add(bLoad.setup("load result from json"));
    gui.add(bSave.setup("save result to json"));
    gui.add(bSaveScreenshot.setup("save screenshot"));
    
    position.set(0, 0);
    isAnalyzing = false;
    
    aniPct = 0.0;
    nLines.setMode(OF_PRIMITIVE_LINE_STRIP);
    nLines.setUsage(GL_DYNAMIC_DRAW);
    
    
    ofDisableDepthTest();
}
//--------------------------------------------------------------

void ofApp::gridBtnEvent(bool & grid_){
    aniPct = 0.0;
    //cout << aniPct << endl;
}

//--------------------------------------------------------------
void ofApp::analyzeDirectory(string imagesPath){
    if (!ccv.isLoaded()){
        ccv.setup(ccvPath);
        if (!ccv.isLoaded()) {
            ofSystemAlertDialog("Can't find model file "+ccvPath+"!");
            return;
        }
    }
    
    // get list of all the images in the directory
    ofLog() << "Gathering images...";
    ofDirectory dir = ofDirectory(imagesPath);
    imageFiles.clear();
    scanDirectoryRecursive(dir);
    if (imageFiles.size() < numImages) {
        numImages = imageFiles.size();
        ofLog(OF_LOG_NOTICE, "There are less images in the directory than the number of images requested. Adjusting to "+ofToString(numImages));
    }

    // start analyzing
    thumbs.clear();
    encodings.clear();
    tsneVecs.clear();
    solvedGrid.clear();
    aniPos.clear();
    isAnalyzing = true;
    ofLog() << "Encoding images...";
    
    aniPos.resize(imageFiles.size());
    nLines.getVertices().resize(imageFiles.size()-1);
    for (int i = 0; i < nLines.getVertices().size(); i++){
        nLines.addColor(ofColor(255,0,0,50));
    }
}

//--------------------------------------------------------------
void ofApp::eAnalyzeDirectory(){
    ofFileDialogResult result = ofSystemLoadDialog("Analyze a directory of image", true);
    if (result.bSuccess) {
        analyzeDirectory(result.getPath());
    }
}

//--------------------------------------------------------------
void ofApp::updateAnalysis(){
    if (thumbs.size() < numImages) {
        int currIdx = thumbs.size();
        ImageThumb thumb;
        thumb.idx = currIdx;
        thumb.path = imageFiles[currIdx].getAbsolutePath();
        thumb.image.load(thumb.path);
        // resize thumb
        //deactivated cropping
        /*
        if (thumb.image.getWidth() > thumb.image.getHeight()) {
            thumb.image.crop((thumb.image.getWidth()-thumb.image.getHeight()) * 0.5, 0, thumb.image.getHeight(), thumb.image.getHeight());
        } else if (thumb.image.getHeight() > thumb.image.getWidth()) {
            thumb.image.crop(0, (thumb.image.getHeight()-thumb.image.getWidth()) * 0.5, thumb.image.getWidth(), thumb.image.getWidth());
        }
         */
        float ratio = THUMB_SIZE/thumb.image.getWidth();
        //running the directory analysis seems to crash when it gets greyscale images, so make sure here to convert to rbg to be safe, running existing json seems ok
        thumb.image.setImageType(OF_IMAGE_COLOR);
        thumb.image.resize(thumb.image.getWidth()*ratio, thumb.image.getHeight()*ratio);
        thumbs.push_back(thumb);
        progressMsg = "loaded "+ofToString(thumbs.size())+"/"+ofToString(numImages)+" images.";
    }
    else if (encodings.size() < thumbs.size()){
        int currIdx = encodings.size();
        vector<float> encoding = ccv.encode(thumbs[currIdx].image, ccv.numLayers()-1);
        encodings.push_back(encoding);
        progressMsg = "loaded "+ofToString(thumbs.size())+" images.";
        progressMsg += "\nencoded "+ofToString(encodings.size())+"/"+ofToString(thumbs.size())+" images.";
        if (encodings.size() == thumbs.size()) {
            progressMsg += "\nrunning t-SNE...";
        }
    }
    else if (tsneVecs.size() == 0) {
        runTsne();
        progressMsg = "loaded "+ofToString(thumbs.size())+" images.";
        progressMsg += "\nencoded "+ofToString(encodings.size())+" images.";
        progressMsg += "\nfinished running t-SNE!";
        progressMsg += "\nsolving grid...";
    }
    else if (solvedGrid.size() == 0) {
        solveToGrid();
        isAnalyzing = false;
        ofLog() << "Finished analyzing directory!";
    }
}

//--------------------------------------------------------------
void ofApp::update(){
    if (isAnalyzing) {
        updateAnalysis();
    }
    
    for (int i = 0; i < thumbs.size(); i++){
     nLines.setVertex(i, glm::vec3(aniPos[i].x , aniPos[i].y ,0));
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
   // ofDisableDepthTest();

    ofBackgroundGradient(ofColor(0), ofColor(100));
    

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    ofPushMatrix();
   
    if(showLines) nLines.draw();
    
     glDisable(GL_DEPTH_TEST);
    if (isAnalyzing) {
        ofDrawBitmapString(progressMsg, 250, 20);
    } else {
        ofTranslate(position.x * (scale - 1.0), position.y * (scale - 1.0));
    
        drawThumbs();
    }
    
    
  
    ofPopMatrix();
    
    
   gui.draw();
    
}

//--------------------------------------------------------------
void ofApp::drawThumbs(){
    if (thumbs.size() == 0) {
        return;
    }
    
    float maxDim = max(scale * ofGetWidth(), scale * ofGetHeight());

    imageSize = (scale * ofGetWidth()) / (THUMB_SIZE * numGridCols);

    float xOffset = (imageSize*THUMB_SIZE - (imageSize*THUMB_SIZE*spacingX))/2;
   // float yOffset = imageSize * THUMB_SIZE *spacingY * 0.5;
    
    if (tViewGrid) {
        for (int i=0; i<solvedGrid.size(); i++) {
            
            float xGrid = ofMap(solvedGrid[i].x, 0, 1, 0, imageSize * THUMB_SIZE * (numGridCols-1));
            float yGrid = ofMap(solvedGrid[i].y, 0, 1, 0, imageSize * THUMB_SIZE * (numGridRows-1))*spacingY;
            
            float x = ofMap(thumbs[i].point.x, 0, 1, 0, maxDim);
            float y = ofMap(thumbs[i].point.y, 0, 1, 0, maxDim);
            
            ofPolyline tempL;
             tempL.addVertex(ofPoint(x,y));
            tempL.addVertex(ofPoint(xGrid,yGrid));
           
         
            if(aniPct < 1.0){
                aniPos[i] = tempL.getPointAtPercent(easeOutCubic(aniPct));
            } else if (aniPct >= 1.0){
                aniPos[i] = tempL.getPointAtPercent(1.0);
            }
            
            float yOffset = (imageSize * THUMB_SIZE *spacingY * 0.5) - ((imageSize * thumbs[i].image.getHeight()*spacingX)/2);

            aniPos[i].x += xOffset;
            aniPos[i].y += yOffset;
            
            thumbs[i].image.draw(aniPos[i].x, aniPos[i].y, imageSize * thumbs[i].image.getWidth()*spacingX, imageSize * thumbs[i].image.getHeight()*spacingX);
           // nLines.setVertex(i, glm::vec3(aniPos[i].x + xOffset, aniPos[i].y + + yOffset,0));
        }
    }
    else {
        float maxDim = max(scale * ofGetWidth(), scale * ofGetHeight());
        for (int i=0; i<thumbs.size(); i++) {
          
            float xGrid = ofMap(solvedGrid[i].x, 0, 1, 0, imageSize * THUMB_SIZE * (numGridCols-1));
            float yGrid = ofMap(solvedGrid[i].y, 0, 1, 0, imageSize * THUMB_SIZE * (numGridRows-1))*spacingY;
            
            float x = ofMap(thumbs[i].point.x, 0, 1, 0, maxDim);
            float y = ofMap(thumbs[i].point.y, 0, 1, 0, maxDim);
            
            ofPolyline tempL;
            tempL.addVertex(ofPoint(xGrid,yGrid));
            tempL.addVertex(ofPoint(x,y));
            
       
            
            if(aniPct < 1.0){
                aniPos[i] = tempL.getPointAtPercent(easeOutCubic(aniPct));
            } else if(aniPct >= 1.0){
                aniPos[i] = tempL.getPointAtPercent(1.0);
            }
            
            float yOffset = (imageSize * THUMB_SIZE *spacingY * 0.5) - ((imageSize * thumbs[i].image.getHeight()*spacingX)/2);
            
            aniPos[i].x += xOffset;
            aniPos[i].y += yOffset;
            
            thumbs[i].image.draw(aniPos[i].x, aniPos[i].y, imageSize * thumbs[i].image.getWidth()*spacingX, imageSize * thumbs[i].image.getHeight()*spacingX);
        }
    }
   if(aniPct < 1.0) aniPct += animationSpeed;
}

//--------------------------------------------------------------
void ofApp::runTsne() {
    ofLog() << "Run t-SNE on images";
    tsneVecs = tsne.run(encodings, 2, perplexity, theta, true);
    
    // normalize t-SNE vectors
    ofPoint tMin(1e8, 1e8), tMax(-1e8, -1e8);
    for (auto & t : tsneVecs) {
        tMin.set(min(tMin.x, (float) t[0]), min(tMin.y, (float) t[1]));
        tMax.set(max(tMax.x, (float) t[0]), max(tMax.y, (float) t[1]));
    }
    
    // save them to thumbs
    for (int t=0; t<tsneVecs.size(); t++) {
        thumbs[t].point.set((tsneVecs[t][0] - tMin.x) / (tMax.x - tMin.x), (tsneVecs[t][1] - tMin.y) / (tMax.y - tMin.y));
    }
}

//--------------------------------------------------------------
void ofApp::solveToGrid() {
    // this is a naive, brute-force way to figure out a grid configuration
    // (num rows x num columns) which fits as many of the original images as
    // possible. try all combinations where rows and cols are between
    // 3/ * sqrt(numImages) and 4/3 * sqrt(numImages)
    int numImages = thumbs.size();
    int minRows = floor((0.75) * sqrt(thumbs.size()));
    int maxRows = ceil(1.3333 * sqrt(thumbs.size()));
    int sizeGrid = 0;
    for (int i=minRows; i<maxRows; i++) {
        for (int j=i; j<=maxRows; j++) {
            int n = i * j;
            if (n > sizeGrid && n <= numImages) {
                numGridRows = i;
                numGridCols = j;
                sizeGrid = n;
            }
        }
    }
    
    // shuffle the thumbnails so you can take a random numImages-sized subset
    random_shuffle(thumbs.begin(), thumbs.end());
    
    // assign to grid
    ofLog() << "Solve t-SNE to "<<numGridCols<<"x"<<numGridRows<<" grid, "<<sizeGrid<<" of "<<numImages<<" fit";
    vector<ofVec2f> tsnePoints; // convert vector<double> to ofVec2f
    for (int t=0; t<sizeGrid; t++) {tsnePoints.push_back(thumbs[t].point);}
    vector<ofVec2f> gridPoints = makeGrid(numGridCols, numGridRows);
    solvedGrid = solver.match(tsnePoints, gridPoints, false);
    
    
}

//--------------------------------------------------------------
void ofApp::loadJSON(string jsonPath) {
    ofJson js;
    ofFile file(jsonPath);
    bool parsingSuccessful = file.exists();
    
    if (!parsingSuccessful) {
        ofLog(OF_LOG_ERROR) << "parsing not successful";
        return;
    }
    
    thumbs.clear();

    int idx = 0;
    file >> js;
    for (int i = 0; i < js.size(); i++) {
        auto entry = js.at(i);
        if(!entry.empty()) {
            string path = entry["path"];
            float x = entry["point"][0];
            float y = entry["point"][1];
            
            ImageThumb thumb;
            thumb.point.set(x, y);
            thumb.idx = idx;
            thumb.path = path;
            thumb.image.load(path);
            // resize thumb
            /*
            if (thumb.image.getWidth() > thumb.image.getHeight()) {
                thumb.image.crop((thumb.image.getWidth()-thumb.image.getHeight()) * 0.5, 0, thumb.image.getHeight(), thumb.image.getHeight());
            } else if (thumb.image.getHeight() > thumb.image.getWidth()) {
                thumb.image.crop(0, (thumb.image.getHeight()-thumb.image.getWidth()) * 0.5, thumb.image.getWidth(), thumb.image.getWidth());
            }
             */
            float ratio = THUMB_SIZE/thumb.image.getWidth();
            thumb.image.resize(thumb.image.getWidth()*ratio, thumb.image.getHeight()*ratio);
            thumbs.push_back(thumb);
            idx++;
        }
    }
    aniPos.resize(thumbs.size());
    nLines.getVertices().resize(thumbs.size()-1);
    for (int i = 0; i < nLines.getVertices().size(); i++){
        nLines.addColor(ofColor(255,0,0,50));
    }
    //resizeThumbs(THUMB_SIZE, THUMB_SIZE);
}

//--------------------------------------------------------------
void ofApp::saveJSON(string jsonPath) {
    ofJson json;
    for (int i=0; i<thumbs.size(); i++) {
        string path = thumbs[i].path;
        float x = thumbs[i].point.x;
        float y = thumbs[i].point.y;
        ofJson entry;
        entry["path"] = path;
        entry["point"][0] = x;
        entry["point"][1] = y;
        json.push_back(entry);
    }
    bool saveSuccessful = ofSaveJson(jsonPath, json);
}

//--------------------------------------------------------------
void ofApp::eLoadJSON() {
    ofFileDialogResult result = ofSystemLoadDialog("Load t-SNE from json file");
    if (result.bSuccess) {
        loadJSON(result.getPath());
        solveToGrid();
        ofLog() << "Finished loading " << result.getPath() << "!";
    }
}

//--------------------------------------------------------------
void ofApp::eSaveJSON() {
    ofFileDialogResult result = ofSystemSaveDialog("data.json", "Export t-SNE to json file");
    if (result.bSuccess) {
        saveJSON(result.getPath());
    }
}

//--------------------------------------------------------------
void ofApp::eSaveScreenshot(){
    ofFileDialogResult result = ofSystemSaveDialog("out.png", "Save current screenshot of t-SNE to PNG file");
    if (result.bSuccess) {
        saveScreenshot(result.getPath());
    }
}

//--------------------------------------------------------------
void ofApp::saveScreenshot(string imgPath){
    if (thumbs.size() == 0) {
        ofLog() << "there are no images to save...";
        return;
    }
    
    ofFbo fbo;
    if (tViewGrid) {
        fbo.allocate(imageSize * thumbs[0].image.getWidth() * numGridCols, imageSize * thumbs[0].image.getHeight() * numGridRows);
    } else {
        float maxDim = max(scale * ofGetWidth(), scale * ofGetHeight());
        fbo.allocate(maxDim + imageSize * thumbs[0].image.getWidth(), maxDim + imageSize * thumbs[0].image.getHeight());
    }
    
    fbo.begin();
    ofClear(0, 0);
    ofBackground(0);
    drawThumbs();
    fbo.end();
    
    ofPixels pix;
    ofImage img;
    fbo.readToPixels(pix);
    img.setFromPixels(pix);
    img.save(imgPath);
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
    position.x = ofClamp(position.x - (ofGetPreviousMouseX()-ofGetMouseX()), -scale * ofGetWidth() - 500, 500);
    position.y = ofClamp(position.y - (ofGetPreviousMouseY()-ofGetMouseY()), -scale * ofGetWidth() - 500, 500);
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(ofMouseEventArgs &evt) {
    scale.set(ofClamp(scale + 0.01 * (evt.scrollY), 1.0, 10.0));
}

//--------------------------------------------------------------


float ofApp::easeOutCubic(float pct_){
return 1 + (--pct_) * pct_ * pct_;
}
