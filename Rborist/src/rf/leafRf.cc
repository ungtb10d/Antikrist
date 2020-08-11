// Copyright (C)  2012-2020   Mark Seligman
//
// This file is part of rfR.
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
   @file leafRf.cc

   @brief C++ interface to R entry for Leaf methods.

   @author Mark Seligman
 */

#include "leafRf.h"
#include "leafbridge.h"
#include "trainbridge.h"
#include "predictbridge.h"
#include "signature.h"

bool LBTrain::thin = false;

LBTrain::LBTrain(unsigned int nTree) :
  nodeHeight(IntegerVector(nTree)),
  nodeRaw(RawVector(0)),
  bagHeight(IntegerVector(nTree)),
  blRaw(RawVector(0)) {
  fill(bagHeight.begin(), bagHeight.end(), 0ul);
}

void LBTrain::init(bool thin_) {
  thin = thin_;
}


void LBTrain::deInit() {
  thin = false;
}

LBTrainReg::LBTrainReg(const NumericVector& yTrain_,
                       unsigned int nTree) :
  LBTrain(nTree),
  yTrain(yTrain_) {
}

LBTrainCtg::LBTrainCtg(const IntegerVector& yTrain_,
                       unsigned int nTree) :
  LBTrain(nTree),
  weight(NumericVector(0)),
  weightSize(0),
  yTrain(yTrain_) {
}

void LBTrain::consume(const TrainChunk* train,
                      unsigned int tIdx,
                      double scale) {
  writeNode(train, tIdx, scale);
  writeBagSample(train, tIdx, scale);
}


void LBTrain::writeNode(const TrainChunk* train,
                        unsigned int tIdx,
                        double scale) {
  // Accumulates node heights.
  train->writeHeight((unsigned int*) &nodeHeight[0], tIdx);

  // Reallocates forest-wide buffer if estimated size insufficient.
  size_t nodeOff, nodeBytes;
  if (!train->leafFits((unsigned int*) &nodeHeight[0], tIdx, static_cast<size_t>(nodeRaw.length()), nodeOff, nodeBytes)) {
    nodeRaw = move(rawResize(&nodeRaw[0], nodeOff, nodeBytes, scale));
  }

  // Writes leaves as raw.
  train->dumpLeafRaw(&nodeRaw[nodeOff]);
}


RawVector LBTrain::rawResize(const unsigned char* raw, size_t offset, size_t bytes, double scale) {
  RawVector temp(scale * (offset + bytes));
  for (size_t i = 0; i < offset; i++)
    temp[i] = raw[i];

  return temp;
}


void LBTrain::writeBagSample(const TrainChunk* train,
                             unsigned int tIdx,
                             double scale) {
  // Thin leaves forgo writing bag state.
  if (thin)
    return;

  train->writeBagHeight((unsigned int*) &bagHeight[0], tIdx);

  // Writes BagSample records as raw.
  size_t blOff, bagBytes;
  if (!train->bagSampleFits((unsigned int*) &bagHeight[0], tIdx, static_cast<size_t>(blRaw.length()), blOff, bagBytes)) {
    blRaw = move(rawResize(&blRaw[0], blOff, bagBytes, scale));
  }
  train->dumpBagLeafRaw(&blRaw[blOff]);
}


void LBTrainReg::consume(const TrainChunk* train,
                         unsigned int tIdx,
                         double scale) {
  LBTrain::consume(train, tIdx, scale);
}


void LBTrainCtg::consume(const TrainChunk* train,
                         unsigned int tIdx,
                         double scale) {
  LBTrain::consume(train, tIdx, scale);
  writeWeight(train, scale);
}


void LBTrainCtg::writeWeight(const TrainChunk* train,
                             double scale) {
  auto sizeLoc = train->getWeightSize();
  if (weightSize + sizeLoc > static_cast<size_t>(weight.length())) {
    weight = move(numericResize(&weight[0], weightSize, sizeLoc, scale));
  }
  train->dumpLeafWeight(&weight[weightSize]);
  weightSize += sizeLoc;
}


NumericVector LBTrainCtg::numericResize(const double* num, size_t offset, size_t elts, double scale) {
  NumericVector temp(scale * (offset + elts));
  for (size_t i = 0; i < offset; i++) {
    temp[i] = num[i];
  }
  return temp;
}


/**
   @brief Wraps core (regression) Leaf vectors for reference by front end.
 */
List LBTrainReg::wrap() {
  BEGIN_RCPP
  List leaf =
    List::create(_["nodeHeight"] = move(nodeHeight),
                 _["node"] = move(nodeRaw),
                 _["bagHeight"] = move(bagHeight),
                 _["bagSample"] = move(blRaw),
                 _["yTrain"] = yTrain
  );
  leaf.attr("class") = "LeafReg";
  
  return leaf;
  END_RCPP
}


