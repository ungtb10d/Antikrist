// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file rftrain.cc

   @brief Bridge entry to training.

   @author Mark Seligman
*/

#include "rftrain.h"
#include "bv.h"
#include "train.h"
#include "predictorframe.h"
#include "frontier.h"
#include "pretree.h"
#include "partition.h"
#include "sfcart.h"
#include "splitnux.h"
#include "sampler.h"
#include "candrf.h"
#include "ompthread.h"
#include "coproc.h"

#include <algorithm>

void RfTrain::initProb(PredictorT predFixed,
                     const vector<double> &predProb) {
  CandRF::init(predFixed, predProb);
}


void RfTrain::initTree(IndexT leafMax) {
  PreTree::init(leafMax);
}


void RfTrain::initOmp(unsigned int nThread) {
  OmpThread::init(nThread);
}


void RfTrain::initSplit(unsigned int minNode,
                      unsigned int totLevels,
                      double minRatio,
		      const vector<double>& feSplitQuant) {
  IndexSet::immutables(minNode);
  Frontier::immutables(totLevels);
  SplitNux::immutables(minRatio, feSplitQuant);
}


void RfTrain::initMono(const PredictorFrame* frame,
		       const vector<double> &regMono) {
  SFRegCart::immutables(frame, regMono);
}


void RfTrain::deInit() {
  SplitNux::deImmutables();
  IndexSet::deImmutables();
  Frontier::deImmutables();
  PreTree::deInit();
  SampleNux::deImmutables();
  CandRF::deInit();
  SFRegCart::deImmutables();
  OmpThread::deInit();
}
