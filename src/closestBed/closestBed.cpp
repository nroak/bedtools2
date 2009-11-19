// 
//  closestBed.cpp
//  BEDTools
//  
//  Created by Aaron Quinlan Spring 2009.
//  Copyright 2009 Aaron Quinlan. All rights reserved.
//
//  Summary:  Looks for the closest features in two BED files.
//
#include "lineFileUtilities.h"
#include "closestBed.h"

const int MAXSLOP = 256000000;  // 2*MAXSLOP = 512 megabases.
			        // We don't want to keep looking if we
			        // can't find a nearby feature within 512 Mb.
const int SLOPGROWTH = 2048000;


/*
	Constructor
*/
BedClosest::BedClosest(string &bedAFile, string &bedBFile, bool &forceStrand, string &tieMode) {

	this->bedAFile = bedAFile;
	this->bedBFile = bedBFile;
	this->forceStrand = forceStrand;
	this->tieMode = tieMode;

	this->bedA = new BedFile(bedAFile);
	this->bedB = new BedFile(bedBFile);
	
}

/*
	Destructor
*/
BedClosest::~BedClosest(void) {
}


/*
	reportNullB
	
	Writes a NULL B entry for cases where no closest BED was found
	Works for BED3 - BED6.
*/
void BedClosest::reportNullB() {
	if (bedB->bedType == 3) {
		printf("none\t-1\t-1\n");
	}
	else if (bedB->bedType == 4) {
		printf("none\t-1\t-1\t-1\n");
	}
	else if (bedB->bedType == 5) {
		printf("none\t-1\t-1\t-1\t-1\n");
	}
	else if (bedB->bedType == 6) {
		printf("none\t-1\t-1\t-1\t-1\t-1\n");
	}
}




void BedClosest::FindWindowOverlaps(BED &a, vector<BED> &hits) {
	
	int slop = 0;  // start out just looking for overlaps 
		       	   // within the current bin (~128Kb)	

	// update the current feature's start and end

	int aFudgeStart = 0;
	int aFudgeEnd;
	int numOverlaps = 0;
	vector<BED> closestB;
	float maxOverlap = 0;
	int minDistance = 999999999;


	if(bedB->bedMap.find(a.chrom) != bedB->bedMap.end()) {

		while ((numOverlaps == 0) && (slop <= MAXSLOP)) {
		
			if ((a.start - slop) > 0) {
				aFudgeStart = a.start - slop;
			}
			else {
				aFudgeStart = 0;
			}
			if ((a.start + slop) < 2 * MAXSLOP) {
				aFudgeEnd = a.end + slop;
			}
			else {
				aFudgeEnd = 2 * MAXSLOP;
			}
		
			bedB->binKeeperFind(bedB->bedMap[a.chrom], aFudgeStart, aFudgeEnd, hits);
	
			for (vector<BED>::iterator h = hits.begin(); h != hits.end(); ++h) {
				
				// if forcing strandedness, move on if the hit
				// is not on the same strand as A.
				if ((this->forceStrand) && (a.strand != h->strand)) {
					continue;		// continue force the next iteration of the for loop.
				}
		
				numOverlaps++;

				// do the actual features overlap?		
				int s = max(a.start, h->start);
				int e = min(a.end, h->end);
		
				if (s < e) {

					// is there enough overlap (default ~ 1bp)
					float overlap = (float)(e-s) / (float)(a.end - a.start);
	
					if ( overlap > 0 ) {
					
						// is this hit the closest?
						if (overlap > maxOverlap) {
							closestB.clear();
							closestB.push_back(*h);
							maxOverlap = overlap;
						}
						else if (overlap == maxOverlap) {
							closestB.push_back(*h);
						}
					}
				}
				else if (h->end < a.start){
					if ((a.start - h->end) <= minDistance) {
						closestB.clear();
						closestB.push_back(*h);
						minDistance = a.start - h->end;
					}	
				}
				else {
					if ((h->start - a.end) <= minDistance) {
						closestB.clear();
						closestB.push_back(*h);
						minDistance = h->start - a.end;
					}	
				}
				
			}
			/* if no overlaps were found, we'll 
			   widen the range by SLOPGROWTH in each direction
			   and search again.
			*/
			slop += SLOPGROWTH;
		}
	}
	else {
		bedA->reportBedTab(a);
		reportNullB(); 
	}

	if (numOverlaps > 0) {
		
		if (closestB.size() == 1) {
			bedA->reportBedTab(a); 
			bedB->reportBedNewLine(closestB[0]);
		}
		else {
			if (this->tieMode == "all") {
				for (vector<BED>::iterator b = closestB.begin(); b != closestB.end(); ++b) {
					bedA->reportBedTab(a); 
					bedB->reportBedNewLine(*b);
				}
			}
			else if (this->tieMode == "first") {
				bedA->reportBedTab(a); 
				bedB->reportBedNewLine(closestB[0]);
			}
			else if (this->tieMode == "last") {
				bedA->reportBedTab(a); 
				bedB->reportBedNewLine(closestB[closestB.size()-1]);
			}
		}
	}
}

 
void BedClosest::ClosestBed() {

	// load the "B" bed file into a map so
	// that we can easily compare "A" to it for overlaps
	bedB->loadBedFileIntoMap();


	string bedLine;
	BED bedEntry;                                                                                                                        
	int lineNum = 0;

	// open the BED file for reading
	if (bedA->bedFile != "stdin") {

		ifstream bed(bedA->bedFile.c_str(), ios::in);
		if ( !bed ) {
			cerr << "Error: The requested bed file (" <<bedA->bedFile << ") could not be opened. Exiting!" << endl;
			exit (1);
		}
		
		BED a;
		while (getline(bed, bedLine)) {
			
			if ((bedLine.find("track") != string::npos) || (bedLine.find("browser") != string::npos)) {
				continue;
			}
			else {
				vector<string> bedFields;
				Tokenize(bedLine,bedFields);

				lineNum++;
				if (bedA->parseBedLine(a, bedFields, lineNum)) {
					vector<BED> hits;
					FindWindowOverlaps(a, hits);
				}
			}
		}
	}
	// "A" is being passed via STDIN.
	else {
		
		BED a;
		while (getline(cin, bedLine)) {

			if ((bedLine.find("track") != string::npos) || (bedLine.find("browser") != string::npos)) {
				continue;
			}
			else {
				vector<string> bedFields;
				Tokenize(bedLine,bedFields);

				lineNum++;
				if (bedA->parseBedLine(a, bedFields, lineNum)) {
					vector<BED> hits;
					FindWindowOverlaps(a, hits);
				}
			}
		}
	}
}
// END ClosestBed


