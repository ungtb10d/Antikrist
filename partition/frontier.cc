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
#include "indexset.h"
#include "trainframe.h"
#include "sample.h"
#include "sampler.h"
#include "train.h"
#include "splitfrontier.h"
#include "deffrontier.h"
#include "path.h"
#include "ompthread.h"
#include "branchsense.h"

#include <numeric>


unsigned int Frontier::totLevels = 0;

void Frontier::immutables(unsigned int totLevels) {
  Frontier::totLevels = totLevels;
}


void Frontier::deImmutables() {
  totLevels = 0;
}


unique_ptr<PreTree> Frontier::oneTree(const TrainFrame* frame,
                                      Sampler* sampler) {
  sampler->rootSample(frame);
  unique_ptr<Frontier> frontier(make_unique<Frontier>(frame, sampler->getSample()));
  return frontier->levels(sampler->getSample());
}


Frontier::Frontier(const TrainFrame* frame_,
                   const Sample* sample) :
  frame(frame_),
  indexSet(vector<IndexSet>(1)),
  bagCount(sample->getBagCount()),
  nCtg(sample->getNCtg()),
  defMap(make_unique<DefFrontier>(frame, this)),
  nodeRel(false),
  idxLive(bagCount),
  relBase(vector<IndexT>(1)),
  rel2ST(vector<IndexT>(bagCount)),
  st2Split(vector<IndexT>(bagCount)),
  st2PT(vector<IndexT>(bagCount)),
  pretree(make_unique<PreTree>(frame->getCardExtent(), bagCount)) {
  indexSet[0].initRoot(sample);
  relBase[0] = 0;
  iota(rel2ST.begin(), rel2ST.end(), 0);
}


unique_ptr<PreTree> Frontier::levels(const Sample* sample) {
  defMap->stage(sample);

  unsigned int level = 0;
  while (!indexSet.empty()) {
    unique_ptr<BranchSense> branchSense = SplitFrontier::split(this);
    indexSet = splitDispatch(branchSense.get(), level);
    defMap->initPrecand();
    level++;
  }

  relFlush();
  pretree->cacheSampleMap(move(st2PT));
  return move(pretree);
}


vector<IndexSet> Frontier::splitDispatch(const BranchSense* branchSense,
					 unsigned int level) {
  SplitSurvey survey = nextLevel(level);
  for (auto & iSet : indexSet) {
    iSet.dispatch(this);
  }

  reindex(branchSense, survey);
  relBase = move(succBase);
  defMap->overlap(survey.splitNext, bagCount, idxLive, nodeRel);

  return produce(survey.splitNext);
}


SplitSurvey Frontier::nextLevel(unsigned int level) {
  if (level + 1 == totLevels) {
    for (auto & iSet : indexSet) {
      iSet.setExtinct();
    }
  }

  SplitSurvey survey = surveySet(indexSet);
  succBase = vector<IndexT>(survey.succCount(indexSet.size()));
  fill(succBase.begin(), succBase.end(), idxLive); // Previous level's extent

  succLive = 0;
  succExtinct = survey.splitNext; // Pseudo-indexing for extinct sets.
  liveBase = 0;
  extinctBase = survey.idxLive;
  idxLive = survey.idxLive;

  return survey;
}


SplitSurvey Frontier::surveySet(vector<IndexSet>& indexSet) const {
  SplitSurvey survey;
  for (auto iSet : indexSet) {
    iSet.surveySplit(survey);
  }

  return survey;
}


void Frontier::candMax(SplitNux& argMax,
		       const vector<SplitNux>& candV) const {
  if (!candV.empty()) {
    indexSet[candV.front().getNodeIdx()].candMax(candV, argMax);
  }
}


void Frontier::updateSimple(const SplitFrontier* sf,
			    const vector<SplitNux>& nuxMax) {
  for (auto nux : nuxMax) {
    if (!nux.noNux()) {
      indexSet[nux.getNodeIdx()].update(sf, nux);
      pretree->addCriterion(sf, nux);
    }
  }
}


void Frontier::updateCompound(const SplitFrontier* sf,
			      const vector<vector<SplitNux>>& nuxMax) {
  pretree->consumeCompound(sf, nuxMax);
}


IndexT Frontier::idxSucc(IndexT extent,
                         IndexT& offOut,
                         bool extinct) {
  IndexT succIdx;
  if (extinct) { // Pseudo split caches settings.
    succIdx = succExtinct++;
    offOut = extinctBase;
    extinctBase += extent;
  }
  else {
    succIdx = succLive++;
    offOut = liveBase;
    liveBase += extent;
  }
  succBase[succIdx] = offOut;

  return succIdx;
}


void Frontier::reindex(const BranchSense* branchSense,
		       const SplitSurvey& survey) {
  if (nodeRel) {
    nodeReindex(branchSense);
  }
  else {
    nodeRel = IdxPath::localizes(bagCount, survey.idxMax);
    if (nodeRel) {
      transitionReindex(branchSense, survey.splitNext);
    }
    else {
      stReindex(branchSense, survey.splitNext);
    }
  }
}


