// Copyright (c) 2018 Nakul Chawla
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RAPTOR_ENCODING_H
#define BITCOIN_RAPTOR_ENCODING_H

#include <array>
#include <atomic>
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
#include "consensus/validation.h"
#include "chain.h"
#include "net.h"
#include "primitives/block.h"
#include "serialize.h"
#include "stat.h"
#include "sync.h"
#include "uint256.h"
#include "util.h"
#include "utiltime.h"

#include "RaptorQ/RaptorQ_v1_hdr.hpp"


extern bool fRaptorEnabled;

class CDataStream;
class CNode;
class CConnman;


class CRaptorSymbol
{
public:
    // std::vector<uint8_t> vEncoded;
    CBlockHeader header;
    uint32_t nSize;
    uint16_t nSymbolSize;

public:
    CRaptorSymbol();
    CRaptorSymbol(const CBlockRef pblock, uint32_t nSize, uint16_t nSymbolSize);
    ~CRaptorSymbol();

    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action)
    {
        READWRITE(header);
        READWRITE(nSize);
        READWRITE(nSymbolSize);
        // READWRITE(vEncoded);
    }

    void SetNull()
    {
        header.SetNull();
        nSize = 0;
        nSymbolSize =0;
        // vEncoded.clear();

    }

};

extern std::map<uint256, CRaptorSymbol> raptorSymbols;

class CRaptorSymbolData
{
private:
    std::atomic<uint64_t> nRaptorSymbolBytes{0};

    CCriticalSection cs_mapRaptorSymbolTimer; // locks the below timer
    std::map<uint256, uint64_t> mapRaptorSymbolTimer;
    CCriticalSection cs_raptorstats; // locks everything below this point
    
    CStatHistory<uint64_t> nSymbolSize;
    CStatHistory<uint64_t> nBlockSize;
    CStatHistory<uint64_t> nInBoundSymbols;
    CStatHistory<uint64_t> nOutBoundSymbols;
    CStatHistory<uint64_t> nDecodeFailures;
    CStatHistory<uint64_t> nTotalRaptorSymbolBytes;
    std::map<int64_t, int64_t> mapRaptorSymbolResponseTime;
    std::map<int64_t, int> mapRaptorSymbolValidationTime;

    template <class T>
    void updateStats(std::map<int64_t, T>& statsMap, T value);
    template <class T>
    void expireStats(std::map<int64_t, T>& statsMap);
    double average(std::map<int64_t, uint64_t>& map);

protected:
    virtual int64_t getTimeForStats() { return GetTimeMillis();}

public:
    void IncrementDecodeFailures();

    uint64_t AddRaptorSymbolBytes(uint64_t, CNode *pfrom);
    uint64_t DeleteRaptorSymbolBytes(uint64_t, CNode *pfrom);
    void ClearRaptorSymbolData(CNode *pfrom);
    void ClearRaptorSymbolData(CNode *pfrom, const uint256 &hash);
    void ClearRaptorSymbolStats();
    uint64_t GetRaptorSymbolBytes();
    std::string ToString();


};

extern CRaptorSymbolData raptordata; // Singleton class

bool encode ( const CBlockRef pblock, uint32_t nSize, const uint16_t nSymbolSize);

bool decode(std::vector<uint8_t>& vEncoded);
bool IsRaptorSymbolValid(CNode* pfrom, const CBlockHeader& header);
bool IsRaptorEnabled();

template <typename T>
inline void pack (std::vector< uint8_t>& dst, T& data);

template <typename T>
inline void unpack (std::vector <uint8_t >& src, int index, T& data);

bool test_raptor (const uint32_t mysize, std::mt19937_64 &rnd, float drop_prob, const uint8_t overhead);


#endif // BITCOIN_SHARD_H
