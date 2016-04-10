// Copyright (C)  2012-2016  Mark Seligman
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
   @file rcppLeaf.h

   @brief C++ class definitions for managing Leaf object.

   @author Mark Seligman

 */


#ifndef ARBORIST_RCPP_LEAF_H
#define ARBORIST_RCPP_LEAF_H

#include <vector>
#include <Rcpp.h>
using namespace Rcpp;

RcppExport SEXP LeafWrapReg(const std::vector<unsigned int> &leafOrigin, const std::vector<class LeafNode> &leafNode, const std::vector<class RankCount> &leafInfo, const std::vector<double> &yRanked);
RcppExport SEXP LeafWrapCtg(const std::vector<unsigned int> &leafOrigin, const std::vector<LeafNode> &leafNode, const std::vector<double> &leafInfo, const CharacterVector &levels);
void LeafUnwrapReg(SEXP sLeaf, std::vector<double> &_yRanked, std::vector<unsigned int> &_leafOrigin, std::vector<class LeafNode> *&_leafNode, std::vector<class RankCount> *&_leafInfo);
void LeafUnwrapCtg(SEXP sLeaf, std::vector<unsigned int> &_leafOrigin, std::vector<class LeafNode> *&_leafNode, std::vector<double> &_leafCtgInfo, CharacterVector &_levels);

#endif