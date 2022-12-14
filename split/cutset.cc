// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file cutset.cc

   @brief Manages workspace of numerical accumulators.

   @author Mark Seligman
 */

#include "splitnux.h"
#include "cutaccum.h"
#include "cutset.h"
#include "splitfrontier.h"
#include "interlevel.h"
#include "partition.h"


void CutSet::accumPreset() {
  cutSig = vector<CutSig>(nAccum);
}



CutSig CutSet::getCut(const SplitNux& nux) const {
  return cutSig[nux.getAccumIdx()];
}


CutSig CutSet::getCut(IndexT accumIdx) const {
  return cutSig[accumIdx];
}


void CutSet::setCut(IndexT accumIdx, const CutSig& sig) {
  cutSig[accumIdx] = sig;
}


bool CutSet::leftCut(const SplitNux& nux) const {
  return cutSig[nux.getAccumIdx()].cutLeft;
}


void CutSet::setCutSense(IndexT cutIdx, bool sense) {
  cutSig[cutIdx].cutLeft = sense;
}


double CutSet::getQuantRank(const SplitNux& nux) const {
  return cutSig[nux.getAccumIdx()].quantRank;
}


IndexT CutSet::getIdxRight(const SplitNux& nux) const {
  return cutSig[nux.getAccumIdx()].obsRight;
}


IndexT CutSet::getIdxLeft(const SplitNux& nux) const {
  return cutSig[nux.getAccumIdx()].obsLeft;
}


IndexT CutSet::getImplicitTrue(const SplitNux& nux) const {
  return cutSig[nux.getAccumIdx()].implicitTrue;
}


void CutSet::write(const InterLevel* interLevel,
		   const SplitNux& nux, const CutAccum& accum) {
  if (nux.getInfo() > 0) {
    cutSig[nux.getAccumIdx()].write(interLevel, nux, accum);
  }
}


void CutSig::write(const InterLevel* interLevel,
		   const SplitNux& nux,
		   const CutAccum& accum) {
  obsLeft = accum.obsLeft;
  obsRight = accum.obsRight;
  implicitTrue = accum.lhImplicit(nux);
  quantRank = accum.interpolateRank(interLevel, nux);
}