/**
   @brief Wraps core (classification) Leaf vectors for reference by front end.
 */
List LBTrainCtg::wrap() {
  BEGIN_RCPP
  List leaf =
    List::create(_["nodeHeight"] = move(nodeHeight),
                 _["node"] = move(nodeRaw),
                 _["bagHeight"] = move(bagHeight),
                 _["bagSample"] = move(blRaw),
                 _["weight"] = move(weight),
                 _["levels"] = as<CharacterVector>(yTrain.attr("levels"))
                 );
  leaf.attr("class") = "LeafCtg";

  return leaf;
  END_RCPP
}


/**
   @brief References front-end member arrays and instantiates
   bridge-specific LeafReg handle.
 */
unique_ptr<LeafRegBridge> LeafRegRf::unwrap(const List& lTrain,
					    const List& lDeframe) {
  List lLeaf(checkLeaf(lTrain));
  return make_unique<LeafRegBridge>((unsigned int*) IntegerVector((SEXP) lLeaf["nodeHeight"]).begin(),
                                    (size_t) IntegerVector((SEXP) lLeaf["nodeHeight"]).length(),
                                    (unsigned char*) RawVector((SEXP) lLeaf["node"]).begin(),
                                    (unsigned int*) IntegerVector((SEXP) lLeaf["bagHeight"]).begin(),
                                    (unsigned char*) RawVector((SEXP) lLeaf["bagSample"]).begin(),
                                    (double*) NumericVector((SEXP) lLeaf["yTrain"]).begin(),
                                    (size_t) NumericVector((SEXP) lLeaf["yTrain"]).length(),
                                    mean(NumericVector((SEXP) lLeaf["yTrain"])),
				    as<size_t>((SEXP) lDeframe["nRow"]));
}


List LeafRegRf::checkLeaf(const List &lTrain) {
  BEGIN_RCPP

  List lLeaf((SEXP) lTrain["leaf"]);
  if (!lLeaf.inherits("LeafReg")) {
    stop("Expecting LeafReg");
  }

  return lLeaf;

  END_RCPP
}



/**
   @brief References front-end vectors and instantiates bridge-specific
   LeafCtg handle.

   @return 
 */
unique_ptr<LeafCtgBridge> LeafCtgRf::unwrap(const List& lTrain,
					    const List& lDeframe,
                                            bool doProb) {
  List lLeaf(checkLeaf(lTrain));
  return make_unique<LeafCtgBridge>((unsigned int*) IntegerVector((SEXP) lLeaf["nodeHeight"]).begin(),
                                    (size_t) IntegerVector((SEXP) lLeaf["nodeHeight"]).length(),
                                    (unsigned char*) RawVector((SEXP) lLeaf["node"]).begin(),
                                    (unsigned int*) IntegerVector((SEXP) lLeaf["bagHeight"]).begin(),
                                    (unsigned char*) RawVector((SEXP) lLeaf["bagSample"]).begin(),
                                    (double*) NumericVector((SEXP) lLeaf["weight"]).begin(),
                                    (unsigned int) CharacterVector((SEXP) lLeaf["levels"]).length(),
                                    as<size_t>((SEXP) lDeframe["nRow"]),
                                    doProb);
}


/**
   @brief Ensures front end holds a LeafCtg.
 */
List LeafCtgRf::checkLeaf(const List &lTrain) {
  BEGIN_RCPP

  List leafCtg((SEXP) lTrain["leaf"]);
  if (!leafCtg.inherits("LeafCtg")) {
    stop("Expecting LeafCtg");
  }

  return leafCtg;

  END_RCPP
}


TestCtg::TestCtg(SEXP sYTest,
                 const CharacterVector &levelsTrain_) :
  levelsTrain(levelsTrain_),
  yTestOne(IntegerVector((SEXP) sYTest)),
  levels(CharacterVector((SEXP) yTestOne.attr("levels"))),
  nCtg(levels.length()),
  test2Merged(mergeLevels(levels, levelsTrain)),
  yTestZero(Reconcile(test2Merged, yTestOne)),
  ctgMerged(max(yTestZero) + 1) {
}


vector<unsigned int> TestCtg::buildConfusion(const LeafCtgBridge* leaf,
					     const vector<unsigned int>& yPred) const {
  vector<unsigned int> confusion(leaf->getRowPredict() * ctgMerged);
  for (unsigned int row = 0; row < leaf->getRowPredict(); row++) {
    confusion[leaf->ctgIdx(yTestZero[row], yPred[row])]++;
  }
  return confusion;
}


