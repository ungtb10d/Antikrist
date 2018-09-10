// Copyright (C)  2012-2018   Mark Seligman
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
   @file frameblockBridge.cc

   @brief C++ interface to R entries for maintaining predictor data structures.

   @author Mark Seligman
*/


#include "framemapBridge.h"
#include "blockBridge.h"

/**
  @brief Extracts contents of a data frame into numeric and (zero-based) factor blocks.  Can be quite slow for large predictor counts, as a linked list is being walked.

  @param sX is the raw data frame, with columns assumed to be either factor or numeric.

  @param sNRow is the number of rows.

  @param sNCol is the number of columns.

  @param sFacCol is the number of factor-valued columns.

  @param sNumCol is the number of numeric-valued columns.

  @param sLevel is a vector of level counts for each column.

  @return PredBlock containing predictor map signature and FrameBlock.
*/

// TODO:  MOve column and row names to signature.
RcppExport SEXP FrameMixed(SEXP sX,
                           SEXP sNumElt,
                           SEXP sFacElt,
                           SEXP sLevels,
                           SEXP sSigTrain) {
  BEGIN_RCPP
  DataFrame xf(sX);
  IntegerVector numElt = IntegerVector(sNumElt) - 1;
  IntegerVector facElt = IntegerVector(sFacElt) - 1;
  vector<unsigned int> levels = as<vector<unsigned int> >(sLevels);
  unsigned int nRow = xf.nrows();
  unsigned int nPredFac = facElt.length();
  unsigned int nPredNum = numElt.length();
  unsigned int nPred = nPredFac + nPredNum;

  IntegerVector predMap(nPred);
  IntegerVector facCard(0);
  IntegerMatrix xFac;
  NumericMatrix xNum;
  if (nPredNum > 0) {
    xNum = NumericMatrix(nRow, nPredNum);
  }
  else
    xNum = NumericMatrix(0, 0);
  if (nPredFac > 0) {
    facCard = IntegerVector(nPredFac);
    xFac = IntegerMatrix(nRow, nPredFac);
  }
  else {
    xFac = IntegerMatrix(0);
  }

  // Fills in matrix columns of numeric and integer components of frame.
  // 'predMap' maps core indices to front-end counterparts.
  int numIdx = 0;
  int facIdx = 0;
  List level(nPredFac);
  for (unsigned int feIdx = 0; feIdx < nPred; feIdx++) {
    unsigned int card = levels[feIdx];
    if (card == 0) {
      xNum(_, numIdx) = as<NumericVector>(xf[feIdx]);
      predMap[numIdx++] = feIdx;
    }
    else {
      facCard[facIdx] = card;
      level[facIdx] = as<CharacterVector>(as<IntegerVector>(xf[feIdx]).attr("levels"));
      xFac(_, facIdx) = as<IntegerVector>(xf[feIdx]) - 1;
      predMap[nPredNum + facIdx++] = feIdx;
    }
  }

  // Factor positions must match those from training and values must conform.
  //
  if (!Rf_isNull(sSigTrain) && nPredFac > 0) {
    List sigTrain(sSigTrain);
    IntegerVector predTrain(as<IntegerVector>(sigTrain["predMap"]));
    if (!is_true(all(predMap == predTrain))) {
      stop("Training, prediction data types do not match");
    }

    List levelTrain(as<List>(sigTrain["level"]));
    FramemapBridge::FactorRemap(xFac, level, levelTrain);
  }

  
  List predBlock = List::create(
      _["blockNum"] = xNum,
      _["nPredNum"] = nPredNum,
      _["blockNumSparse"] = List(), // For now.
      _["blockFacSparse"] = R_NilValue, // For now.
      _["blockFac"] = xFac,
      _["nPredFac"] = nPredFac,
      _["nRow"] = nRow,
      _["facCard"] = facCard,
      _["signature"] = move(FramemapBridge::wrapSignature(predMap,
                                                          level,
                                                          colnames(xf),
                                                          rownames(xf)))
      );
  predBlock.attr("class") = "PredBlock";

  return predBlock;
  END_RCPP
}

  // Signature contains front-end decorations not exposed to the
  // core.
