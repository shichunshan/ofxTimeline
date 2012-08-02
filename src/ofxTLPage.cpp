/**
 * ofxTimeline
 *	
 * Copyright (c) 2011 James George
 * http://jamesgeorge.org + http://flightphase.com
 * http://github.com/obviousjim + http://github.com/flightphase 
 *
 * implementaiton by James George (@obviousjim) and Tim Gfrerer (@tgfrerer) for the 
 * Voyagers gallery National Maritime Museum 
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------
 *
 * ofxTimeline 
 * Lightweight SDK for creating graphic timeline tools in openFrameworks
 */

#include "ofxTLPage.h"
#include "ofxTLTicker.h"
#include "ofxTLUtils.h"
#include "ofxTimeline.h"

ofxTLPage::ofxTLPage()
:	trackContainerRect(ofRectangle(0,0,0,0)),
	headerHeight(0),
	defaultTrackHeight(0),
	isSetup(false),
	snappingTolerance(12),
	ticker(NULL),
	snapToOtherTracksEnabled(true),
	headerHasFocus(false),
	focusedTrack(NULL),
	draggingSelectionRectangle(false)
{
	//
}

ofxTLPage::~ofxTLPage(){
	if(isSetup){
		ofRemoveListener(timeline->events().zoomEnded, this, &ofxTLPage::zoomEnded);
        
		for(int i = 0; i < headers.size(); i++){
			if(tracks[headers[i]->name]->getCreatedByTimeline()){
				delete tracks[headers[i]->name];
			}
			delete headers[i];
		}
		headers.clear();
		tracks.clear();
	}
}

#pragma mark utility
void ofxTLPage::setup(){
	
	ofAddListener(timeline->events().zoomEnded, this, &ofxTLPage::zoomEnded);
	isSetup = true;
	headerHeight = 20;
	defaultTrackHeight = 50;
	loadTrackPositions(); //name must be set
}

//given a folder the page will look for xml files to load within that
void ofxTLPage::loadTracksFromFolder(string folderPath){
    for(int i = 0; i < headers.size(); i++){
		string filename = folderPath + tracks[headers[i]->name]->getXMLFileName();
        tracks[headers[i]->name]->setXMLFileName(filename);
        tracks[headers[i]->name]->load();
    }
}

void ofxTLPage::setTicker(ofxTLTicker* t){
	ticker = t;
}

void ofxTLPage::draw(){	
	for(int i = 0; i < headers.size(); i++){
		headers[i]->draw();
		tracks[headers[i]->name]->_draw();
	}	
	if(draggingInside && snappingEnabled){
		ofPushStyle();
		ofSetColor(255,255,255,100);
		for(int i = 0; i < snapPoints.size(); i++){
			ofLine(snapPoints[i], trackContainerRect.y, snapPoints[i], trackContainerRect.y+trackContainerRect.height);
		}
		ofPopStyle();
	}
	
	for(int i = 0; i < headers.size(); i++){
		tracks[headers[i]->name]->drawModalContent();
	}
    
    if(draggingSelectionRectangle){
		ofFill();
		ofSetColor(timeline->getColors().keyColor, 30);
		ofRect(selectionRectangle);
		
		ofNoFill();
		ofSetColor(timeline->getColors().keyColor, 255);
		ofRect(selectionRectangle);
		
	}
}

void ofxTLPage::timelineGainedFocus(){

}

void ofxTLPage::timelineLostFocus(){
    focusedTrack = NULL;
}

