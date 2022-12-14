// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file forestrw.cc

   @brief Core-specific packing/unpacking of external Forest representations.

   @author Mark Seligman
 */


#include "forestrw.h"
#include "forest.h"


void ForestRW::dump(const Forest* forest,
		    vector<vector<unsigned int> >& predTree,
		    vector<vector<double> >& splitTree,
		    vector<vector<double> >& lhDelTree,
		    vector<vector<unsigned char> >& facSplitTree) {
  IndexT fsDummy;
  vector<vector<IndexT>> delDummy;
  vector<vector<PredictorT>> predDummy;
  forest->dump(predDummy, splitTree, delDummy, fsDummy);
}