// Column and row names stubbed to zero-length vectors if null.
List FramemapBridge::wrapSignature(const IntegerVector &predMap,
               const List &level,
               const CharacterVector &colNames,
               const CharacterVector &rowNames) {
  List signature = List::create(
                                _["predMap"] = predMap,
                                _["level"] = level,
                                _["colNames"] = Rf_isNull(colNames) ? CharacterVector(0) : colNames,
                                _["rowNames"] = Rf_isNull(rowNames) ? CharacterVector(0) : rowNames
        );
  signature.attr("class") = "Signature";

  return signature;
}

void FramemapBridge::FactorRemap(IntegerMatrix &xFac, List &levelTest, List &levelTrain) {
  for (int col = 0; col < xFac.ncol(); col++) {
    CharacterVector colTest(as<CharacterVector>(levelTest[col]));
    CharacterVector colTrain(as<CharacterVector>(levelTrain[col]));
    if (is_true(any(colTest != colTrain))) {
      IntegerVector colMatch = match(colTest, colTrain);
      IntegerVector sq = seq(0, colTest.length() - 1);
      IntegerVector idxNonMatch = sq[is_na(colMatch)];
      if (idxNonMatch.length() > 0) {
        warning("Factor levels not observed in training:  employing proxy");
        int proxy = colTrain.length() + 1;
        colMatch[idxNonMatch] = proxy;
      }

      colMatch = colMatch - 1;  // match() is one-based.
      IntegerMatrix::Column xCol = xFac(_, col);
      IntegerVector colT(xCol);
      IntegerVector colRemap = colMatch[colT];
      xFac(_, col) = colRemap;
    }
  }
}


RcppExport SEXP FrameNum(SEXP sX) {
  NumericMatrix blockNum(as<NumericMatrix>(sX));
  int nPred = blockNum.ncol();
  List dimnames = blockNum.attr("dimnames");

  IntegerVector facCard(0);
  List predBlock = List::create(
        _["blockNum"] = blockNum,
        _["blockNumSparse"] = List(), // For now.
        _["blockFacSparse"] = R_NilValue, // For now.
        _["nPredNum"] = nPred,
        _["blockFac"] = IntegerMatrix(0),
        _["nPredFac"] = 0,
        _["nRow"] = blockNum.nrow(),
        _["facCard"] = facCard,
        _["signature"] = move(FramemapBridge::wrapSignature(seq_len(nPred) - 1,
                                        List::create(0),
                                        colnames(blockNum),
                                        rownames(blockNum)))
                                );
  predBlock.attr("class") = "PredBlock";

  return predBlock;
}

// TODO:  Move column and row names to signature.
/**
   @brief Reads an S4 object containing (sparse) dgCMatrix.
 */
