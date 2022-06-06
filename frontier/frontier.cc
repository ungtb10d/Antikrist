// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file frontier.cc

   @brief Maintains the sample-index representation of the frontier, typically by level.

   @author Mark Seligman
 */

#include "frontier.h"
#include "algparam.h"
#include "obsfrontier.h"
#include "indexset.h"
#include "trainframe.h"
#include "sampleobs.h"
#include "sampler.h"
#include "train.h"
#include "splitfrontier.h"
#include "interlevel.h"
#include "ompthread.h"
#include "branchsense.h"

unsigned int Frontier::totLevels = 0;

void Frontier::immutables(unsigned int totLevels) {
  Frontier::totLevels = totLevels;
}


void Frontier::deImmutables() {
  totLevels = 0;
}


unique_ptr<PreTree> Frontier::oneTree(const TrainFrame* frame,
                                      const Sampler* sampler,
				      unsigned int tIdx) {
  Frontier frontier(frame, sampler, tIdx);
  return frontier.levels();
}


Frontier::Frontier(const TrainFrame* frame_,
		   const Sampler* sampler,
		   unsigned int tIdx) :
  frame(frame_),
  sample(sampler->rootSample(tIdx)),
  bagCount(sample->getBagCount()),
  nCtg(sample->getNCtg()),
  interLevel(make_unique<InterLevel>(frame, this)),
  pretree(make_unique<PreTree>(frame, bagCount)),
  smTerminal(SampleMap(bagCount)) {
  frontierNodes.emplace_back(sample.get());
}


unique_ptr<PreTree> Frontier::levels() {
  smNonterm = SampleMap(bagCount);
  smNonterm.addNode(bagCount, 0);
  iota(smNonterm.sampleIndex.begin(), smNonterm.sampleIndex.end(), 0);
  while (!frontierNodes.empty()) {
    smNonterm = splitDispatch();
    frontierNodes = interLevel->overlap(this, smNonterm, frontierNodes);
  }
  pretree->setTerminals(move(smTerminal));

  return move(pretree);
}


SampleMap Frontier::splitDispatch() {
  earlyExit(interLevel->getLevel());
  CandType cand(interLevel.get());
  cand.precandidates(this, interLevel.get());

  interLevel->repartition(this, sample.get());
  splitFrontier = SplitFactoryT::factory(this);
  unique_ptr<BranchSense> branchSense = splitFrontier->split(cand);
  SampleMap smNext = surveySplits();

  ObsFrontier* cellFrontier = interLevel->getFront();
#pragma omp parallel default(shared) num_threads(OmpThread::nThread)
  {
#pragma omp for schedule(dynamic, 1)
    for (OMPBound splitIdx = 0; splitIdx < frontierNodes.size(); splitIdx++) {
      setScore(splitIdx);
      cellFrontier->updateMap(getNode(splitIdx), branchSense.get(), smNonterm, smTerminal, smNext);
    }
  }

  return smNext;
}


void Frontier::earlyExit(unsigned int level) {
  if (level + 1 == totLevels) {
    for (auto & iSet : frontierNodes) {
      iSet.setUnsplitable();
    }
  }
}


vector<IndexSet> Frontier::produce() const {
  vector<IndexSet> frontierNext;
  for (auto iSet : frontierNodes) {
    if (!iSet.isTerminal()) {
      frontierNext.emplace_back(this, iSet, true);
      frontierNext.emplace_back(this, iSet, false);
    }
  }

  return frontierNext;
}


SampleMap Frontier::surveySplits() {
  SampleMap smNext;
  for (auto & iSet : frontierNodes) {
    registerSplit(iSet, smNext);
  }

  smNext.sampleIndex = vector<IndexT>(smNext.getEndIdx());

  return smNext;
}


void Frontier::registerSplit(IndexSet& iSet,
			     SampleMap& smNext) {
  if (iSet.isTerminal()) {
    registerTerminal(iSet);
  }
  else {
    registerNonterminal(iSet, smNext);
  }
}


void Frontier::registerTerminal(IndexSet& iSet) {
  iSet.setIdxNext(smTerminal.getNodeCount());
  smTerminal.addNode(iSet.getExtent(), iSet.getPTId());
}


void Frontier::registerNonterminal(IndexSet& iSet, SampleMap& smNext) {
  iSet.setIdxNext(smNext.getNodeCount());
  smNext.addNode(iSet.getExtentSucc(true), iSet.getPTIdSucc(this, true));
  smNext.addNode(iSet.getExtentSucc(false), iSet.getPTIdSucc(this, false));
}


void Frontier::setScore(IndexT splitIdx) const {
  pretree->setScore(splitFrontier.get(), frontierNodes[splitIdx]);
}


IndexT Frontier::getPTIdSucc(IndexT ptId, bool senseTrue) const {
  return pretree->getSuccId(ptId, senseTrue);
}


void Frontier::updateSimple(const vector<SplitNux>& nuxMax,
			    BranchSense* branchSense) {
  IndexT splitIdx = 0;
  for (auto nux : nuxMax) {
    if (!nux.noNux()) {
      // splitUpdate() updates the runSet accumulators, so must
      // be invoked prior to updating the pretree's criterion state.
      frontierNodes[splitIdx].update(splitFrontier->splitUpdate(nux, branchSense));
      pretree->addCriterion(splitFrontier.get(), nux);
    }
    splitIdx++;
  }
}


void Frontier::updateCompound(const vector<vector<SplitNux>>& nuxMax) {
  pretree->consumeCompound(splitFrontier.get(), nuxMax);
}


vector<double> Frontier::sumsAndSquares(vector<vector<double> >& ctgSum) {
  vector<double> sumSquares(frontierNodes.size());

#pragma omp parallel default(shared) num_threads(OmpThread::nThread)
  {
#pragma omp for schedule(dynamic, 1)
    for (OMPBound splitIdx = 0; splitIdx < frontierNodes.size(); splitIdx++) {
      ctgSum[splitIdx] = frontierNodes[splitIdx].sumsAndSquares(sumSquares[splitIdx]);
    }
  }
  return sumSquares;
}



