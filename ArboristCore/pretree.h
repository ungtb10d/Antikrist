// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file pretree.h

   @brief Class defintions for the pre-tree, a serial and minimal representation from which the decision tree is built.

   @author Mark Seligman

 */

#ifndef ARBORIST_PRETREE_H
#define ARBORIST_PRETREE_H


/**
 @brief Serialized representation of the pre-tree, suitable for tranfer between
 devices such as coprocessors, disks and nodes.

 Left and right subnodes are referenced as indices into the vector
 representation of the tree. Leaves are distinguished as having two
 negative-valued subnode indices, while splits have both subset
 indices positive.  Mixed negative and non-negative subnode indices
 indicate an error.
*/
class PTNode {
 public:
  int id;
  int lhId;  // LH subnode index. Non-negative iff non-terminal.
  int predIdx; // Split only.
  double splitVal; // Split only.
  void SplitConsume(int &pred, double &num, int &bump);
};


class PreTree {
  static int nPred;
  static unsigned int heightEst;
  PTNode *nodeVec; // Vector of tree nodes.
  int nodeCount; // Allocation height of node vector.
  int bitLength; // Length of bit vector recording factor-valued splits.
  int treeHeight;
  int leafCount;
  int treeBitOffset;
  int *sample2PT; // Public accessor is Sample2Frontier().
  double *info; // Aggregates info value of nonterminals, by predictor.
  bool *treeSplitBits;
  inline bool *BitFactory(int bitLength = 0);

  void Consume(int nodeVal[], double numVec[], int bumpVec[], double _predInfo[], int &facWidth, int *&facSplits);
  void ConsumeSplitBits(int outBits[]);
 protected:
  int bagCount;
  /**
     @brief Indicates whether node is terminal.

     @param idx is the node index.

     @retrun true iff node is nonterminal.
   */
  bool IsNT(int ptIdx) {
    return nodeVec[ptIdx].lhId > 0;
  }

 public:
  PreTree(int _bagCount);
  ~PreTree();
  static void Immutables(unsigned int _nPred, unsigned int _nSamp, unsigned int _minH);
  static void DeImmutables();
  static void RefineHeight(unsigned int height);

  class SplitPred *BagRows(const class PredOrd *predOrd, int &_bagCount, double &_sum);

  void DecTree(int *predTree, double *splitTree, int *bumpTree, unsigned int *facBits, double predInfo[]);
  void SplitConsume(int nodeVal[], double numVec[], int bumpVec[]);
  void BitConsume(unsigned int *outBits);
  
  /**
     @return offset into the split-value bit vector for the current level.
   */
  int BitWidth() {
    return treeBitOffset;
  }


  /**
     @return true iff bit at position 'pos' is set.
   */
  bool BitVal(int pos) {
    return treeSplitBits[pos];
  }

  /**
   @brief Maps sample index to index of frontier node with which it is currently associated.
 
   @param sIdx is the index of a sample

   @return pretree index.
  */
  inline int Sample2Frontier(int sIdx) const {
    return sample2PT[sIdx];
  }


  /**
     @brief Accessor for sample map.

     @return sample map.
   */
  inline int* FrontierMap() {
    return sample2PT;
  }


  inline int Height() const {
    return treeHeight;
  }

  
  inline int BagCount() const {
    return bagCount;
  }

  
  void TerminalOffspring(int _parId, int &ptLH, int &ptRH);
  
  /**
     @brief Sets specified bit value to true.  Assumes initialized false.

     @param pos is the bit position beyond to set.

     @return void.
  */
  inline void LHBit(int pos) {
    treeSplitBits[treeBitOffset + pos] = true;
  }

  void NonTerminal(int _id, double _info, double _splitVal, int _predIdx);


  /**
     @brief Post-increments bit offset value.

     @param bump is the increment amount.

     @return cached bit offset value.
   */
  int PostBump(int bump) {
    int preVal = treeBitOffset;
    treeBitOffset += bump;

    return preVal;
  }

  double Replay(class SamplePred *samplePred, int predIdx, int level, int start, int end, int ptId);
  
  void CheckStorage(int splitNext, int leafNext);
  void ReBits();
  void ReNodes();
  double FacBits(const bool facBits[], int facWidth);
  void ConsumeNodes(int predVec[], double splitVec[], int bumpVec[]);
};

#endif
