// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef ARBORIST_SPLITPRED_H
#define ARBORIST_SPLITPRED_H

/**
   @file splitpred.h

   @brief Class definitions for the four types of predictor splitting:  {regression, categorical} x {numerical, factor}.

   @author Mark Seligman

 */

/**
   @brief Pair-based splitting information.
 */
class SPPair {
  int splitIdx;
  int predIdx;
  int setIdx; // Nonnegative iff nontrivial run.
 public:
  inline void Coords(int &_splitIdx, int &_predIdx) const {
    _splitIdx = splitIdx;
    _predIdx = predIdx;
  }

  inline void SetCoords(int _splitIdx, int _predIdx) {
    splitIdx = _splitIdx;
    predIdx = _predIdx;
  }
  
  inline int SplitIdx() const {
    return splitIdx;
  }

  /**
    @brief Looks up associated pair-run index.

    @return referece to index in PairRun vector.
   */
  inline void SetRSet(int idx) {
    setIdx = idx;
  }

  inline int RSet() const {
    return setIdx;
  }

  void Split(class SplitPred *splitPred, const class IndexNode indexNode[], class SPNode *nodeBase,  class SplitSig *splitSig);
};


/**
   @brief Per-predictor splitting facilities.
 */
// Predictor-specific implementation of node.
// Currently available in four flavours depending on response type of node and data
// type of predictor:  { regression, categorical } x { numeric, factor }.
//
class SplitPred {
  int pairCount;
  SPPair *spPair;
  void SplitFlags();
  void SplitPredProb(const double ruPred[], bool splitFlags[]);
  void SplitPredFixed(int predFix, const double ruPred[], class BHPair heap[], bool splitFlags[]);
  void LevelSplit(const class IndexNode _indexNode[], class SPNode *nodeBase, int splitCount);
  SPPair *PairInit(int &pairCount);
 protected:
  static int nPred;
  static int nPredNum;
  static int predNumFirst;
  static int nPredFac;
  int splitCount;
  
  class Run *run;
  bool *splitFlags;
  static void Immutables();
 public:
  const class SamplePred *samplePred;
  SplitPred(class SamplePred *_samplePred);
  static SplitPred *FactoryReg(class SamplePred *_samplePred);
  static SplitPred *FactoryCtg(class SamplePred *_samplePred, class SampleNodeCtg *_sampleCtg);
  static void ImmutablesCtg(unsigned int _nRow, int _nSamp, unsigned int _ctgWidth);
  static void ImmutablesReg(unsigned int _nRow, int _nSamp);
  static void DeImmutables();

  void LevelInit(class Index *index, int splitCount);
  void LevelSplit(const class IndexNode indexNode[], int level, int splitCount, class SplitSig *splitSig);
  void LevelSplit(const class IndexNode indexNode[], class SPNode *nodeBase, int splitCount, class SplitSig *splitSig);  
  void LengthTransmit(int splitIdx, int lNext, int rNext);
  unsigned int &LengthNext(int splitNext, int predIdx);
  void LengthVec(int splitNext);
  inline void SplitFields(int pairIdx, int &_splitIdx, int &_predIdx) {
    spPair[pairIdx].Coords(_splitIdx, _predIdx);
  }

  void RLNext(int splitIdx, int lNext, int rNext);
  unsigned int &RLNext(int splitIdx, int predIdx);
  bool Singleton(int splitIdx, int predIdx);
  void Split(const class IndexNode indexNode[], class SPNode *nodeBase, class SplitSig *splitSig);

  class Run *Runs() {
    return run;
  }
  
  virtual ~SplitPred();
  virtual void RunOffsets() = 0;
  virtual void LevelPreset(const class Index *index) = 0;
  virtual double Prebias(int splitIdx, int sCount, double sum) = 0;
  virtual void LevelClear();

  virtual void SplitNum(const SPPair *spPair, const class IndexNode indexNode[], const class SPNode spn[], class SplitSig *splitSig) = 0;
  virtual void SplitFac(const SPPair *spPair, const class IndexNode indexNode[], const class SPNode spn[], class SplitSig *splitSig) = 0;
};


/**
   @brief Splitting facilities specific regression trees.
 */