RcppExport SEXP FrameSparse(SEXP sX) {
  BEGIN_RCPP
  S4 spNum(sX);

  IntegerVector i;
  if (R_has_slot(sX, Rf_mkString("i"))) {
    i = spNum.slot("i");
  }
  IntegerVector j;
  if (R_has_slot(sX, Rf_mkString("j"))) {
    j = spNum.slot("j");
  }
  IntegerVector p;
  if (R_has_slot(sX, Rf_mkString("p"))) {
    p = spNum.slot("p");
  }

  if (!R_has_slot(sX, Rf_mkString("Dim"))) {
    stop("Expecting dimension slot");
  }


  IntegerVector dim = spNum.slot("Dim");
  unsigned int nRow = dim[0];
  unsigned int nPred = dim[1];

  // 'eltsNZ' holds the nonzero elements.
  NumericVector eltsNZ;

  if (R_has_slot(sX, Rf_mkString("x"))) {
    eltsNZ = spNum.slot("x");
  }
  else {
    stop("Pattern matrix:  NYI");
  }

  vector<double> valNum;
  vector<unsigned int> rowStart;
  vector<unsigned int> runLength;

  vector<unsigned int> predStart(nPred);

  // Divines the encoding format and packs appropriately.
  //
  if (i.length() == 0) {
    FramemapBridge::SparseJP(eltsNZ, j, p, nRow, valNum, rowStart, runLength);
  }
  else if (j.length() == 0) {
    FramemapBridge::SparseIP(eltsNZ, i, p, nRow, valNum, rowStart, runLength, predStart);
  }
  else if (p.length() == 0) {
    FramemapBridge::SparseIJ(eltsNZ, i, j, nRow, valNum, rowStart, runLength);
  }
  else {
    stop("Indeterminate sparse matrix format");
  }

  List blockNumSparse = List::create(
          _["valNum"] = valNum,
          _["rowStart"] = rowStart,
          _["runLength"] = runLength,
          _["predStart"] = predStart);
  blockNumSparse.attr("class") = "BlockNumSparse";

  List dimNames;
  CharacterVector rowName = CharacterVector(0);
  CharacterVector colName = CharacterVector(0);
  if (R_has_slot(sX, Rf_mkString("Dimnames"))) {
    dimNames = spNum.slot("Dimnames");
    if (!Rf_isNull(dimNames[0])) {
      rowName = dimNames[0];
    }
    if (!Rf_isNull(dimNames[1])) {
      colName = dimNames[1];
    }
  }

  IntegerVector facCard(0);
  List predBlock = List::create(
        _["blockNum"] = NumericMatrix(0),
        _["nPredNum"] = nPred,
        _["blockNumSparse"] = move(blockNumSparse),
        _["blockFacSparse"] = R_NilValue, // For now.
        _["blockFac"] = IntegerMatrix(0),
        _["nPredFac"] = 0,
        _["nRow"] = nRow,
        _["facCard"] = facCard,
        _["signature"] = move(FramemapBridge::wrapSignature(seq_len(nPred) -1,
                                        List::create(0),
                                        colName,
                                        rowName))
                                );

  predBlock.attr("class") = "PredBlock";

  return predBlock;
  END_RCPP
}


// 'i' in [0, nRow-1] list rows with nonzero elements.
// 'p' holds the starting offset for each column in 'eltsNZ'.
//    Repeated values indicate full-zero columns. 
//
void FramemapBridge::SparseIP(const NumericVector &eltsNZ,
                                const IntegerVector &i,
                                const IntegerVector &p,
                                unsigned int nRow,
                                vector<double> &valNum,
                                vector<unsigned int> &rowStart,
                                vector<unsigned int> &runLength,
                                vector<unsigned int> &predStart) {
  // Pre-scans column heights. 'p' has length one greater than number
  // of columns, providing ready access to heights.
  const double zero = 0.0;
  vector<unsigned int> nzHeight(p.length());
  unsigned int idxStart = p[0];
  for (R_len_t colIdx = 1; colIdx < p.length(); colIdx++) {
    nzHeight[colIdx - 1] = p[colIdx] - idxStart;
    idxStart = p[colIdx];
  }

  // Replace triple entries with RLEPush( , , ):
  for (unsigned int colIdx = 0; colIdx < predStart.size(); colIdx++) {
    unsigned int height = nzHeight[colIdx];
    predStart[colIdx] = valNum.size();
    if (height == 0) {
      valNum.push_back(zero);
      runLength.push_back(nRow);
      rowStart.push_back(0);
    }
    else {
      unsigned int nzPrev = nRow; // Inattainable row value.
      // Row indices into 'i' and 'x' are zero-based.
      unsigned int idxStart = p[colIdx];
      unsigned int idxEnd = idxStart + height;
      for (unsigned int rowIdx = idxStart; rowIdx < idxEnd; rowIdx++) {
        unsigned int nzRow = i[rowIdx];
        if (nzPrev == nRow && nzRow > 0) { // Zeroes lead.
          valNum.push_back(zero);
          runLength.push_back(nzRow);
          rowStart.push_back(0);
        }
        else if (nzRow > nzPrev + 1) { // Zeroes precede.
          valNum.push_back(zero);
          runLength.push_back(nzRow - (nzPrev + 1));
          rowStart.push_back(nzPrev + 1);
        }
        valNum.push_back(eltsNZ[rowIdx]);
        runLength.push_back(1);
        rowStart.push_back(nzRow);
        nzPrev = nzRow;
      }
      if (nzPrev + 1 < nRow) { // Zeroes trail.
        valNum.push_back(zero);
        runLength.push_back(nRow - nzPrev - 1);
        rowStart.push_back(nzPrev + 1);
      }
    }
  }
}



