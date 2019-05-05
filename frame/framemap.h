// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file framemap.h

   @brief Class definitions for maintenance of type-based data blocks.

   @author Mark Seligman
 */

#ifndef ARBORIST_FRAMEMAP_H
#define ARBORIST_FRAMEMAP_H

#include <vector>

using namespace std;


/**
   @brief Summarizes frame contents by predictor type.
 */
class FrameMap {
  unsigned int nRow;
  const vector<unsigned int> &feCard; // Factor predictor cardinalities.
  unsigned int nPredFac;
  unsigned int nPredNum;

  const unsigned int cardMax;  // High watermark of factor cardinalities.

 public:
  FrameMap(const vector<unsigned int> &feCard_,
	  unsigned int nPred_,
	  unsigned int nRow_);


  /**
     @brief Assumes numerical predictors packed in front of factor-valued.

     @return Position of fist factor-valued predictor.
  */
  inline unsigned int FacFirst() const {
    return nPredNum;
  }

  
  /**
     @brief Determines whether predictor is numeric or factor.

     @param predIdx is internal predictor index.

     @return true iff index references a factor.
   */
  inline bool isFactor(unsigned int predIdx)  const {
    return predIdx >= FacFirst();
  }


  /**
     @brief Computes block-relative position for a predictor.

     @param[out] thisIsFactor outputs true iff predictor is factor-valued.

     @return block-relative index.
  */
  inline unsigned int getIdx(unsigned int predIdx, bool &thisIsFactor) const{
    thisIsFactor = isFactor(predIdx);
    return thisIsFactor ? predIdx - FacFirst() : predIdx;
  }


  /**
     @brief Determines a dense position for factor-valued predictors.

     @param predIdx is a predictor index.

     @param nStride is a stride value.

     @param[out] thisIsFactor is true iff predictor is factor-valued.

     @return strided factor offset, if factor, else predictor index.
   */
  inline unsigned int getFacStride(unsigned int predIdx,
				unsigned int nStride,
				bool &thisIsFactor) const {
    unsigned int facIdx = getIdx(predIdx, thisIsFactor);
    return thisIsFactor ? nStride * nPredFac + facIdx : predIdx;
  }


  /**
     @return number or observation rows.
   */
  inline unsigned int getNRow() const {
    return nRow;
  }

  /**
     @return number of observation predictors.
  */
  inline unsigned int getNPred() const {
    return nPredFac + nPredNum;
  }

  /**
     @return number of factor predictors.
   */
  inline unsigned int getNPredFac() const {
    return nPredFac;
  }

  /**
     @return number of numerical predictors.
   */
  inline unsigned int getNPredNum() const {
    return nPredNum;
  }


  /**
     @brief Fixes contiguous factor ordering as numerical preceding factor.

     @return Position of first numerical predictor.
  */
  inline unsigned int NumFirst() const {
    return 0;
  }


  /**
     @brief Positions predictor within numerical block.

     @param predIdx is the core-ordered index of a predictor assumed to be numeric.

     @return Position of predictor within numerical block.
  */
  inline unsigned int getNumIdx(int predIdx) const {
    return predIdx - NumFirst();
  }


  /**
   @brief Computes cardinality of factor-valued predictor, or zero if not a
   factor.

   @param predIdx is the internal predictor index.

   @return factor cardinality or zero.
  */
  inline int getFacCard(int predIdx) const {
    return isFactor(predIdx) ? feCard[predIdx - FacFirst()] : 0;
  }

  
  /**
     @brief Maximal predictor cardinality.  Useful for packing.

     @return highest cardinality, if any, among factor predictors.
   */
  inline unsigned int getCardMax() const {
    return cardMax;
  }
};

#endif
