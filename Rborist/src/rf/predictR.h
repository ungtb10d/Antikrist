// Copyright (C)  2012-2022   Mark Seligman
//
// This file is part of rf.
//
// rf is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// rf is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rfR.  If not, see <http://www.gnu.org/licenses/>.

/**
   @file predictR.h

   @brief C++ interface to R entry for prediction.

   @author Mark Seligman
 */


#ifndef RF_PREDICT_R_H
#define RF_PREDICT_R_H

#include <Rcpp.h>
using namespace Rcpp;


/**
   @brief Prediction with separate test vector.

   @param sFrame contains the blocked observations.

   @param sTrain contains the trained object.

   @param sYTest is the vector of test values, possbily NULL.

   @param sOOB indicates whether testing is out-of-bag.

   @return wrapped predict object.
 */
RcppExport SEXP predictRcpp(const SEXP sFrame,
			    const SEXP sTrain,
			    const SEXP sSampler,
			    const SEXP sYTest,
			    const SEXP sArgs);


/**
   @brief Prediction with training response as test vector.

   Paramaters as with predictRcpp.
 */
RcppExport SEXP validateRcpp(const SEXP sFrame,
			     const SEXP sTrain,
			     const SEXP sSampler,
			     const SEXP sArgs);


/**
   @brief Bridge-variant PredictBridge pins unwrapped front-end structures.
 */
struct PBRf {

  static List predictCtg(const List& lDeframe,
			 const List& lTrain,
			 const List& lSampler,
			 const SEXP sYTest,
			 const List& lArgs);


  /**
     @brief Prediction for regression.  Parameters as above.
   */
  static List predictReg(const List& lDeframe,
                         const List& lTrain,
			 const List& lSampler,
                         const SEXP sYTest,
			 const List& lArgs);

  
  /**
     @brief Unwraps regression data structurs and moves to box.

     @return unique pointer to bridge-variant PredictBridge. 
   */
  static unique_ptr<struct PredictRegBridge> unwrapReg(const List& lDeframe,
						       const List& lTrain,
						       const List& lSampler,
						       const SEXP sYTest,
						       const List& lArgs);

  /**
     @brief Instantiates core prediction object and predicts quantiles.

     @return wrapped predictions.
   */
  List predict(SEXP sYTest,
               const vector<double>& quantile) const;

  /**
     @brief Unwraps regression data structurs and moves to box.

     @return unique pointer to bridge-variant PredictBridge. 
   */
  static unique_ptr<struct PredictCtgBridge> unwrapCtg(const List& lDeframe,
						       const List& lTrain,
						       const List& lSampler,
						       const SEXP sYTest,
						       const List& lArgs);


  static List summary(const List& lDeframe,
		      SEXP sYTest,
                      const struct PredictRegBridge* pBridge);


  /**
     @brief Builds a NumericMatrix representation of the quantile predictions.
     
     @param pBridge is the prediction handle.

     @return transposed core matrix if quantiles requested, else empty matrix.
  */
  static NumericMatrix getQPred(const struct PredictRegBridge* pBridge);


  static List getPrediction(const PredictRegBridge* pBridge);


  /**
     @param varTest is the variance of the test vector.
   */  
  static List getValidation(const PredictRegBridge* pBridge,
			    const NumericVector& yTestFE);
  

  static List getImportance(const struct PredictRegBridge* pBridge,
			    const NumericVector& yTestFE,
			    const CharacterVector& predNames);



private:
  /**
     @brief Instantiates core prediction object and predicts means.

     @return wrapped predictions.
   */
  static List predictReg(SEXP sYTest);

  /**
     @brief Instantiates core PredictRf object, driving prediction.

     @return wrapped prediction.
   */
  static List predictCtg(SEXP sYTest, const List& lTrain, const List& sFrame);


  /**
     @return regression test vector suitable for core.
   */
  static vector<double> regTest(const SEXP sYTest);


  /**
     @return quantile vector suitable for core.
   */
  static vector<double> quantVec(const List& lArgs);
  

  static vector<unsigned int> ctgTest(const List& lSampler,
				      const SEXP sYTest);
};


/**
   @brief Rf specialization of Core PredictReg, q.v.
 */
struct LeafRegRf {

  static List predict(const List &list,
                      SEXP sYTest,
                      class Predict *predict);
};

  
/**
   @brief Rf specialization of Core PredictCtg, q.v.
 */
struct LeafCtgRf {
  static List predict(const List &list,
		      SEXP sYTest,
		      const List &signature,
		      class Predict *predict,
		      bool doProb);
  /**
     @param sYTest is the one-based test vector, possibly null.

     @param rowNames are the row names of the test data.

     @return list of summary entries.   
  */
  static List summary(const List& lDeframe,
		      const List& lSampler,
                      const struct PredictCtgBridge* pBridge,
                      SEXP sYTest);


  /**
     @brief Produces census summary, which is common to all categorical
     prediction.

     @param rowNames is the user-supplied specification of row names.

     @return matrix of predicted categorical responses, by row.
  */
  static IntegerMatrix getCensus(const PredictCtgBridge* pBridge,
                                 const CharacterVector& levelsTrain,
                                 const CharacterVector& rowNames);

  
  /**
     @param rowNames is the user-supplied collection of row names.

     @return probability matrix if requested, otherwise empty matrix.
  */
  static NumericMatrix getProb(const PredictCtgBridge* pBridge,
                               const CharacterVector& levelsTrain,
                               const CharacterVector &rowNames);

  
  static List getPrediction(const PredictCtgBridge* pBridge,
			    const CharacterVector& levelsTrain,
			    const CharacterVector& ctgNames);
};


/**
   @brief Internal back end-style vectors cache annotations for
   per-tree access.
 */
struct TestCtg {
  const CharacterVector levelsTrain;
  const CharacterVector levels;
  const IntegerVector test2Merged;
  const vector<unsigned int> yTestZero;
  const unsigned int ctgMerged;

  TestCtg(const IntegerVector& yTest,
          const CharacterVector &levelsTrain_);

  
  /**
     @brief Determines summary array dimensions by reconciling cardinalities
     of training and test reponses.

     @return reconciled test vector.
  */
  static vector<unsigned int> reconcile(const IntegerVector& test2Train,
					const IntegerVector& yTestOne);
  

  /**
     @brief Reconciles factor encodings of training and test responses.
   */
  IntegerVector mergeLevels(const CharacterVector& levelsTest);


  List getValidation(const PredictCtgBridge* pBridge);


  List getImportance(const PredictCtgBridge* pBridge,
		     const CharacterVector& predNames);

  
  /**
     @brief Fills in misprediction vector.

     @param pBridge is the bridge handle.
  */
  NumericVector getMisprediction(const struct PredictCtgBridge* pBridge) const;
  

/**
   @brief Produces summary information specific to testing:  mispredction
   vector and confusion matrix.

   @param pBridge is the bridge handle.

   @param levelsTrain are the levels encountered during training.

   @return numeric matrix to accommodate wide count values.
 */
  NumericMatrix getConfusion(const PredictCtgBridge* pBridge,
			     const CharacterVector& levelsTrain) const;



  NumericMatrix mispredPermuted(const PredictCtgBridge* pBridge,
				const CharacterVector& predNames) const;



  NumericVector oobErrPermuted(const PredictCtgBridge* pBridge,
			       const CharacterVector& predNames) const;
};

#endif