void Frontier::nodeReindex(const BranchSense* branchSense) {
  vector<IndexT> succST(idxLive);
  rel2PT = vector<IndexT>(idxLive);

#pragma omp parallel default(shared) num_threads(OmpThread::nThread)
  {
#pragma omp for schedule(dynamic, 1) 
    for (OMPBound splitIdx = 0; splitIdx < indexSet.size(); splitIdx++) {
      indexSet[splitIdx].reindex(branchSense, this, idxLive, succST);
    }
  }
  rel2ST = move(succST);
}


IndexT Frontier::relLive(IndexT relIdx,
                            IndexT targIdx,
                            IndexT path,
                            IndexT base,
                            IndexT ptIdx) {
  IndexT stIdx = rel2ST[relIdx];
  rel2PT[targIdx] = ptIdx;
  defMap->setLive(relIdx, targIdx, stIdx, path, base);

  return stIdx;
}


void Frontier::relExtinct(IndexT relIdx, IndexT ptId) {
  IndexT stIdx = rel2ST[relIdx];
  st2PT[stIdx] = ptId;
  defMap->setExtinct(relIdx, stIdx);
}


void Frontier::stReindex(const BranchSense* branchSense,
			 IndexT splitNext) {
  unsigned int chunkSize = 1024;
  IndexT nChunk = (bagCount + chunkSize - 1) / chunkSize;

#pragma omp parallel default(shared) num_threads(OmpThread::nThread)
  {
#pragma omp for schedule(dynamic, 1)
  for (OMPBound chunk = 0; chunk < nChunk; chunk++) {
    stReindex(branchSense, defMap->getSubtreePath(), splitNext, chunk * chunkSize, (chunk + 1) * chunkSize);
  }
  }
}


void Frontier::stReindex(const BranchSense* branchSense,
			 IdxPath* stPath,
                         IndexT splitNext,
                         IndexT chunkStart,
                         IndexT chunkNext) {
  for (IndexT stIdx = chunkStart; stIdx < min(chunkNext, bagCount); stIdx++) {
    IndexT splitIdx = st2Split[stIdx];
    if (stPath->isLive(stIdx)) {
      IndexT pathSucc, ptSucc;
      IndexT splitSucc = indexSet[splitIdx].offspring(branchSense, stIdx, pathSucc, ptSucc);
      st2Split[stIdx] = splitSucc;
      stPath->setSuccessor(stIdx, pathSucc, splitSucc < splitNext);
      st2PT[stIdx] = ptSucc;
    }
  }
}


void Frontier::transitionReindex(const BranchSense* branchSense,
				 IndexT splitNext) {
  IdxPath *stPath = defMap->getSubtreePath();
  for (IndexT stIdx = 0; stIdx < bagCount; stIdx++) {
    if (stPath->isLive(stIdx)) {
      IndexT pathSucc, idxSucc, ptSucc;
      IndexT splitIdx = st2Split[stIdx];
      IndexT splitSucc = indexSet[splitIdx].offspring(branchSense, stIdx, pathSucc, idxSucc, ptSucc);
      if (splitSucc < splitNext) {
        stPath->setLive(stIdx, pathSucc, idxSucc);
        rel2ST[idxSucc] = stIdx;
      }
      else {
        stPath->setExtinct(stIdx);
      }
      st2PT[stIdx] = ptSucc;
    }
  }
}


vector<IndexSet> Frontier::produce(IndexT splitNext) {
  vector<IndexSet> indexNext(splitNext);
  for (auto & iSet : indexSet) {
    iSet.succHands(this, indexNext);
  }
  return indexNext;
}


IndexT Frontier::getPTIdSucc(IndexT ptId, bool senseTrue) const {
  return pretree->getSuccId(ptId, senseTrue);
}


void Frontier::getPTIdTF(IndexT ptId, IndexT& ptTrue, IndexT& ptFalse) const {
  pretree->getSuccTF(ptId, ptTrue, ptFalse);
}


void Frontier::reachingPath(IndexT splitIdx,
                            IndexT parIdx,
                            const IndexRange& bufRange,
                            IndexT relBase,
                            unsigned int path) const {
  defMap->reachingPath(splitIdx, parIdx, bufRange, relBase, path);
}


vector<double> Frontier::sumsAndSquares(vector<vector<double> >& ctgSum) {
  vector<double> sumSquares(indexSet.size());

#pragma omp parallel default(shared) num_threads(OmpThread::nThread)
  {
#pragma omp for schedule(dynamic, 1)
    for (OMPBound splitIdx = 0; splitIdx < indexSet.size(); splitIdx++) {
      ctgSum[splitIdx] = indexSet[splitIdx].sumsAndSquares(sumSquares[splitIdx]);
    }
  }
  return sumSquares;
}