double TestCtg::oobError(const vector<unsigned int>& yPred) const {
  unsigned int missed = 0;
  for (unsigned int i = 0; i < yPred.size(); i++) {
    missed += static_cast<unsigned int>(yTestZero[i]) == yPred[i] ? 0 : 1;
  }

  return double(missed) / yPred.size();  // Caller precludes zero length.
}


List LeafRegRf::summary(SEXP sYTest, const PredictBridge* pBridge) {
  BEGIN_RCPP

  LeafRegBridge* leaf = static_cast<LeafRegBridge*>(pBridge->getLeaf());
  List prediction;
  if (Rf_isNull(sYTest)) {
    prediction = List::create(
                              _["yPred"] = leaf->getYPred(),
                              _["qPred"] = getQPred(leaf, pBridge),
                              _["qEst"] = getQEst(pBridge)
                              );
    prediction.attr("class") = "PredictReg";
  }
  else { // Validation/testing
    NumericVector yTest((SEXP) sYTest);
    double rsq, mae;
    double msePred = mse(leaf->getYPred(), yTest, rsq, mae);
    prediction = List::create(
                              _["yPred"] = leaf->getYPred(),
                              _["qPred"] = getQPred(leaf, pBridge),
                              _["qEst"] = getQEst(pBridge),
                              _["rsq"] = rsq,
                              _["mse"] = msePred,
                              _["mae"] = mae,
			      _["msePermuted"] = msePermute(leaf, yTest)
                              );
    prediction.attr("class") = "ValidReg";
  }

  return prediction;
  END_RCPP
}


NumericVector LeafRegRf::msePermute(const LeafRegBridge* leaf,
				    const NumericVector& yTest) {
  double rsq, mae;
  const vector<vector<double>>& yPermute = leaf->getYPermute();
  NumericVector importanceOut(yPermute.size());
  size_t i = 0;
  for (auto yPerm : yPermute) {
    importanceOut[i++] = mse(yPerm, yTest, rsq, mae);
  }

  return importanceOut;
}


double LeafRegRf::mse(const vector<double>& yPred,
		      const NumericVector& yTest,
		      double& rsq,
		      double& mae) {
  double sse = 0.0;
  mae = 0.0;
  IndexT rowPred = yTest.length();
  IndexT i = 0;
  for (auto yP : yPred) {
    double error = yP - yTest[i++];
    sse += error * error;
    mae += fabs(error);
  }
  rsq = 1.0 - sse / (var(yTest) * (rowPred - 1.0));
  mae /= rowPred;

  return sse / rowPred;
}


NumericMatrix LeafRegRf::getQPred(const LeafRegBridge* leaf,
                                  const PredictBridge* pBridge) {
  BEGIN_RCPP

  size_t nRow(leaf->getRowPredict());
  vector<double> qPred(pBridge->getQPred());
  return qPred.empty() ? NumericMatrix(0) : transpose(NumericMatrix(qPred.size() / nRow, nRow, qPred.begin()));
    
  END_RCPP
}


NumericVector LeafRegRf::getQEst(const PredictBridge* pBridge) {
  BEGIN_RCPP

  vector<double> qEst(pBridge->getQEst());
  return NumericVector(qEst.begin(), qEst.end());

  END_RCPP
}


List LeafCtgRf::summary(const List& lDeframe, const List& lTrain, const PredictBridge* pBridge, SEXP sYTest) {
  BEGIN_RCPP

  LeafCtgBridge* leaf = static_cast<LeafCtgBridge*>(pBridge->getLeaf());
  List lLeaf(checkLeaf(lTrain));
  CharacterVector levelsTrain((SEXP) lLeaf["levels"]);
  CharacterVector rowNames(Signature::unwrapRowNames(lDeframe));
  IntegerVector yPredZero(leaf->getYPred().begin(), leaf->getYPred().end());
  IntegerVector yPredOne(yPredZero + 1);
  yPredOne.attr("class") = "factor";
  yPredOne.attr("levels") = levelsTrain;
  List prediction;
  if (!Rf_isNull(sYTest)) {
    auto testCtg = make_unique<TestCtg>(sYTest, levelsTrain);
    prediction = List::create(
                              _["yPred"] = yPredOne,
                              _["census"] = getCensus(leaf, levelsTrain, rowNames),
                              _["prob"] = getProb(leaf, levelsTrain, rowNames),
                              _["confusion"] = testCtg->getConfusion(leaf, levelsTrain),
                              _["misprediction"] = testCtg->misprediction(leaf, leaf->getYPred()),
                              _["oobError"] = testCtg->oobError(leaf->getYPred()),
			      _["mispredPermuted"] = testCtg->mispredPermute(leaf),
			      _["oobErrPermuted"] = testCtg->oobErrPermute(leaf)
    );
    prediction.attr("class") = "ValidCtg";
  }
  else {
    prediction = List::create(
                      _["yPred"] = yPredOne,
                      _["census"] = getCensus(leaf, levelsTrain, rowNames),
                      _["prob"] = getProb(leaf, levelsTrain, rowNames)
   );
   prediction.attr("class") = "PredictCtg";
  }

  return prediction;
  END_RCPP
}


