// Copyright (C)  2012-2022  Mark Seligman
//
// This file is part of rf.
//
// rfR is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// rfR is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with rfR.  If not, see <http://www.gnu.org/licenses/>.

/**
   @file predictRf.cc

   @brief C++ interface to R entry for prediction methods.

   @author Mark Seligman
 */

#include "predictbridge.h"
#include "predictR.h"
#include "samplerR.h"
#include "leafR.h"
#include "forestR.h"
#include "forestbridge.h"
#include "samplerbridge.h"
#include "leafbridge.h"
#include "rleframeR.h"
#include "signature.h"

#include <algorithm>

RcppExport SEXP predictRcpp(const SEXP sDeframe,
		     const SEXP sTrain,
		     const SEXP sSampler,
		     const SEXP sYTest,
		     const SEXP sArgs) {
  BEGIN_RCPP
    List lArgs(sArgs);
  bool verbose = as<bool>(lArgs["verbose"]);
  if (verbose)
    Rcout << "Entering prediction" << endl;

  List summary;
  List lSampler(sSampler);
  SEXP yTrain = lSampler["yTrain"];
  if (Rf_isFactor(yTrain))
    summary = PBRf::predictCtg(List(sDeframe), List(sTrain), lSampler, sYTest, lArgs);
  else
    summary = PBRf::predictReg(List(sDeframe), List(sTrain), lSampler, sYTest, List(sArgs));

  if (verbose)
    Rcout << "Prediction completed" << endl;
  
  return summary;
  END_RCPP
}


RcppExport SEXP validateRcpp(const SEXP sDeframe,
			 const SEXP sTrain,
			 const SEXP sSampler,
			 const SEXP sArgs) {
  BEGIN_RCPP

    List lArgs(sArgs);
  bool verbose = as<bool>(lArgs["verbose"]);
  if (verbose)
    Rcout << "Entering validation" << endl;

  List lSampler(sSampler);
  SEXP yTrain = lSampler["yTrain"];
  List summary;
  if (Rf_isFactor(yTrain))
    summary = PBRf::predictCtg(List(sDeframe), List(sTrain), lSampler, yTrain, lArgs);
  else
    summary = PBRf::predictReg(List(sDeframe), List(sTrain), lSampler, yTrain, lArgs);

  if (verbose)
    Rcout << "Validation completed" << endl;

  return summary;
  
  END_RCPP
}


List PBRf::predictReg(const List& lDeframe,
		      const List& lTrain,
		      const List& lSampler,
		      const SEXP sYTest,
		      const List& lArgs) {
  BEGIN_RCPP

    unique_ptr<PredictRegBridge> pBridge(unwrapReg(lDeframe, lTrain, lSampler, sYTest, lArgs));
  pBridge->predict();

  return summary(lDeframe, sYTest, pBridge.get());
  
  END_RCPP
}


unique_ptr<PredictRegBridge> PBRf::unwrapReg(const List& lDeframe,
					     const List& lTrain,
					     const List& lSampler,
					     const SEXP sYTest,
					     const List& lArgs) {
  unique_ptr<SamplerBridge> samplerBridge(SamplerR::unwrapPredict(lSampler, lDeframe, as<bool>(lArgs["bagging"])));
  unique_ptr<LeafBridge> leafBridge(LeafR::unwrap(lTrain, samplerBridge.get()));
  return make_unique<PredictRegBridge>(RLEFrameR::unwrap(lDeframe),
				       ForestRf::unwrap(lTrain),
				       move(samplerBridge),
				       move(leafBridge),
				       regTest(sYTest),
				       as<unsigned int>(lArgs["impPermute"]),
				       as<bool>(lArgs["trapUnobserved"]),
				       as<unsigned int>(lArgs["nThread"]),
				       quantVec(lArgs));
}


vector<double> PBRf::regTest(const SEXP sYTest) {
  vector<double> yTest;
  if (!Rf_isNull(sYTest)) {
    NumericVector yTestFE(as<NumericVector>(sYTest));
    yTest = as<vector<double>>(yTestFE);
  }
  return yTest;
}


vector<double> PBRf::quantVec(const List& lArgs) {
  vector<double> quantile;
  if (!Rf_isNull(lArgs["quantVec"])) {
    NumericVector quantVec(as<NumericVector>(lArgs["quantVec"]));
    quantile = vector<double>(quantVec.begin(), quantVec.end());
  }
  return quantile;
}