#pragma mark events
void ofxTLPage::mousePressed(ofMouseEventArgs& args){
    
	draggingInside = trackContainerRect.inside(args.x, args.y);
	ofxTLTrack* newFocus = NULL;
	if(draggingInside){
        if(ofGetModifierKeyShift()){
            draggingSelectionRectangle = true;
            selectionRectangleAnchor = ofVec2f(args.x,args.y);
            selectionRectangle = ofRectangle(args.x,args.y,0,0);
        }
		if(snappingEnabled){
			refreshSnapPoints();
		}
		headerHasFocus = false;
		for(int i = 0; i < headers.size(); i++){
			headers[i]->mousePressed(args);
            bool clickIsInHeader = headers[i]->getDrawRect().inside(args.x,args.y);
            bool clickIsInTrack = tracks[headers[i]->name]->getDrawRect().inside(args.x,args.y);
            bool clickIsInFooter = headers[i]->getFooterRect().inside(args.x,args.y);
            headerHasFocus |= (clickIsInFooter || clickIsInHeader);
            if(tracks[headers[i]->name]->isEnabled()){ 
                tracks[headers[i]->name]->_mousePressed(args);
				if(clickIsInTrack || clickIsInHeader){
                    newFocus = tracks[headers[i]->name];
                }
            }
		}
	}
    
    //TODO: explore multi-focus tracks for pasting into many tracks at once
    //paste events get sent to the focus track
    if(newFocus != NULL && newFocus != focusedTrack){
        if(focusedTrack != NULL){
            focusedTrack->lostFocus();
        }
        newFocus->gainedFocus();
        focusedTrack = newFocus; //is set to NULL when the whole timeline loses focus
    }    
}

void ofxTLPage::mouseMoved(ofMouseEventArgs& args){
	for(int i = 0; i < headers.size(); i++){
		headers[i]->mouseMoved(args);
		tracks[headers[i]->name]->_mouseMoved(args);
	}	
}

void ofxTLPage::mouseDragged(ofMouseEventArgs& args){
	if(draggingInside){
		bool snapped = false;
		float snapPercent = 0;
        if(draggingSelectionRectangle){
            selectionRectangle = ofRectangle(selectionRectangleAnchor.x, selectionRectangleAnchor.y, 
									 args.x-selectionRectangleAnchor.x, args.y-selectionRectangleAnchor.y);
			if(selectionRectangle.width < 0){
				selectionRectangle.x = selectionRectangle.x+selectionRectangle.width;
				selectionRectangle.width = -selectionRectangle.width;
			}
            
			if(selectionRectangle.height < 0){
				selectionRectangle.y = selectionRectangle.y+selectionRectangle.height;
				selectionRectangle.height = -selectionRectangle.height;
			}
            
            //clamp to the bounds
            if(selectionRectangle.x < trackContainerRect.x){
                float widthBehind =  trackContainerRect.x - selectionRectangle.x;
                selectionRectangle.x = trackContainerRect.x;
                selectionRectangle.width -= widthBehind;
            }
			
            if(selectionRectangle.y < trackContainerRect.y){
                float heightAbove =  trackContainerRect.y - selectionRectangle.y;
                selectionRectangle.y = trackContainerRect.y;
                selectionRectangle.height -= heightAbove;
            }
            
            if(selectionRectangle.x+selectionRectangle.width > trackContainerRect.x+trackContainerRect.width){
                selectionRectangle.width = (trackContainerRect.x+trackContainerRect.width - selectionRectangle.x);                
            }
            if(selectionRectangle.y+selectionRectangle.height > trackContainerRect.y+trackContainerRect.height){
                selectionRectangle.height = (trackContainerRect.y+trackContainerRect.height - selectionRectangle.y);
            }
        }
        else {
            if(snappingEnabled){
                
                refreshSnapPoints();
                
                float closestSnapDistance = snappingTolerance;
                float closestSnapPoint;
                //modify drag X if it should be snapped
                for(int i = 0; i < snapPoints.size(); i++){
                    float distanceToPoint = abs(args.x - snapPoints[i]);
                    if(distanceToPoint < closestSnapDistance){
                        closestSnapPoint = i;
                        closestSnapDistance = distanceToPoint; 
                    }
                }
                
                if(closestSnapDistance < snappingTolerance){
                    args.x = snapPoints[closestSnapPoint] + dragAnchor;
                    snapPercent = snapPoints[closestSnapPoint];
                    snapped = true;
                }
            }
            
            for(int i = 0; i < headers.size(); i++){
                headers[i]->mouseDragged(args);
                if(!headerHasFocus){
                    tracks[headers[i]->name]->mouseDragged(args, snapped);
                }
            }
        }        
	}
}

