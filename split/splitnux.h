// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef SPLIT_SPLITNUX_H
#define SPLIT_SPLITNUX_H

/**
   @file splitnux.h

   @brief Minimal container capable of characterizing split.

   @author Mark Seligman
 */

#include "splitcoord.h"
#include "sumcount.h"
#include "typeparam.h"

#include <vector>


class SplitNux {
  static constexpr double minRatioDefault = 0.0;
  static double minRatio;

  SplitCoord splitCoord;
  IndexRange idxRange; // Fixed from IndexSet.
  IndexT accumIdx; // Index into accumulator workspace.
  double sum; // Initial sum, fixed by index set.
  IndexT sCount; // Initial sample count, fixed by index set.
  unsigned char bufIdx; // Base buffer for SR indices.
  IndexT implicitCount; // Initialized from IndexSet.
  IndexT ptId; // Index into tree:  offset from position given by index set.

  // Set during splitting:
  double info; // Weighted variance or Gini, currently.
  
public:  
  static vector<double> splitQuant; // Where within CDF to cut.  MOVE to CutSet.
/**
   @brief Builds static quantile splitting vector from front-end specification.

   @param feSplitQuant specifies the splitting quantiles for numerical predictors.
  */
  static void immutables(double minRatio_,
			 const vector<double>& feSplitQuant);

  
  /**
     @brief Empties the static quantile splitting vector.
   */
  static void deImmutables();


  /**
     @brief Computes information gain using pre-bias and accumulator.
   */
  void infoGain(const class SplitFrontier* sf,
		const class CutAccum* accum);


  /**
     @brief As above, but only employs pre-bias.

     @return true iff gain greater than zero.
   */
  void infoGain(const class SplitFrontier* splitFrontier);


  /**
     @brief As above, but only employs accumulator.
   */
  void infoGain(const class CutAccum* accum);
  

  /**
     @return desired cut range.
   */
  IndexRange cutRange(const class CutSet* cutSet,
		      bool leftRange) const;
  

  /**
     @brief Computes cut-based left range for numeric splits.
   */
  IndexRange cutRangeLeft(const class CutSet* cutSet) const;


  /**
     @brief Computes cut-based right range for numeric splits.
   */
  IndexRange cutRangeRight(const class CutSet* cutSet) const;


  /**
     @brief Trivial constructor. 'info' value of 0.0 ensures ignoring.
  */  
  SplitNux() : splitCoord(SplitCoord()),
	       accumIdx(0),
	       sum(0.0),
	       sCount(0),
	       bufIdx(0),
	       implicitCount(0),
	       ptId(0),
	       info(0.0) {
  }

  
  /**
     @brief Copy constructor:  post splitting.
   */
  SplitNux(const SplitNux& nux) :
    splitCoord(nux.splitCoord),
    idxRange(nux.idxRange),
    accumIdx(nux.accumIdx),
    sum(nux.sum),
    sCount(nux.sCount),
    bufIdx(nux.bufIdx),
    implicitCount(nux.implicitCount),
    ptId(nux.ptId),
    info(nux.info) {
  }

  SplitNux& operator= (const SplitNux& nux) {
    splitCoord = nux.splitCoord;
    idxRange = nux.idxRange;
    accumIdx = nux.accumIdx;
    sum = nux.sum;
    sCount = nux.sCount;
    bufIdx = nux.bufIdx;
    implicitCount = nux.implicitCount;
    ptId = nux.ptId;
    info = nux.info;

    return *this;
  }

  
  /**
     @brief Transfer constructor over iteratively-encoded IndexSet.

     @param idx positions nux within a multi-criterion set.
   */
  SplitNux(const SplitNux& parent,
	   const class IndexSet* iSet,
	   bool sense,
	   IndexT idx = 0);


  /**
     @brief Pre-split constructor.
   */
  SplitNux(const DefCoord& preCand,
	   const class SplitFrontier* splitFrontier,
	   const class DefMap* defMap,
	   PredictorT runCount);

  
  ~SplitNux() {
  }


  /**
     @brief Reports whether frame identifies underlying predictor as factor-valued.

     @return true iff splitting predictor is a factor.
   */
  bool isFactor(const class SummaryFrame* frame) const;
  

  /**
     @brief Passes through to frame method.

     @return cardinality iff factor-valued predictor else zero.
   */
  PredictorT getCardinality(const class SummaryFrame*) const;


  
  /**
     @brief Reports whether potential split be informative with respect to a threshold.

     @param minInfo is an information threshold.

     @return true iff information content exceeds the threshold.
   */
  bool isInformative(double minInfo) const {
    return info > minInfo;
  }


  /**
     @return minInfo threshold.
   */
  double getMinInfo() const {
    return minRatio * info;
  }


  /**
     @brief Resets trial information value of this greater.

     @param[out] runningMax holds the running maximum value.

     @return true iff value revised.
   */
  bool maxInfo(double& runningMax) const {
    if (info > runningMax) {
      runningMax = info;
      return true;
    }
    return false;
  }


  auto getPTId() const {
    return ptId;
  }

  
  auto getPredIdx() const {
    return splitCoord.predIdx;
  }

  auto getNodeIdx() const {
    return splitCoord.nodeIdx;
  }
  

  auto getDefCoord() const {
    return DefCoord(splitCoord, bufIdx);
  }

  
  auto getSplitCoord() const {
    return splitCoord;
  }

  auto getBufIdx() const {
    return bufIdx;
  }
  
  auto getAccumIdx() const {
    return accumIdx;
  }

  /**
     @brief Reference getter for over-writing info member.
  */
  double& refInfo() {
    return info;
  }
  
  auto getInfo() const {
    return info;
  }

  
  void setInfo(double info) {
    this->info = info;
  }


  /**
     @brief Indicates whether this is an empty placeholder.
   */
  inline bool noNux() const {
    return splitCoord.noCoord();
  }


  auto getRange() const {
    return idxRange;
  }
  
  
  auto getIdxStart() const {
    return idxRange.getStart();
  }

  
  auto getExtent() const {
    return idxRange.getExtent();
  }

  
  auto getIdxEnd() const {
    return idxRange.getEnd() - 1;
  }

  auto getSCount() const {
    return sCount;
  }
  

  auto getSum() const {
    return sum;
  }

  
  /**
     @return Count of implicit indices associated with IndexSet.
  */   
  IndexT getImplicitCount() const {
    return implicitCount;
  }
};


#endif