class SPReg : public SplitPred {
  ~SPReg();
  void SplitHeap(const class IndexNode *indexNode, const class SPNode spn[], int predIdx, class SplitSig *splitSig);
  void Split(const class IndexNode indexNode[], class SPNode *nodeBase, class SplitSig *splitSig);
  void SplitNum(const SPPair *spPair, const class IndexNode indexNode[], const class SPNode spn[], class SplitSig *splitSig);
  void SplitNumWV(const SPPair *spPair, const class IndexNode *indexNode, const class SPNode spn[], class SplitSig *splitSig);
  void SplitFac(const SPPair *spPair, const class IndexNode indexNode[], const class SPNode *nodeBase, class SplitSig *splitSig);
  void SplitFacWV(const SPPair *spPair, const class IndexNode *indexNode, const class SPNode spn[], class SplitSig *splitSig);
  unsigned int BuildRuns(class RunSet *runSet, const class SPNode spn[], int start, int end);
  int HeapSplit(class RunSet *runSet, double sum, int &lhSampCt, double &maxGini);

 public:
  SPReg(class SamplePred *_samplePred);
  static void Immutables(unsigned int _nRow, int _nSamp);
  static void DeImmutables();
  void RunOffsets();
  void LevelPreset(const class Index *index);
  double Prebias(int spiltIdx, int sCount, double sum);
  void LevelClear();
};


/**
   @brief Splitting facilities for categorical trees.
 */
class SPCtg : public SplitPred {
  double *ctgSum; // Per-level sum, by split/category pair.
  double *ctgSumR; // Numeric predictors:  sum to right.
  double *sumSquares; // Per-level sum of squares, by split.
// Numerical tolerances taken from A. Liaw's code:
  static const double minDenom = 1.0e-5;
  static const double minSumL = 1.0e-8;
  static const double minSumR = 1.0e-5;
  const class SampleNodeCtg *sampleCtg;
  void LevelPreset(const class Index *index);
  double Prebias(int splitIdx, int sCount, double sum);
  void LevelClear();
  void Split(const class IndexNode indexNode[], class SPNode *nodeBase, class SplitSig *splitSig);
  void RunOffsets();
  void SumsAndSquares(const class Index *index);
  int LHBits(unsigned int lhBits, int pairOffset, unsigned int depth, int &lhSampCt);


  /**
     @brief Looks up node values by category.

     @param splitIdx is the split index.

     @param ctg is the category.

     @return Sum of index node values at split index, category.
   */
  inline double CtgSum(int splitIdx, unsigned int ctg) {
    return ctgSum[splitIdx * ctgWidth + ctg];
  }
  void LevelInitSumR();
  void SplitNum(const SPPair *spPair, const class IndexNode indexNode[], const class SPNode spn[], class SplitSig *splitSig);
  void SplitNumGini(const SPPair *spPair, const class IndexNode *indexNode, const class SPNode spn[], class SplitSig *splitSig);
  int SplitBinary(class RunSet *runSet, int splitIdx, double sum, double &maxGini, int &lhSampCt);  
  unsigned int BuildRuns(class RunSet *runSet, const class SPNode spn[], int start, int end);
  int SplitRuns(class RunSet *runSet, int splitIdx, double sum, double &maxGini, int &lhSampCt);
  
 public:
  static unsigned int ctgWidth; // Response cardinality:  immutable.
  static void Immutables(unsigned int _nRow, int _nSamp, unsigned int _ctgWidth);
  static void DeImmutables();
  SPCtg(class SamplePred *_samplePred, class SampleNodeCtg _sampleCtg[]);
  ~SPCtg();

  /**
     @brief Records sum of proxy values at 'yCtg' strictly to the right and updates the
     subaccumulator by the current proxy value.

     @param predIdx is the predictor index.  Assumes numerical predictors contiguous.  

     @param splitIdx is the split index.

     @param yCtg is the categorical response value.

     @param yVal is the proxy response value.

     @return recorded sum.
  */
  inline double CtgSumRight(int splitIdx, int predIdx, unsigned int yCtg, double yVal) {
    int off = (predIdx - predNumFirst) * splitCount * ctgWidth + splitIdx * ctgWidth + yCtg;
    double val = ctgSumR[off];
    ctgSumR[off] = val + yVal;

    return val;
  }

 public:
  void SplitFac(const SPPair *spPair, const class IndexNode indexNode[], const class SPNode spn[], class SplitSig *splitSig);
  void SplitFacGini(const SPPair *spPair, const class IndexNode *indexNode, const class SPNode spn[], class SplitSig *splitSig);
};


#endif
