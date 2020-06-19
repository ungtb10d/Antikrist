// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file defmap.

   @brief Methods involving the most recently trained tree layers.

   @author Mark Seligman
 */

#include "defmap.h"
#include "deflayer.h"
#include "splitfrontier.h"
#include "splitnux.h"
#include "trainframe.h"
#include "rankedframe.h"
#include "path.h"

#include <numeric>
#include <algorithm>


DefMap::DefMap(const TrainFrame* frame_,
               IndexT bagCount) :
  frame(frame_),
  nPred(frame->getNPred()),
  nPredFac(frame->getNPredFac()),
  stPath(make_unique<IdxPath>(bagCount)),
  splitPrev(0), splitCount(1),
  rankedFrame(frame->getRankedFrame()),
  noRank(rankedFrame->NoRank()),
  history(vector<unsigned int>(0)),
  layerDelta(vector<unsigned char>(nPred)),
  runCount(vector<unsigned int>(nPredFac))
{

  layer.push_front(make_unique<DefLayer>(1, nPred, rankedFrame, bagCount, bagCount, false, this));
  IndexRange bufRange = IndexRange(0, bagCount);
  layer[0]->initAncestor(0, bufRange);
  fill(layerDelta.begin(), layerDelta.end(), 0);
  fill(runCount.begin(), runCount.end(), 0);
}


void DefMap::rootDef(PredictorT predIdx,
		     bool singleton,
		     IndexT implicitCount) {
  PreCand cand(SplitCoord(0, predIdx), 0); // Root node and buffer both zero.
  (void) layer[0]->define(cand, singleton, implicitCount);
  setRunCount(cand.splitCoord, false, singleton ? 1 : frame->getCardinality(predIdx));
}


void DefMap::eraseLayers(unsigned int flushCount) {
  if (flushCount > 0) {
    layer.erase(layer.end() - flushCount, layer.end());
  }
}


bool DefMap::factorStride(const SplitCoord& splitCoord,
		     unsigned int& facStride) const {
  bool isFactor;
  facStride = frame->getFacStride(splitCoord.predIdx, splitCoord.nodeIdx, isFactor);
  return isFactor;
}


unsigned int DefMap::preschedule(const SplitCoord& splitCoord,
				 vector<PreCand>& restageCand,
				 vector<PreCand>& preCand) const {
  reachFlush(splitCoord, restageCand);
  return layer[0]->preschedule(splitCoord, preCand) ? 1 : 0;
}


void DefMap::reachFlush(const SplitCoord& splitCoord,
			vector<PreCand>& restageCand) const {
  DefLayer *reachingLayer = reachLayer(splitCoord);
  reachingLayer->flushDef(getHistory(reachingLayer, splitCoord), restageCand);
}


bool DefMap::isSingleton(const PreCand& defCoord) const {
  return layer[0]->isSingleton(defCoord.splitCoord);
}


bool DefMap::isSingleton(const PreCand& defCoord,
			 PredictorT& runCount) const {
  runCount = getRunCount(defCoord);
  return layer[0]->isSingleton(defCoord.splitCoord);
}


IndexT DefMap::getImplicitCount(const PreCand& preCand) const {
  return layer[0]->getImplicit(preCand);
}


void DefMap::adjustRange(const PreCand& preCand,
			 IndexRange& idxRange) const {
  layer[0]->adjustRange(preCand, idxRange);
}


unsigned int DefMap::flushRear(SplitFrontier* splitFrontier) {
  unsigned int unflushTop = layer.size() - 1;

  // Capacity:  1 front layer + 'pathMax' back layers.
  // If at capacity, every reaching definition should be flushed
  // to current layer ut avoid falling off the deque.
  // Flushing prior to split assignment, rather than during, should
  // also save lookup time, as all definitions reaching from rear are
  // now at current layer.
  //
  if (!NodePath::isRepresentable(layer.size())) {
    layer.back()->flush(splitFrontier);
    unflushTop--;
  }

  // Walks backward from rear, purging non-reaching definitions.
  // Stops when a layer with no non-reaching nodes is encountered.
  //
  for (unsigned int off = unflushTop; off > 0; off--) {
    if (!layer[off]->nonreachPurge())
      break;
  }

  IndexT backDef = 0;
  for (auto lv = layer.begin() + unflushTop; lv != layer.begin(); lv--) {
    backDef += (*lv)->getDefCount();
  }

  IndexT thresh = backDef * efficiency;
  for (auto lv = layer.begin() + unflushTop; lv != layer.begin(); lv--) {
    if ((*lv)->flush(splitFrontier, thresh)) {
      unflushTop--;
    }
    else {
      break;
    }
  }

  // assert(unflushTop < layer.size();
  return layer.size() - 1 - unflushTop;
}