void ofxTLPage::mouseReleased(ofMouseEventArgs& args){
	if(draggingInside){
		for(int i = 0; i < headers.size(); i++){
			headers[i]->mouseReleased(args);
			tracks[headers[i]->name]->_mouseReleased(args);
		}		
		draggingInside = false;
	}
    
	if(draggingSelectionRectangle){
		draggingSelectionRectangle = false;
        ofRange timeRange = ofRange(timeline->screenXtoNormalizedX(selectionRectangle.x),
                                    timeline->screenXtoNormalizedX(selectionRectangle.x+selectionRectangle.width));
		for(int i = 0; i < headers.size(); i++){
            ofRectangle trackBounds = tracks[headers[i]->name]->getDrawRect();
			ofRange valueRange = ofRange(ofMap(selectionRectangle.y, trackBounds.y, trackBounds.y+trackBounds.height, 0.0, 1.0, true),
                                         ofMap(selectionRectangle.y+selectionRectangle.height, trackBounds.y, trackBounds.y+trackBounds.height, 0.0, 1.0, true));
            if(valueRange.min != valueRange.max){
				tracks[headers[i]->name]->regionSelected(timeRange, valueRange);
			}
		}		        
    }
}

void ofxTLPage::setDragAnchor(float anchor){
	dragAnchor = anchor;
}

void ofxTLPage::refreshSnapPoints(){
	//get the snapping points
	snapPoints.clear();
	if(snapToOtherTracksEnabled){
		for(int i = 0; i < headers.size(); i++){
			tracks[headers[i]->name]->getSnappingPoints(snapPoints);	
		}
	}
	if(ticker != NULL){
		ticker->getSnappingPoints(snapPoints);
	}	
}

//copy paste
string ofxTLPage::copyRequest(){
	string buf;
	for(int i = 0; i < headers.size(); i++){
		buf += tracks[headers[i]->name]->copyRequest();
	}	
	return buf;	
}

string ofxTLPage::cutRequest(){
	string buf;
	for(int i = 0; i < headers.size(); i++){
		buf += tracks[headers[i]->name]->cutRequest();
	}	
	return buf;
}

void ofxTLPage::pasteSent(string pasteboard){
    if(focusedTrack != NULL){
        focusedTrack->pasteSent(pasteboard);
    }
    
//	for(int i = 0; i < headers.size(); i++){
//		//only paste into where we are hovering
//        if(tracks[headers[i]->name]->hasFocus()){
//		//if(tracks[headers[i]->name]->getDrawRect().inside( ofVec2f(ofGetMouseX(), ofGetMouseY()) )){ //TODO: replace with hasFocus()
//			tracks[headers[i]->name]->pasteSent(pasteboard);
//		}
//	}	
}
		
void ofxTLPage::selectAll(){
	for(int i = 0; i < headers.size(); i++){
		//only paste into where we are hovering
		if(tracks[headers[i]->name]->getDrawRect().inside( ofVec2f(ofGetMouseX(), ofGetMouseY()) )){ //TODO: replace with hasFocus()
			tracks[headers[i]->name]->selectAll();
		}
	}
}

void ofxTLPage::keyPressed(ofKeyEventArgs& args){
	for(int i = 0; i < headers.size(); i++){
		tracks[headers[i]->name]->keyPressed(args);
	}
}

void ofxTLPage::nudgeBy(ofVec2f nudgePercent){
	for(int i = 0; i < headers.size(); i++){
		tracks[headers[i]->name]->nudgeBy(nudgePercent);
	}
}