NumericVector TestCtg::misprediction(const LeafCtgBridge* leaf,
				     const vector<unsigned int>& yPred) const {
  BEGIN_RCPP
  NumericVector misPred(ctgMerged);
  vector<unsigned int> confusion = buildConfusion(leaf, yPred);
  for (unsigned int ctgRec = 0; ctgRec < ctgMerged; ctgRec++) {
    unsigned int numWrong = 0;
    unsigned int numRight = 0;
    for (unsigned int ctgPred = 0; ctgPred < leaf->getCtgTrain(); ctgPred++) {
      unsigned int numConf = confusion[leaf->ctgIdx(ctgRec, ctgPred)];
      if (ctgPred != ctgRec) {  // Misprediction iff off-diagonal.
        numWrong += numConf;
      }
      else {
        numRight = numConf;
      }
    }
    misPred[ctgRec] = numWrong + numRight == 0 ? 0.0 : double(numWrong) / double(numWrong + numRight);
  }

  NumericVector misPredOut = misPred[test2Merged];
  misPredOut.attr("names") = levels;
  return misPredOut;
  END_RCPP
}



NumericMatrix TestCtg::mispredPermute(const LeafCtgBridge* leaf) const {
  BEGIN_RCPP
  const vector<vector<unsigned int>>& yPermute = leaf->getYPermute();
  NumericMatrix importanceOut(ctgMerged, yPermute.size());

  size_t i = 0;
  for (auto yPerm : yPermute) {
    importanceOut(_, i++) = misprediction(leaf, yPerm);
  }

  return importanceOut;
  END_RCPP
}


NumericVector TestCtg::oobErrPermute(const LeafCtgBridge* leaf) const {
  BEGIN_RCPP
  NumericVector errOut(leaf->getYPermute().size());
  size_t i = 0;
  for (auto yPerm : leaf->getYPermute()) {
    errOut[i++] = oobError(yPerm);
  }

  return errOut;
  END_RCPP
}


IntegerMatrix LeafCtgRf::getCensus(const LeafCtgBridge* leaf,
                                   const CharacterVector& levelsTrain,
                                   const CharacterVector& rowNames) {
  BEGIN_RCPP
  IntegerMatrix census = transpose(IntegerMatrix(leaf->getCtgTrain(), leaf->getRowPredict(), leaf->getCensus()));
  census.attr("dimnames") = List::create(rowNames, levelsTrain);
  return census;
  END_RCPP
}


NumericMatrix LeafCtgRf::getProb(const LeafCtgBridge* leaf,
                                 const CharacterVector& levelsTrain,
                                 const CharacterVector& rowNames) {
  BEGIN_RCPP
  if (!leaf->getProb().empty()) {
    NumericMatrix prob = transpose(NumericMatrix(leaf->getCtgTrain(), leaf->getRowPredict(), &(leaf->getProb())[0]));
    prob.attr("dimnames") = List::create(rowNames, levelsTrain);
    return prob;
  }
  else {
    return NumericMatrix(0);
  }
  END_RCPP
}


IntegerVector TestCtg::mergeLevels(const CharacterVector &levelsTest,
                                   const CharacterVector &levelsTrain) {
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


IntegerVector TestCtg::Reconcile(const IntegerVector &test2Merged,
                                 const IntegerVector &yTestOne) {
  BEGIN_RCPP
  IntegerVector yZero = yTestOne -1;
  IntegerVector yZeroOut(yZero.length());
  for (R_len_t i = 0; i < yZero.length(); i++) {
    yZeroOut[i] = test2Merged[yZero[i]];
  }
  return yZeroOut;
  END_RCPP
}


IntegerMatrix TestCtg::getConfusion(const LeafCtgBridge* leaf,
				    const CharacterVector& levelsTrain) {
  BEGIN_RCPP
  vector<unsigned int> confusion = buildConfusion(leaf, leaf->getYPred());
  unsigned int ctgTrain = levelsTrain.length();
  IntegerMatrix conf = transpose(IntegerMatrix(ctgTrain, nCtg, &confusion[0]));
  IntegerMatrix confOut(nCtg, ctgTrain);
  for (unsigned int i = 0; i < nCtg; i++) {
    confOut(i, _) = conf(test2Merged[i], _);
  }
  confOut.attr("dimnames") = List::create(levels, levelsTrain);

  return confOut;
  END_RCPP
}