List PBRf::summary(const List& lDeframe, SEXP sYTest, const PredictRegBridge* pBridge) {
  BEGIN_RCPP

  List summaryReg;
  if (Rf_isNull(sYTest)) {
    summaryReg = List::create(
			      _["prediction"] = getPrediction(pBridge)
			      );
  }
  else if (!pBridge->permutes()) { // Validation, no importance.
    summaryReg = List::create(
			      _["prediction"] = getPrediction(pBridge),
			      _["validation"] = getValidation(pBridge, NumericVector((SEXP)sYTest))
			      );
  }
  else { // Validation + importance
    summaryReg = List::create(
			      _["prediction"] = getPrediction(pBridge),
			      _["validation"] = getValidation(pBridge, NumericVector((SEXP)sYTest)),
			      _["importance"] = getImportance(pBridge, NumericVector((SEXP) sYTest), Signature::unwrapColNames(lDeframe))
			      );
  }
  summaryReg.attr("class") = "SummaryReg";

  return summaryReg;
  END_RCPP
}


List PBRf::predictCtg(const List& lDeframe,
                      const List& lTrain,
		      const List& lSampler,
                      const SEXP sYTest,
		      const List& lArgs) {

  BEGIN_RCPP

    unique_ptr<PredictCtgBridge> pBridge(unwrapCtg(lDeframe, lTrain, lSampler, sYTest, lArgs));
  pBridge->predict();

  return LeafCtgRf::summary(lDeframe, lSampler, pBridge.get(), sYTest);

  END_RCPP
}


unique_ptr<PredictCtgBridge> PBRf::unwrapCtg(const List& lDeframe,
					     const List& lTrain,
					     const List& lSampler,
					     const SEXP sYTest,
					     const List& lArgs) {
  unique_ptr<SamplerBridge> samplerBridge(SamplerR::unwrapPredict(lSampler, lDeframe, as<bool>(lArgs["bagging"])));
  unique_ptr<LeafBridge> leafBridge(LeafR::unwrap(lTrain, samplerBridge.get()));
  return make_unique<PredictCtgBridge>(RLEFrameR::unwrap(lDeframe),
				       ForestRf::unwrap(lTrain),
				       move(samplerBridge),
				       move(leafBridge),
				       ctgTest(lSampler, sYTest),
				       as<unsigned int>(lArgs["impPermute"]),
				       as<bool>(lArgs["ctgProb"]),
				       as<bool>(lArgs["trapUnobserved"]),
				       as<unsigned int>(lArgs["nThread"]));
}


vector<unsigned int> PBRf::ctgTest(const List& lSampler, const SEXP sYTest) {
  if (!Rf_isNull(sYTest)) { // Makes zero-based copy.
    IntegerVector yTrain(as<IntegerVector>(lSampler["yTrain"]));
    TestCtg testCtg(sYTest, as<CharacterVector>(yTrain.attr("levels")));
    return testCtg.yTestZero;
  }
  else {
    vector<unsigned int> yTest;
    return yTest;
  }  
}


List PBRf::getPrediction(const PredictRegBridge* pBridge) {
  BEGIN_RCPP

  List prediction = List::create(
				 _["yPred"] = pBridge->getYPred(),
				 _["qPred"] = getQPred(pBridge),
				 _["qEst"] = pBridge->getQEst()
				 );
  prediction.attr("class") = "PredictReg";
  return prediction;

  END_RCPP
}


NumericMatrix PBRf::getQPred(const PredictRegBridge* pBridge) {
  BEGIN_RCPP

  size_t nRow(pBridge->getNRow());
  auto qPred = pBridge->getQPred();
  return qPred.empty() ? NumericMatrix(0) : transpose(NumericMatrix(qPred.size() / nRow, nRow, qPred.begin()));
    
  END_RCPP
}


List PBRf::getValidation(const PredictRegBridge* pBridge,
			 const NumericVector& yTestFE) {
  BEGIN_RCPP

  double sse = pBridge->getSSE();
  size_t nRow = yTestFE.length();
  List validation = List::create(_["mse"] = sse / nRow,
				 _["rsq"] = nRow == 1 ? 0.0 : 1.0 - sse / (var(yTestFE) * (nRow - 1)),
				 _["mae"] = pBridge->getSAE() / nRow
				 );
  validation.attr("class") = "ValidReg";
  return validation;

  END_RCPP
}