void DefMap::restage(ObsPart* obsPart,
		const PreCand& mrra) const {
  layer[mrra.del]->rankRestage(obsPart, mrra, layer[0].get());
}


DefMap::~DefMap() {
  for (auto & defLayer : layer) {
    defLayer->flush();
  }
  layer.clear();
}


void DefMap::overlap(IndexT splitNext,
                IndexT bagCount,
                IndexT idxLive,
		bool nodeRel) {
  splitPrev = splitCount;
  splitCount = splitNext;
  if (splitCount == 0) // No further splitting or restaging.
    return;

  layer.push_front(make_unique<DefLayer>(splitCount, nPred, rankedFrame, bagCount, idxLive, nodeRel, this));

  historyPrev = move(history);
  history = vector<unsigned int>(splitCount * (layer.size()-1));

  deltaPrev = move(layerDelta);
  layerDelta = vector<unsigned char>(splitCount * nPred);

  runCount = vector<PredictorT>(splitCount * nPredFac);
  fill(runCount.begin(), runCount.end(), 0);

  for (auto lv = layer.begin() + 1; lv != layer.end(); lv++) {
    (*lv)->reachingPaths();
  }
}


void
DefMap::backdate() const {
  if (layer.size() > 2 && layer[1]->isNodeRel()) {
    for (auto lv = layer.begin() + 2; lv != layer.end(); lv++) {
      if (!(*lv)->backdate(getFrontPath(1))) {
        break;
      }
    }
  }
}

  
void
DefMap::reachingPath(IndexT splitIdx,
		     IndexT parIdx,
		     const IndexRange& bufRange,
		     IndexT relBase,
		     unsigned int path) {
  for (unsigned int backLayer = 0; backLayer < layer.size() - 1; backLayer++) {
    history[splitIdx + splitCount * backLayer] = backLayer == 0 ? parIdx : historyPrev[parIdx + splitPrev * (backLayer - 1)];
  }

  inherit(splitIdx, parIdx);
  layer[0]->initAncestor(splitIdx, bufRange);
  
  // Places <splitIdx, start> pair at appropriate position in every
  // reaching path.
  //
  for (auto lv = layer.begin() + 1; lv != layer.end(); lv++) {
    (*lv)->pathInit(splitIdx, path, bufRange, relBase);
  }
}


void
DefMap::setLive(IndexT ndx,
		IndexT targIdx,
		IndexT stx,
		unsigned int path,
		IndexT ndBase) {
  layer[0]->setLive(ndx, path, targIdx, ndBase);

  if (!layer.back()->isNodeRel()) {
    stPath->setLive(stx, path, targIdx);  // Irregular write.
  }
}


void
DefMap::setExtinct(IndexT nodeIdx,
		   IndexT stIdx) {
  layer[0]->setExtinct(nodeIdx);
  setExtinct(stIdx);
}


void
DefMap::setExtinct(IndexT stIdx) {
  if (!layer.back()->isNodeRel()) {
    stPath->setExtinct(stIdx);
  }
}


IndexT
DefMap::getSplitCount(unsigned int del) const {
  return layer[del]->getSplitCount();
}


void
DefMap::addDef(const PreCand& defCoord,
	       bool singleton) {
  if (layer[0]->define(defCoord, singleton)) {
    layerDelta[defCoord.splitCoord.strideOffset(nPred)] = 0;
  }
}
  

IndexT
DefMap::getHistory(const DefLayer *reachLayer,
                   IndexT splitIdx) const {
  return reachLayer == layer[0].get() ? splitIdx : history[splitIdx + (reachLayer->getDel() - 1) * splitCount];
}


SplitCoord
DefMap::getHistory(const DefLayer* reachLayer,
		   const SplitCoord& coord) const {
  return reachLayer == layer[0].get() ? coord :
    SplitCoord(history[coord.nodeIdx + splitCount * (reachLayer->getDel() - 1)], coord.predIdx);
}

const IdxPath*
DefMap::getFrontPath(unsigned int del) const {
  return layer[del]->getFrontPath();
}


void DefMap::setSingleton(const SplitCoord& splitCoord) const {
  layer[0]->setSingleton(splitCoord);
}
