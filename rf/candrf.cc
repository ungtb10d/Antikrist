// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file candrf.cc

   @brief Methods building the list of splitting candidates.

   @author Mark Seligman
 */


#include "bheap.h"
#include "candrf.h"
#include "sfcart.h"
#include "deffrontier.h"
#include "callback.h"


PredictorT CandRF::predFixed = 0;
vector<double> CandRF::predProb;


void CandRF::init(PredictorT feFixed,
	     const vector<double>& feProb) {
  predFixed = feFixed;
  for (auto prob : feProb) {
    predProb.push_back(prob);
  }
}


void CandRF::deInit() {
  predFixed = 0;
  predProb.clear();
}


void CandRF::precandidates(DefFrontier* defFrontier) {
// TODO:  Preempt overflow by walking wide subtrees depth-nodeIdx.
  IndexT splitCount = defFrontier->getNSplit();
  PredictorT nPred = defFrontier->getNPred();
  IndexT cellCount = splitCount * nPred;
  
  auto ruPred = CallBack::rUnif(cellCount);
  vector<BHPair> heap(predFixed == 0 ? 0 : cellCount);

  for (IndexT splitIdx = 0; splitIdx < splitCount; splitIdx++) {
    IndexT splitOff = splitIdx * nPred;
    if (defFrontier->isUnsplitable(splitIdx)) { // Node cannot split.
      continue;
    }
    else if (predFixed == 0) { // Probability of predictor splitable.
      candidateProb(nPred, defFrontier, splitIdx, &ruPred[splitOff]);
    }
    else { // Fixed number of predictors splitable.
      candidateFixed(nPred, defFrontier, splitIdx, &ruPred[splitOff], &heap[splitOff]);
    }
  }
}


void CandRF::candidateProb(PredictorT nPred,
			   DefFrontier* defFrontier,
			   IndexT splitIdx,
			   const double ruPred[]) {
  for (PredictorT predIdx = 0; predIdx < nPred; predIdx++) {
    if (ruPred[predIdx] < predProb[predIdx]) {
      (void) defFrontier->preschedule(SplitCoord(splitIdx, predIdx));
    }
  }
}


void CandRF::candidateFixed(PredictorT nPred,
			    DefFrontier* defFrontier,
			    IndexT splitIdx,
			    const double ruPred[],
			    BHPair heap[]) {

  // Inserts negative, weighted probability value:  choose from lowest.
  for (PredictorT predIdx = 0; predIdx < nPred; predIdx++) {
    BHeap::insert(heap, predIdx, -ruPred[predIdx] * predProb[predIdx]);
  }

  // Pops 'predFixed' items in order of increasing value.
  PredictorT schedCount = 0;
  for (PredictorT heapSize = nPred; heapSize > 0; heapSize--) {
    SplitCoord splitCoord(splitIdx, BHeap::slotPop(heap, heapSize - 1));
    if (defFrontier->preschedule(splitCoord)) {
      if (++schedCount == predFixed) {
	break;
      }
    }
  }
}