List PBRf::getImportance(const PredictRegBridge* pBridge,
			 const NumericVector& yTestFE,
			 const CharacterVector& predNames) {
  BEGIN_RCPP

  auto ssePerm = pBridge->getSSEPermuted();
  NumericVector mseOut(ssePerm.begin(), ssePerm.end());
  mseOut = mseOut / yTestFE.length();
  mseOut.attr("names") = predNames;

  List importance = List::create(_["mse"] = mseOut);
  importance("class") = "ImportanceReg";
  return importance;

  END_RCPP
}


TestCtg::TestCtg(const IntegerVector& yTestOne,
                 const CharacterVector& levelsTrain_) :
  levelsTrain(levelsTrain_),
  levels(CharacterVector(as<CharacterVector>(yTestOne.attr("levels")))),
  test2Merged(mergeLevels(levels)),
  yTestZero(reconcile(test2Merged, yTestOne)),
  ctgMerged(*max_element(yTestZero.begin(), yTestZero.end()) + 1) {
}


IntegerVector TestCtg::mergeLevels(const CharacterVector& levelsTest) {
  BEGIN_RCPP
  IntegerVector test2Merged(match(levelsTest, levelsTrain));
  IntegerVector sq = seq(0, test2Merged.length() - 1);
  IntegerVector idxNA = sq[is_na(test2Merged)];
  if (idxNA.length() > 0) {
    warning("Uninferable test levels not encountered in training");
    int proxy = levelsTrain.length() + 1;
    for (R_len_t i = 0; i < idxNA.length(); i++) {
      int idx = idxNA[i];
      test2Merged[idx] = proxy++;
    }
  }
  return test2Merged - 1;
  END_RCPP
}


vector<unsigned int> TestCtg::reconcile(const IntegerVector& test2Merged,
					const IntegerVector& yTestOne) {
  IntegerVector yZero = yTestOne - 1;
  vector<unsigned int> yZeroOut(yZero.length());
  for (R_len_t i = 0; i < yZero.length(); i++) {
    yZeroOut[i] = test2Merged[yZero[i]];
  }
  return yZeroOut;
}


List LeafCtgRf::summary(const List& lDeframe, const List& lSampler, const PredictCtgBridge* pBridge, SEXP sYTest) {
  BEGIN_RCPP

  IntegerVector yTrain(as<IntegerVector>(lSampler["yTrain"]));
  CharacterVector levelsTrain(as<CharacterVector>(yTrain.attr("levels")));
  CharacterVector ctgNames(Signature::unwrapRowNames(lDeframe));

  List summaryCtg;
  if (Rf_isNull(sYTest)) {
    summaryCtg = List::create(
			      _["prediction"] = getPrediction(pBridge, levelsTrain, ctgNames)
			      );
  }
  else {
    TestCtg testCtg(IntegerVector((SEXP) sYTest), levelsTrain);
    if (!pBridge->permutes()) {
      summaryCtg = List::create(
			      _["prediction"] = getPrediction(pBridge, levelsTrain, ctgNames),
			      _["validation"] = testCtg.getValidation(pBridge)
			      );
    }
    else {
      summaryCtg = List::create(
			      _["prediction"] = getPrediction(pBridge, levelsTrain, ctgNames),
			      _["validation"] = testCtg.getValidation(pBridge),
			      _["importance"] = testCtg.getImportance(pBridge, Signature::unwrapColNames(lDeframe))
			      );
    }
  }

  summaryCtg.attr("class") = "SummaryCtg";
  return summaryCtg;

  END_RCPP
}


List LeafCtgRf::getPrediction(const PredictCtgBridge* pBridge,
			      const CharacterVector& levelsTrain,
			      const CharacterVector& ctgNames) {
  BEGIN_RCPP
  auto yPred = pBridge->getYPred();
  IntegerVector yPredZero(yPred.begin(), yPred.end());
  IntegerVector yPredOne(yPredZero + 1);
  yPredOne.attr("class") = "factor";
  yPredOne.attr("levels") = levelsTrain;
  List prediction = List::create(
				 _["yPred"] = yPredOne,
				 _["census"] = getCensus(pBridge, levelsTrain, ctgNames),
				 _["prob"] = getProb(pBridge, levelsTrain, ctgNames)
				 );
  prediction.attr("class") = "PredictCtg";
  return prediction;

  END_RCPP
}


