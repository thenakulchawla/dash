// Copyright (c) 2018 Nakul Chawla
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RAPTOR_ENCODING_H
#define BITCOIN_RAPTOR_ENCODING_H

#include <array>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <stdlib.h>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include "serialize.h"
// #include "../src/libRaptorQ/RaptorQ/RaptorQ_v1_hdr.hpp"
#include "RaptorQ/RaptorQ_v1_hdr.hpp"


// extern bool fShardErasureEnabled;
extern bool fRaptorEnabled;

class CDataStream;

// class CRaptorDecodeSymbol
// {
// public:
//     CRaptorDecodeSymbol();
//     CRaptorDecodeSymbol(const uint32_t nSize, const uint8_t nOverhead);
//
//     ADD_SERIALIZE_METHODS;
//
//     template <typename Stream, typename Operation>
//     inline void SerializationOp(Stream &s, Operation ser_action)
//     {
//         READWRITE(nSize, nOverhead);
//     }
//
// private:
//     uint32_t nSize;
//     uint8_t nOverhead;
//
// };

template <typename T>
inline void pack (std::vector< uint8_t >& dst, T& data);


template <typename T>
inline void unpack (std::vector <uint8_t >& src, int index, T& data);

bool test_raptor (const uint32_t mysize, std::mt19937_64 &rnd, float drop_prob, const uint8_t overhead);


#endif // BITCOIN_SHARD_H
