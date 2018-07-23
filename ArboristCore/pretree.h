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

#include <vector>
#include <algorithm>

#include "decnode.h"


/**
  @brief DecNode specialized for training.
 */
class PTNode : public DecNode {
  FltVal info;  // Nonzero iff nonterminal.
 public:
  
  void NonterminalConsume(const class FrameTrain *frameTrain, class ForestTrain *forest, unsigned int tIdx, vector<double> &predInfo, unsigned int idx) const;

  void splitNum(const class SplitCand &cand,
                unsigned int lhDel);

  /**
     @brief Resets to default terminal status.

     @return void.
   */
  inline void setTerminal() {
    lhDel = 0;
  }


  /**
     @brief Resets to nonterminal with specified lh-delta.

     @return void.
   */
  inline void setNonterminal(unsigned int lhDel) {
    this->lhDel = lhDel;
  }

  
  inline bool NonTerminal() const {
    return lhDel != 0;
  }


  inline unsigned int LHId(unsigned int ptId) const {
    return NonTerminal() ? ptId + lhDel : 0;
  }

  inline unsigned int RHId(unsigned int ptId) const {
    return NonTerminal() ? LHId(ptId) + 1 : 0;
  }


  inline void SplitFac(unsigned int predIdx, unsigned int lhDel, unsigned int bitEnd, double info) {
    this->predIdx = predIdx;
    this->lhDel = lhDel;
    this->splitVal.offset = bitEnd;
    this->info = info;
  }
};


/**
 @brief Serialized representation of the pre-tree, suitable for tranfer between
 devices such as coprocessors, disks and nodes.
*/
class PreTree {
  static unsigned int heightEst;
  static unsigned int leafMax; // User option:  maximum # leaves, if > 0.
  const class FrameTrain *frameTrain;
  PTNode *nodeVec; // Vector of tree nodes.
  unsigned int nodeCount; // Allocation height of node vector.
  unsigned int height;
  unsigned int leafCount;
  unsigned int bitEnd; // Next free slot in factor bit vector.
  class BV *splitBits;
  vector<unsigned int> termST;
  class BV *BitFactory();
  const vector<unsigned int> FrontierConsume(class ForestTrain *forest, unsigned int tIdx) const ;
  const unsigned int bagCount;
  unsigned int BitWidth();


  /**
     @brief Accounts for the addition of two terminals to the tree.

     @return void, with incremented height and leaf count.
  */
  inline void TerminalOffspring() {
  // Two more leaves for offspring, one fewer for this.
    height += 2;
    leafCount++;
  }


 public:
  PreTree(const class FrameTrain *_frameTrain, unsigned int _bagCount);
  ~PreTree();
  static void Immutables(unsigned int _nSamp, unsigned int _minH, unsigned int _leafMax);
  static void DeImmutables();
  static void Reserve(unsigned int height);

  const vector<unsigned int> Consume(class ForestTrain *forest, unsigned int tIdx, vector<double> &predInfo);
  void NonterminalConsume(class ForestTrain *forest, unsigned int tIdx, vector<double> &predInfo) const;
  void BitConsume(unsigned int *outBits);
  void LHBit(int idx, unsigned int pos);

  void branchFac(const class SplitCand& argMax,
                 unsigned int _id);

  /**
     @brief Finalizes numeric-valued nonterminal.

     @param argMax is the split candidate characterizing the branch.

     @param id is the node index.
  */
  void branchNum(const class SplitCand &argMax,
                 unsigned int id);

  void Level(unsigned int splitNext, unsigned int leafNext);
  void ReNodes();
  void SubtreeFrontier(const vector<unsigned int> &stTerm);
  unsigned int LeafMerge();
  
  inline unsigned int LHId(unsigned int ptId) const {
    return nodeVec[ptId].LHId(ptId);
  }

  
  inline unsigned int RHId(unsigned int ptId) const {
    return nodeVec[ptId].RHId(ptId);
  }

  
  /**
     @return true iff node is nonterminal.
   */
  inline bool NonTerminal(unsigned int ptId) const {
    return nodeVec[ptId].NonTerminal();
  }


    /**
       @brief Determines whether a nonterminal can be merged with its
       children.

       @param ptId is the index of a nonterminal.

       @return true iff node has two leaf children.
    */
  inline bool Mergeable(unsigned int ptId) const {
    return !NonTerminal(LHId(ptId)) && !NonTerminal(RHId(ptId));
  }  

  
  /**
     @brief Fills in references to values known to be useful for building
     a block of PreTree objects.

     @return void.
   */
  inline void BlockBump(unsigned int &_height, unsigned int &_maxHeight, unsigned int &_bitWidth, unsigned int &_leafCount, unsigned int &_bagCount) {
    _height += height;
    _maxHeight = max(height, _maxHeight);
    _bitWidth += BitWidth();
    _leafCount += leafCount;
    _bagCount += bagCount;
  }
};

#endif
