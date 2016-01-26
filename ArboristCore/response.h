// This file is part of ArboristCore.

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
   @file response.h

   @brief Class definitions for representing response-specific aspects of training, especially regression versus categorical support.

   @author Mark Seligman

 */


#ifndef ARBORIST_RESPONSE_H
#define ARBORIST_RESPONSE_H

/**
   @brief Methods and members for management of response-related computations.
 */
class Response {
 protected:
  const double *y;
 public:
  Response(const double _y[]);
  static class ResponseReg *FactoryReg(const double yNum[], double yRanked[], unsigned int _nRow);
  static class ResponseCtg *FactoryCtg(const int feCtg[], const double feProxy[], unsigned int _nRow);
  
  virtual ~Response(){}
};

/**
   @brief Specialization to regression trees.
 */
class ResponseReg : public Response {
  class SampleReg* SampleRows(const class RowRank *rowRank);
  unsigned int *row2Rank;

 public:
  ResponseReg(const double _y[], double yRanked[], unsigned int nRow);
  ~ResponseReg();
  class SampleReg **BlockSample(const class RowRank *rowRank, int tCount);
};

/**
   @brief Specialization to classification trees.
 */
class ResponseCtg : public Response {
  class SampleCtg* SampleRows(const class RowRank *rowRank);
  unsigned int *yCtg; // 0-based factor-valued response.
 public:

  class SampleCtg **BlockSample(const class RowRank *rowRank, int tCount);
  ResponseCtg(const int _yCtg[], const double _yProxy[], unsigned int _nRow);
  ~ResponseCtg();
  
  static int CtgSum(unsigned int sIdx, double &sum);
};

#endif