void ofxTLPage::addTrack(string trackName, ofxTLTrack* track){

	ofxTLTrackHeader* newHeader = new ofxTLTrackHeader();
    newHeader->setTimeline(timeline);
	newHeader->setTrack(track);
	newHeader->setup();

	//	cout << "adding " << name << " current zoomer is " << zoomer->getDrawRect().y << endl;

	ofRectangle newHeaderRect = ofRectangle(trackContainerRect.x, trackContainerRect.height, trackContainerRect.width, headerHeight);
	newHeader->setDrawRect(newHeaderRect);
	newHeader->name = trackName;

	headers.push_back(newHeader);

	track->setup();
	
	ofRectangle drawRect;
	if(savedTrackPositions.find(trackName) != savedTrackPositions.end()){
		drawRect = savedTrackPositions[trackName];
	}
	else {
		drawRect = ofRectangle(0, newHeaderRect.y+newHeaderRect.height, ofGetWidth(), defaultTrackHeight);
	}

	track->setDrawRect(drawRect);
	track->setZoomBounds(currentZoomBounds);

	tracks[trackName] = track;
}

ofxTLTrack* ofxTLPage::getTrack(string trackName){
	if(tracks.find(trackName) == tracks.end()){
		ofLogError("ofxTLPage -- Couldn't find element named " + trackName + " on page " + name);
		return NULL;
	}
	return tracks[trackName];
}

void ofxTLPage::collapseAllTracks(){
	for(int i = 0; i < headers.size(); i++){
		headers[i]->collapseTrack();
	}
	recalculateHeight();
}

void ofxTLPage::bringTrackToTop(ofxTLTrack* track){
    for(int i = 0; i < headers.size(); i++){
        if(track == headers[i]->getTrack()){
            ofxTLTrackHeader* header = headers[i];
			for(int j = i; j > 0; j--){
                headers[j] = headers[j-1];
            }
            headers[0] = header;
            recalculateHeight();
            return;
        }
    }
    ofLogError("ofxTLPage::bringTrackToTop -- track " + track->getName() + " not found");
}

void ofxTLPage::bringTrackToBottom(ofxTLTrack* track){
    for(int i = 0; i < headers.size(); i++){
        if(track == headers[i]->getTrack()){
        	ofxTLTrackHeader* header = headers[i];
            for(int j = i; j < headers.size()-1; j++){
                headers[j] = headers[j+1];
            }
            headers[headers.size()-1] = header;
            recalculateHeight();
            return;
        }
    }
    ofLogError("ofxTLPage::bringTrackToBottom -- track " + track->getName() + " not found");
}

void ofxTLPage::removeTrack(string name){
	//TODO: bfd
}

void ofxTLPage::recalculateHeight(){
	float currentY = trackContainerRect.y;
	float totalHeight = 0;

	for(int i = 0; i < headers.size(); i++){
		ofRectangle thisHeader = headers[i]->getDrawRect();
		ofRectangle trackRectangle = tracks[ headers[i]->name ]->getDrawRect();
		
		//float startY = thisHeader.y+thisHeader.height;
        float startY = currentY+thisHeader.height;
		float endY = startY + trackRectangle.height;
		
		thisHeader.width = trackContainerRect.width;
		thisHeader.y = currentY;
		headers[i]->setDrawRect(thisHeader);
		trackRectangle.y = startY;
		trackRectangle.width = trackContainerRect.width;
		
		tracks[ headers[i]->name ]->setDrawRect( trackRectangle );
        
//        cout << "	setting track " << tracks[ headers[i]->name ]->getName() << " to " << trackRectangle.y << endl;
        
		currentY += thisHeader.height + trackRectangle.height + FOOTER_HEIGHT;
		totalHeight += thisHeader.height + trackRectangle.height + FOOTER_HEIGHT;
		
		savedTrackPositions[headers[i]->name] = trackRectangle;

	}
	
	trackContainerRect.height = totalHeight;
	saveTrackPositions();
}

