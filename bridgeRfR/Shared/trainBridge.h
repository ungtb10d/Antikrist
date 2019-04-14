// Copyright (C)  2012-2019   Mark Seligman
//
// This file is part of ArboristBridgeR.
//
// ArboristBridgeR is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// ArboristBridgeR is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ArboristBridgeR.  If not, see <http://www.gnu.org/licenses/>.

/**
   @file bridgeTrain.h

   @brief C++ interface to R entry for training.

   @author Mark Seligman
 */

#ifndef ARBORIST_TRAIN_BRIDGE_H
#define ARBORIST_TRAIN_BRIDGE_H

#include <Rcpp.h>
using namespace Rcpp;

#include <memory>
#include <string>
#include <vector>
using namespace std;

RcppExport SEXP TrainRF(const SEXP sArgList);


struct TrainBridge {

  // Training granularity.  Values guesstimated to minimize footprint of
  // Core-to-Bridge copies while also not over-allocating:
  static constexpr unsigned int treeChunk = 20;
  static constexpr double allocSlop = 1.2;

  static unsigned int nCtg; // # outcome categores:  classification iff > 0.
  static bool verbose; // Whether to report progress while training.
  
  const unsigned int nTree; // # trees under training.
  unique_ptr<class BagBridge> bag; // Summarizes row bagging, by tree.
  unique_ptr<struct FBTrain> forest; // Pointer to core forest.
  NumericVector predInfo; // Forest-wide sum of predictors' split information.
  unique_ptr<struct LBTrain> leaf; // Pointer to core leaf frame.

  /**
     @brief Regression constructor.

     @param nTree is the number of trees in the forest.

     @param predMap maps core to front-end predictor indices.

     @param yTrain is the training response vector.
   */
  TrainBridge(unsigned int nTree_,
              const IntegerVector& predMap,
              const NumericVector& yTrain);

  /**
     @brief Classification constructor.  Parameters as above.
   */
  TrainBridge(unsigned int nTree_,
              const IntegerVector& predMap,
              const IntegerVector& yTrain);


  /**
     @brief Trains classification forest.

     @param y is the categorical response vector.

     @param classWeight is the class-weighted response.

     @param frameTrain summarizes the predictor set.

     @param rankedPair contains the pre-formatted observation rankings.

     @param predMap maps core to front-end predictor indices.

     @param nTree is the number of trees in the forest.

     @return R-style list of trained summaries.
  */
  static List classification(const IntegerVector &y,
                             const NumericVector &classWeight,
                             const class FrameTrain *frameTrain,
                             const class RankedSet *rankedPair,
                             const IntegerVector &predMap,
                             unsigned int nTree,
                             vector<string> &diag);
  
  /**
     @brief Trains regression forest.

     @param y is the numeric response vector.

     @param frameTrain summarizes the predictor set.

     @param rankedPair contains the pre-formatted observation rankings.

     @param predMap maps core to front-end predictor indices.

     @param nTree is the number of trees in the forest.

     @return R-style list of trained summaries.
  */
  static List regression(const NumericVector &y,
                         const class FrameTrain *frameTrain,
                         const class RankedSet *rankedPair,
                         const IntegerVector &predMap,
                         unsigned int nTree,
                         vector<string> &diag);

  /**
      @brief R-language interface to response caching.

      Class weighting constructs a proxy response from category frequency.
      The response is then jittered to diminish the possibility of ties
      during scoring.  The magnitude of the jitter, then, should be scaled
      so that no combination of samples can "vote" themselves into a
      false plurality.

      @param y is the (zero-based) categorical response vector.

      @param classWeight are user-suppled weightings of the categories.

      @return vector of final (i.e., jittered and scaled) class weights.
  */
  static NumericVector ctgProxy(const IntegerVector &y,
                                const NumericVector &classWeight);

  /**
     @brief Scales the per-predictor information quantity by # trees.

     @param predMap is the core-to-front map of predictor indices.

     @return remapped vector of scaled information values.
   */
  NumericVector scalePredInfo(const IntegerVector &predMap);

  
  /**
     @return implicit R_NilValue.
   */
  static SEXP init(const List &argList,
                   const class FrameTrain* frameTrain,
                   const IntegerVector &predMap);

  /**
     @brief Unsets static initializations.

     @return implicit R_NilValue.
   */
  static SEXP deInit();
  

  /**
     @brief Static entry into training.

     @param argList is the user-supplied argument list.

     @param predMap maps core to front-end predictor indices.

     @param facCard gives the cardinality of a predictor, zero if numeric.

     @param nRow is the number of observations being trained.

     @return R-style list of trained summaries.
   */
  static List train(const List &argList,
                    const IntegerVector &predMap,
                    const vector<unsigned int> &facCard,
                    unsigned int nRow);

  /**
     @brief Consumes core representation of a trained tree for writing.

     @unsigned int tIdx is the absolute tree index.

     @param scale guesstimates a reallocation size.
   */
  void consume(const class Train* train,
               unsigned int tIdx,
               unsigned int chunkSize);

  
  /**
     @brief Whole-forest summary of trained chunks.

     @param predMap[] maps core predictor indices to front-end.

     @param diag accumulates diagnostic messages.

     @return the summary.
   */
  List summarize(const IntegerVector& predMap,
                 const vector<string>& diag);

private:
  /**
     @brief Estimates scale factor for full-forest reallocation.

     @param treesTot is the total number of trees trained so far.

     @return scale estimation sufficient to accommodate entire forest.
   */
  inline double safeScale(unsigned int treesTot) const {
    return (treesTot == nTree ? 1 : allocSlop) * double(nTree) / treesTot;
  }
};
#endif