List TestCtg::getValidation(const PredictCtgBridge* pBridge) {
  BEGIN_RCPP
  List validCtg = List::create(
			       _["confusion"] = getConfusion(pBridge, levelsTrain),
			       _["misprediction"] = getMisprediction(pBridge),
			       _["oobError"] = pBridge->getOOBError()
			       );
  validCtg.attr("class") = "ValidCtg";
  return validCtg;
  
  END_RCPP
}


List TestCtg::getImportance(const PredictCtgBridge* pBridge,
			    const CharacterVector& predNames) {
  BEGIN_RCPP

  List importanceCtg = List::create(
				    _["mispred"] = mispredPermuted(pBridge, predNames),
				    _["oobErr"] = oobErrPermuted(pBridge, predNames)
				    );
  importanceCtg.attr("class") = "importanceCtg";
  return importanceCtg;
  
  END_RCPP
}


NumericVector TestCtg::getMisprediction(const PredictCtgBridge* pBridge) const {
  BEGIN_RCPP

  auto mispred = pBridge->getMisprediction();
  NumericVector mispredOut = as<NumericVector>(NumericVector(mispred.begin(), mispred.end())[test2Merged]);
  mispredOut.attr("names") = levels;
  return mispredOut;

  END_RCPP
}


NumericMatrix TestCtg::mispredPermuted(const PredictCtgBridge* pBridge,
				      const CharacterVector& predNames) const {
  BEGIN_RCPP

  auto mispredCore = pBridge->getMispredPermuted();
  NumericMatrix mispredOut(levels.length(), mispredCore.size());

  unsigned int col = 0;
  for (auto mispred : mispredCore) {
    mispredOut.column(col++) = as<NumericVector>(NumericVector(mispred.begin(), mispred.end())[test2Merged]);
  }
  mispredOut.attr("dimnames") = List::create(levels, predNames);
  
  return mispredOut;
  END_RCPP
}


NumericVector TestCtg::oobErrPermuted(const PredictCtgBridge* pBridge,
				     const CharacterVector& predNames) const {
  BEGIN_RCPP

  auto oobPerm = pBridge->getOOBErrorPermuted();
  NumericVector errOut(oobPerm.begin(), oobPerm.end());
  errOut.attr("names") = predNames;

  return errOut;
  END_RCPP
}


IntegerMatrix LeafCtgRf::getCensus(const PredictCtgBridge* pBridge,
                                   const CharacterVector& levelsTrain,
                                   const CharacterVector& ctgNames) {
  BEGIN_RCPP
  IntegerMatrix census = transpose(IntegerMatrix(levelsTrain.length(), pBridge->getNRow(), pBridge->getCensus()));
  census.attr("dimnames") = List::create(ctgNames, levelsTrain);
  return census;
  END_RCPP
}


NumericMatrix LeafCtgRf::getProb(const PredictCtgBridge* pBridge,
                                 const CharacterVector& levelsTrain,
                                 const CharacterVector& ctgNames) {
  BEGIN_RCPP
  if (!pBridge->getProb().empty()) {
    NumericMatrix prob = transpose(NumericMatrix(levelsTrain.length(), pBridge->getNRow(), &(pBridge->getProb())[0]));
    prob.attr("dimnames") = List::create(ctgNames, levelsTrain);
    return prob;
  }
  else {
    return NumericMatrix(0);
  }
  END_RCPP
}


NumericMatrix TestCtg::getConfusion(const PredictCtgBridge* pBridge,
				    const CharacterVector& levelsTrain) const {
  BEGIN_RCPP

  // Converts to numeric vector to accommodate wide rows in R.
  auto confusion = pBridge->getConfusion();
  NumericVector confNum(confusion.begin(), confusion.end());
  unsigned int ctgTrain = levelsTrain.length();
  unsigned int ctgTest = levels.length();
  NumericMatrix conf = transpose(NumericMatrix(ctgTrain, ctgTest, &confNum[0]));
  NumericMatrix confOut(ctgTest, ctgTrain);
  for (unsigned int i = 0; i < ctgTest; i++) {
    confOut(i, _) = conf(test2Merged[i], _);
  }
  confOut.attr("dimnames") = List::create(levels, levelsTrain);

  return confOut;
  END_RCPP
}