SEXP FramemapBridge::SparseJP(NumericVector &eltsNZ,
                                IntegerVector &j,
                                IntegerVector &p,
                                unsigned int nRow,
                                vector<double> &valNum,
                                vector<unsigned int> &rowStart,
                                vector<unsigned int> &runLength) {
  BEGIN_RCPP
  stop("Sparse form j/p:  NYI");
  END_RCPP
}


SEXP FramemapBridge::SparseIJ(NumericVector &eltsNZ,
                                IntegerVector &i,
                                IntegerVector &j,
                                unsigned int nRow,
                                vector<double> &valNum,
                                vector<unsigned int> &rowStart,
                                vector<unsigned int> &runLength) {
  BEGIN_RCPP
  stop("Sparse form i/j:  NYI");
  END_RCPP
}


/**
   @brief Unwraps field values useful for prediction.
 */
List FramemapBridge::unwrapSignature(const List& sPredBlock) {
  BEGIN_RCPP
  PredblockLegal(sPredBlock);
  List signature = as<List>(sPredBlock["signature"]);
  SignatureLegal(signature);

  return signature;
  END_RCPP
}


SEXP FramemapBridge::PredblockLegal(const List &predBlock) {
  BEGIN_RCPP
  if (!predBlock.inherits("PredBlock")) {
    stop("Expecting PredBlock");
  }

  if (!Rf_isNull(predBlock["blockFacSparse"])) {
    stop ("Sparse factors:  NYI");
  }
  END_RCPP
}


/**
   @brief Unwraps field values useful for export.
 */
void FramemapBridge::SignatureUnwrap(const List& sTrain, IntegerVector &_predMap, List &_level) {
  List sSignature((SEXP) sTrain["signature"]);
  SignatureLegal(sSignature);

  _predMap = as<IntegerVector>((SEXP) sSignature["predMap"]);
  _level = as<List>(sSignature["level"]);
}


SEXP FramemapBridge::SignatureLegal(const List &signature) {
  BEGIN_RCPP
  if (!signature.inherits("Signature")) {
    stop("Expecting Signature");
  }
  END_RCPP
}


unique_ptr<FrameTrain> FramemapBridge::FactoryTrain(
                    const vector<unsigned int> &facCard,
                    unsigned int nPred,
                    unsigned int nRow) {
  return make_unique<FrameTrain>(facCard, nPred, nRow);
}


unique_ptr<FramePredictBridge> FramemapBridge::FactoryPredict(const List& sPredBlock) {
  unwrap(sPredBlock);
  return make_unique<FramePredictBridge>(
                 move(BlockNumBridge::Factory(sPredBlock)),
                 move(BlockFacBridge::Factory(sPredBlock)),
                 as<unsigned int>(sPredBlock["nRow"]));
}


/**
   @brief Unwraps field values useful for prediction.
 */
SEXP FramemapBridge::unwrap(const List &sPredBlock) {
  BEGIN_RCPP
  PredblockLegal(sPredBlock);
  END_RCPP
}


FramePredictBridge::FramePredictBridge(
               unique_ptr<BlockNumBridge> _blockNum,
               unique_ptr<BlockFacBridge> _blockFac,
               unsigned int nRow) :
  blockNum(move(_blockNum)),
  blockFac(move(_blockFac)) {
  framePredict = move(make_unique<FramePredict>(blockNum->Num(),
                                                blockFac->Fac(),
                                                nRow));
}