ofRectangle ofxTLPage::getDrawRect(){
	return trackContainerRect;
}

float ofxTLPage::getComputedHeight(){
	return trackContainerRect.height;
}

float ofxTLPage::getBottomEdge(){
    return trackContainerRect.y + trackContainerRect.height;
}

void ofxTLPage::setSnapping(bool snapping){
	snappingEnabled = snapping;
}

void ofxTLPage::enableSnapToOtherTracks(bool snapToOthes){
	snapToOtherTracksEnabled = snapToOthes;
}

#pragma mark saving/restoring state
void ofxTLPage::loadTrackPositions(){
	ofxXmlSettings trackPositions;
	if(trackPositions.loadFile(name + "_trackPositions.xml")){
		
		//cout << "loading element position " << name << "_trackPositions.xml" << endl;
		
		trackPositions.pushTag("positions");
		int numtracks = trackPositions.getNumTags("element");
		for(int i = 0; i < numtracks; i++){
			string name = trackPositions.getAttribute("element", "name", "", i);
			trackPositions.pushTag("element", i);
			ofRectangle elementPosition = ofRectangle(ofToFloat(trackPositions.getValue("x", "0")),
													  ofToFloat(trackPositions.getValue("y", "0")),
													  ofToFloat(trackPositions.getValue("width", "0")),
													  ofToFloat(trackPositions.getValue("height", "0")));
			savedTrackPositions[name] = elementPosition;
			trackPositions.popTag();
		}
		trackPositions.popTag();
	}
}

void ofxTLPage::saveTrackPositions(){
	ofxXmlSettings trackPositions;
	trackPositions.addTag("positions");
	trackPositions.pushTag("positions");
	
	int curElement = 0;
	map<string, ofRectangle>::iterator it;
	for(it = savedTrackPositions.begin(); it != savedTrackPositions.end(); it++){
		trackPositions.addTag("element");
		trackPositions.addAttribute("element", "name", it->first, curElement);
		
		trackPositions.pushTag("element", curElement);
		trackPositions.addValue("x", it->second.x);
		trackPositions.addValue("y", it->second.y);
		trackPositions.addValue("width", it->second.width);
		trackPositions.addValue("height", it->second.height);
		
		trackPositions.popTag(); //element
		
		curElement++;
	}
	trackPositions.popTag();
	trackPositions.saveFile(name + "_trackPositions.xml");
}


#pragma mark getters/setters
void ofxTLPage::setZoomBounds(ofRange zoomBounds){
	currentZoomBounds = zoomBounds;
}

void ofxTLPage::zoomEnded(ofxTLZoomEventArgs& args){
	currentZoomBounds = args.currentZoom;
}

void ofxTLPage::setName(string newName){
	name = newName;
}

void ofxTLPage::unselectAll(){
	for(int i = 0; i < headers.size(); i++){
		tracks[headers[i]->name]->unselectAll();
	}	
}

void ofxTLPage::clear(){
	for(int i = 0; i < headers.size(); i++){
		tracks[headers[i]->name]->clear();
    }
}

void ofxTLPage::save(){
	for(int i = 0; i < headers.size(); i++){
		tracks[headers[i]->name]->save();
    }
}

string ofxTLPage::getName(){
	return name;
}

void ofxTLPage::setContainer(ofVec2f offset, float width){
	trackContainerRect.x = offset.x;
	trackContainerRect.y = offset.y;
	trackContainerRect.width = width;
	recalculateHeight();
}

void ofxTLPage::setHeaderHeight(float newHeaderHeight){
	headerHeight = newHeaderHeight;
	recalculateHeight();
}

void ofxTLPage::setDefaultTrackHeight(float newDefaultTrackHeight){
	defaultTrackHeight = newDefaultTrackHeight;
}

