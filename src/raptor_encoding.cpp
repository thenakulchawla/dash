#include <iostream>
#include <algorithm>
#include "alert.h"
#include "chain.h"
#include "chainparams.h"
#include "init.h"
#include "net.h"
#include "primitives/block.h"
#include "streams.h"
#include "util.h"
#include "version.h"
#include "utiltime.h"
#include "validation.h"
// #include "serialize.h"
#include "stat.h"

#include "raptor_encoding.h"

namespace RaptorQ = RaptorQ__v1;

CRaptorSymbolData raptordata;

std::map< uint256, std::vector<std::pair<uint32_t, std::vector<uint8_t>>> > raptorSymbols;

CRaptorSymbol::CRaptorSymbol() 
{ 
    this->nSymbolSize=0;
}

CRaptorSymbol::~CRaptorSymbol()
{
}

// CRaptorSymbol::CRaptorSymbol(int nSymbolSize)
// {
//     this->nSymbolSize = nSymbolSize;
// }

CRaptorSymbol::CRaptorSymbol(const CBlockRef pblock, uint16_t nSymbolSize)
{

    this->header = pblock->GetBlockHeader();
    this->vEncoded = encode(pblock, nSymbolSize);
    this->nSymbolSize = nSymbolSize;

    std::vector<uint8_t> input;
    pack(input,*pblock);
    uint16_t min_symbols = CalculateMinSymbols(nSymbolSize, input); 
    this->nBlockSize = CalculateBlockSizeForRaptorSymbol( min_symbols);
    RaptorQ::Block_Size block = static_cast<RaptorQ::Block_Size> ( nBlockSize );
    RaptorQ::Encoder<typename std::vector<uint8_t>::iterator,typename std::vector<uint8_t>::iterator> enc (block, nSymbolSize);

    this->nSize = CalculateTotalSymbolSize(enc, input);

}

template <typename T>
inline void pack (std::vector< uint8_t >& dst, T& data) 
{
    // uint8_t * src = static_cast < uint8_t* >(static_cast < void * >(&data));
    uint8_t * src = reinterpret_cast < uint8_t* >(&data);
    dst.insert (dst.end (), src, src + sizeof (T));
}   

template <typename T>
inline void unpack (std::vector <uint8_t >& src, int index, T& data) 
{
    std::copy (&src[index], &src[index + sizeof (T)], &data);
}

template <class T>
void CRaptorSymbolData::updateStats(std::map<int64_t, T>& statsMap, T value)
{
    AssertLockHeld(cs_raptorstats);
    statsMap[getTimeForStats()] = value;
    expireStats(statsMap);

}

template <class T>
void CRaptorSymbolData::expireStats(std::map<int64_t, T>& statsMap)
{
    AssertLockHeld(cs_raptorstats);
    // Delete entries that are more than 24 hours old
    int64_t nTimeCutOff = getTimeForStats() - 60*60*24*1000;

    typename std::map<int64_t, T>::iterator iter  = statsMap.begin();
    while (iter != statsMap.end())
    {
        typename std::map<int64_t, T>::iterator mi = iter++;
        // increment to avoid iterator becoming invalid when erasing below
        if (mi->first < nTimeCutOff)
            statsMap.erase(mi);

    }

}

double CRaptorSymbolData::average(std::map<int64_t, uint64_t>& map)
{
    AssertLockHeld(cs_raptorstats);
    expireStats(map);
    if (map.size() == 0)
        return 0.0;

    uint64_t accum=0U;
    for (std::pair<int64_t, uint64_t>p : map)
    {
        //avoid wraparounds
        accum = std::max(accum, accum+p.second);
    }
    return ( double ) accum/map.size();

}

void CRaptorSymbolData::IncrementDecodeFailures()
{
    LOCK(cs_raptorstats);
    nDecodeFailures +=1;

}

uint64_t CRaptorSymbolData::AddRaptorSymbolBytes(uint64_t bytes, CNode *pfrom)
{
    pfrom->nLocalRaptorSymbolBytes += bytes;
    uint64_t ret= nRaptorSymbolBytes.fetch_add(bytes) + bytes;
    return ret;

}

uint64_t CRaptorSymbolData::DeleteRaptorSymbolBytes(uint64_t bytes, CNode *pfrom)
{
    if (bytes <= pfrom->nLocalRaptorSymbolBytes)
        pfrom->nLocalRaptorSymbolBytes -= bytes;

    if (bytes <= nRaptorSymbolBytes)
        nRaptorSymbolBytes.fetch_sub(bytes);

}

void CRaptorSymbolData::ClearRaptorSymbolData(CNode *pnode)
{
    // Remove bytes from counter
    raptordata.DeleteRaptorSymbolBytes(pnode->nLocalRaptorSymbolBytes, pnode);
    pnode->nLocalRaptorSymbolBytes = 0;

    // Clear out graphene block data we no longer need
    // pnode->grapheneBlockWaitingForTxns = -1;
    pnode->raptorSymbol.SetNull();
    // pnode->grapheneBlockHashes.clear();
    // pnode->grapheneMapHashOrderIndex.clear();
    // pnode->mapGrapheneMissingTx.clear();

    LogPrint("raptor", "Total in-memory raptor bytes size after clearing a raptor block is %ld bytes\n",
        raptordata.GetRaptorSymbolBytes());
}

void CRaptorSymbolData::ClearRaptorSymbolData(CNode *pnode, const uint256 &hash)
{
    // We must make sure to clear the graphene block data first before clearing the graphene block in flight.
    ClearRaptorSymbolData(pnode);
    // ClearRaptorSymbolInFlight(pnode, hash);
}

void CRaptorSymbolData::ClearRaptorSymbolStats()
{
    LOCK(cs_raptorstats);

    nSymbolSize.Clear();
    nInBoundSymbols.Clear();
    nOutBoundSymbols.Clear();
    nDecodeFailures.Clear();
    nTotalRaptorSymbolBytes.Clear();

    mapRaptorSymbolResponseTime.clear();
    mapRaptorSymbolValidationTime.clear();
}

uint64_t CRaptorSymbolData::GetRaptorSymbolBytes() { return nRaptorSymbolBytes.load(); }

std::string CRaptorSymbolData::ToString()
{
    LOCK(cs_raptorstats);
    std::ostringstream ss;
    ss << "Raptor Codes consumed " << formatInfoUnit(double(nSymbolSize())) << " bandwidth";
    return ss.str();

}

bool IsRaptorSymbolValid(CNode* pfrom, const CBlockHeader& header)
{
    CValidationState state;
    if (!CheckBlockHeader (header, state, Params().GetConsensus(), true) )
    {
        return error("Received invalid header for raptor symbol %s from peer %d", header.GetHash().ToString(), pfrom->id);
    }

    if (state.Invalid())
    {
        return error("Received invalid header for raptor symbol %s from peer %d", header.GetHash().ToString(), pfrom->id);
    }

    // Check for the current symbol size too. Add it as a parameter.

    return true;
}

bool IsRaptorEnabled()
{
    return fRaptorEnabled;
}

std::vector<std::pair<uint32_t, std::vector<uint8_t>>> encode (const CBlockRef pblock, uint16_t nSymbolSize)
{

    // const CBlockIndex* pindex = nullptr;
    // BlockMap::iterator mi = mapBlockIndex.find(pblock_param.GetBlockHeader().GetHash());
    // if (mi != mapBlockIndex.end())
    // {
    //     pindex = ( *mi ).second;
    // }
    //
    // CBlock pblock;
    // bool ret = ReadBlockFromDisk(pblock, pindex, Params().GetConsensus());

    // int nSizeBlock = ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION); 
    // LogPrint("raptor","Size of block to be encoded %d\n", nSizeBlock);
    LogPrint("raptor", "Block header before packing %s\n", pblock->GetBlockHeader().GetHash().ToString());

    // First check whether the block is already encoded

    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> to_send;
    uint256 headerHash = pblock->GetBlockHeader().GetHash();
    if (raptorSymbols.find( pblock->GetBlockHeader().GetHash()) != raptorSymbols.end() )
    {
        LogPrint("raptor", "Symbols are already encoded\n");
        to_send = raptorSymbols[headerHash];
        return to_send;
    }
            
    std::vector<uint8_t> input;
    pack(input, *pblock);

    // how many symbols do we need to encode all our input in a single block?
    // auto min_symbols = (input.size() * sizeof(uint256)) / nSymbolSize;
    
    auto min_symbols = (input.size()*sizeof(uint8_t)) / nSymbolSize;
    if ((input.size() * sizeof(uint8_t)) % nSymbolSize != 0)
        ++min_symbols;

    // LogPrint("raptor","Size of input to be encoded %d\n", input.size());
    // LogPrint("raptor", "min_symbols %d\n", min_symbols);


    // convert "symbols" to a typesafe equivalent, RaptorQ::Block_Size
    // This is needed because not all numbers are valid block sizes, and this
    // helps you choose the right block size
    RaptorQ::Block_Size block = RaptorQ::Block_Size::Block_10;
    for (auto blk : *RaptorQ::blocks)
    {
        // RaptorQ::blocks is a pointer to an array, just scan it to find your
        // block.
        if (static_cast<uint16_t> (blk) >= min_symbols) {
            block = blk;
            break;
        }

    }

    RaptorQ::Encoder<typename std::vector<uint8_t>::iterator, typename std::vector<uint8_t>::iterator> enc (block, nSymbolSize);

    // give input to the encoder, the encoder answers with the size of what
    // it can use
    uint32_t nSize = enc.set_data(input.begin(), input.end());
    
    // if (enc.set_data(input.begin(), input.end()) != nSize)
    // {
    //     LogPrint("raptor", "Could not give data to the encoder\n");
    //     LogPrint("raptor", "Encoder size should be %d\n", static_cast<uint32_t>(size_to_be));
    //
    //     // return false;
    // }

    // actual symbols. you could just use static_cast<uint16_t> (blok)
    // but this way you can actually query the encoder.
    uint16_t _symbols = enc.symbols();
    // print some stuff in output
    
    // LogPrint("raptor", "Size: %d, symbols: %d, symbol size: %d \n", nSize, static_cast<uint32_t> (_symbols), static_cast<int32_t>(enc.symbol_size()) );

    // RQ need to do its magic on the input before you can ask the symbols.
    // multiple ways to do this are available.
    // The simplest is to run the computation and block everything until
    // the work has been done. Not a problem for small sizes (<200),
    // but big sizes will take **a lot** of time, consider running this with the
    // asynchronous calls
    if (!enc.compute_sync()) 
    {
        // if this happens it's a bug in the library.
        // the **Decoder** can fail, but the **Encoder** can never fail.
        LogPrint("raptor","Enc-RaptorQ failure! really bad!\n");
        // return false;
    }

    auto source_sym_it = enc.begin_source();
    for (; source_sym_it != enc.end_source(); ++source_sym_it) 
    {
        std::vector<uint8_t> source_sym_data (nSymbolSize, 0);
        auto it = source_sym_data.begin();
        auto written = (*source_sym_it) (it, source_sym_data.end());
        uint32_t tmp_id = (*source_sym_it ).id();
        // LogPrint("raptor", "tmp_id during encoding : %d\n", tmp_id);

        if (written != nSymbolSize) 
        {
            // this can only happen if "source_sym_data" did not have
            // enough space for a symbol (here: never)
            LogPrint("raptor","written %d -vs- nSymbolSize %d Could not get the whole source symbol!\n",written,nSymbolSize);
        }

        // LogPrint("raptor", "Encoding to_send of size %d\n", to_send.size());
        to_send.emplace_back(tmp_id, std::move(source_sym_data));

    }

    raptorSymbols.insert(std::make_pair(headerHash, to_send));
    // Decode her for testing

    /**
    using Decoder_type = RaptorQ::Decoder<typename std::vector<uint8_t>::iterator,typename std::vector<uint8_t>::iterator>;
    LogPrint("raptor", "Decoding blockSize : using from above, nSymbolSize : %d, nSize : %d while encoding\n",  nSymbolSize, nSize);
    // RaptorQ::Block_Size block = static_cast<RaptorQ::Block_Size> ( blockSize );
    // LogPrint("raptor", "Decoding block : %d, nSymbolSize : %d, nSize : %d\n", block, nSymbolSize, nSize);

    
    Decoder_type dec (block, nSymbolSize, Decoder_type::Report::COMPLETE);
    // "Decoder_type::Report::COMPLETE" means that the decoder will not
    // give us any output until we have decoded all the data.
    // there are modes to extract the data symbol by symbol in an ordered
    // an unordered fashion, but let's keep this simple.

    // we will store the output of the decoder here:
    // note: the output need to have at least "nSize" bytes, and
    // we fill it with zeros
    std::vector<uint8_t> output (nSize, 0);


    // now push every received symbol into the decoder

    for (auto &rec_sym : to_send)
    {
        // When you add a symbol, you can get:
        //   NONE: no error
        //   NOT_NEEDED: libRaptorQ ignored it because everything is
        //              already decoded
        //   INITIALIZATION: wrong parameters to the decoder contructor
        //   WRONG_INPUT: not enough data on the symbol?
        //   some_other_error: errors in the library
        auto it = rec_sym.second.begin();
        uint32_t tmp_id = rec_sym.first;
        LogPrint("raptor", "to_send.size() : %d, rec_sym.second.size(): %d , tmp_id: %d\n", to_send.size(), rec_sym.second.size(), tmp_id);

        auto err = dec.add_symbol(it, rec_sym.second.end(), tmp_id  );
        if (err == RaptorQ::Error::NONE) 
        {
            LogPrint("raptor", " NONE error in decoder while encoding\n");
            // return false;
        }
        else if (err == RaptorQ::Error::NOT_NEEDED)
        {
            LogPrint("raptor", " NOT_NEEDED error in decoder while encoding\n");
            // return false;
        }
        else if (err == RaptorQ::Error::WRONG_INPUT)
        {
            LogPrint("raptor", "  WRONG_INPUT in decoder while encoding\n");
            // return false;
        }
        else if (err == RaptorQ::Error::INITIALIZATION)
        {
            LogPrint("raptor", "INITIALIZATION error in decoder while encoding\n");
            // return false;
        }
        else
        {
            LogPrint("raptor","error adding, library  while encoding \n");
            // return false;

        }

    }


    // by now we now there will be no more input, so we tell this to the
    // decoder. You can skip this call, but if the decoder does not have
    // enough data it sill wait forever (or until you call .stop())
    dec.end_of_input (RaptorQ::Fill_With_Zeros::NO);
    // optional if you want partial decoding without using the repair
    // symbols
    // std::vector<bool> symbols_bitmask = dec.end_of_input (
    //                                          RaptorQ::Fill_With_Zeros::YES);

    // decode, and do not return until the computation is finished.
    auto res = dec.wait_sync();
    if (res.error != RaptorQ::Error::NONE) {
        LogPrintf( "Couldn't decode.\n");
        // return false;
    }

    // now save the decoded data into our output, and finally make a block out of it to be flushed
    size_t decode_from_byte = 0;
    size_t skip_bytes_at_beginning_of_output =0;
    auto out_it = output.begin();
    auto decoded = dec.decode_bytes (out_it, output.end(), decode_from_byte, skip_bytes_at_beginning_of_output);
    // "decode_from_byte" can be used to have only a part of the output.
    // it can be used in advanced setups where you ask only a part
    // of the block at a time.
    // "skip_bytes_at_begining_of_output" is used when dealing with containers
    // which size does not align with the output. For really advanced usage only
    // Both should be zero for most setups.

    if (decoded.written != nSize) 
    {
        if (decoded.written == 0) 
        {
            // we were really unlucky and the RQ algorithm needed
            // more symbols!
            LogPrintf( "Couldn't decode, RaptorQ Algorithm failure. Can't Retry.\n");
        } 
        else 
        {
            // probably a library error
            LogPrintf( "Partial Decoding? This should not have happened: decoded-wriiten %d vs nSize %s \n ", decoded.written, nSize);
        }
        // return false;
    } 
    else 
    {
        LogPrintf( "Decoded: %d\n ", nSize) ;
    }

    CBlock decode_block;
    // size_t offset=0;
    // unpack(output,offset,decode_block);

    CDataStream ss(output, SER_NETWORK, PROTOCOL_VERSION);
    ss >> decode_block;
    LogPrint("raptor", "Block header after unpacking %d\n", decode_block.GetBlockHeader().GetHash().ToString());
    **/

    return to_send;
}

bool CRaptorSymbol::decode (std::vector<std::pair<uint32_t, std::vector<uint8_t>>>& vEncoded, uint16_t blockSize, uint16_t nSymbolSize, uint32_t nSize)
{
    // define "Decoder_type" to write less afterwards
    using Decoder_type = RaptorQ::Decoder<typename std::vector<uint8_t>::iterator,typename std::vector<uint8_t>::iterator>;
    // LogPrint("raptor", "Decoding blockSize : %d, nSymbolSize : %d, nSize : %d\n", blockSize, nSymbolSize, nSize);
    RaptorQ::Block_Size block = static_cast<RaptorQ::Block_Size> ( blockSize );
    // LogPrint("raptor", "Decoding block : %d, nSymbolSize : %d, nSize : %d\n", block, nSymbolSize, nSize);

    
    Decoder_type dec (block, nSymbolSize, Decoder_type::Report::COMPLETE);
    // "Decoder_type::Report::COMPLETE" means that the decoder will not
    // give us any output until we have decoded all the data.
    // there are modes to extract the data symbol by symbol in an ordered
    // an unordered fashion, but let's keep this simple.

    // we will store the output of the decoder here:
    // note: the output need to have at least "nSize" bytes, and
    // we fill it with zeros
    std::vector<uint8_t> output (nSize, 0);


    // now push every received symbol into the decoder

    for (auto &rec_sym : vEncoded)
    {
        // When you add a symbol, you can get:
        //   NONE: no error
        //   NOT_NEEDED: libRaptorQ ignored it because everything is
        //              already decoded
        //   INITIALIZATION: wrong parameters to the decoder contructor
        //   WRONG_INPUT: not enough data on the symbol?
        //   some_other_error: errors in the library
        auto it = rec_sym.second.begin();
        uint32_t tmp_id = rec_sym.first;
        // LogPrint("raptor", "vEncoded.size() : %d, rec_sym.second.size(): %d , tmp_id: %d\n", vEncoded.size(), rec_sym.second.size(), tmp_id);

        auto err = dec.add_symbol(it, rec_sym.second.end(), tmp_id  );
        if (err == RaptorQ::Error::NONE) 
        {
            // LogPrint("raptor", " NONE error in decoder\n");
            // return false;
        }
        else if (err == RaptorQ::Error::NOT_NEEDED)
        {
            // LogPrint("raptor", " NOT_NEEDED error in decoder\n");
            // return false;
        }
        else if (err == RaptorQ::Error::WRONG_INPUT)
        {
            LogPrint("raptor", "  WRONG_INPUT in decoder\n");
            return false;
        }
        else if (err == RaptorQ::Error::INITIALIZATION)
        {
            LogPrint("raptor", "INITIALIZATION error in decoder\n");
            return false;
        }
        else
        {
            LogPrint("raptor","error adding, library  \n");
            return false;

        }

    }


    // by now we now there will be no more input, so we tell this to the
    // decoder. You can skip this call, but if the decoder does not have
    // enough data it sill wait forever (or until you call .stop())
    dec.end_of_input (RaptorQ::Fill_With_Zeros::NO);
    // optional if you want partial decoding without using the repair
    // symbols
    // std::vector<bool> symbols_bitmask = dec.end_of_input (
    //                                          RaptorQ::Fill_With_Zeros::YES);

    // decode, and do not return until the computation is finished.
    auto res = dec.wait_sync();
    if (res.error != RaptorQ::Error::NONE) {
        LogPrintf( "Couldn't decode.\n");
        return false;
    }

    // now save the decoded data into our output, and finally make a block out of it to be flushed
    size_t decode_from_byte = 0;
    size_t skip_bytes_at_beginning_of_output =0;
    auto out_it = output.begin();
    auto decoded = dec.decode_bytes (out_it, output.end(), decode_from_byte, skip_bytes_at_beginning_of_output);
    // "decode_from_byte" can be used to have only a part of the output.
    // it can be used in advanced setups where you ask only a part
    // of the block at a time.
    // "skip_bytes_at_begining_of_output" is used when dealing with containers
    // which size does not align with the output. For really advanced usage only
    // Both should be zero for most setups.

    if (decoded.written != nSize) 
    {
        if (decoded.written == 0) 
        {
            // we were really unlucky and the RQ algorithm needed
            // more symbols!
            LogPrintf( "Couldn't decode, RaptorQ Algorithm failure. Can't Retry.\n");
        } 
        else 
        {
            // probably a library error
            LogPrintf( "Partial Decoding? This should not have happened: decoded-wriiten %d vs nSize %s \n ", decoded.written, nSize);
        }
        return false;
    } 
    else 
    {
        LogPrintf( "Decoded: %d\n ", nSize) ;
    }


    // byte-wise check: did we actually decode everything the right way?
    // for testing
    // for (uint64_t i = 0; i < nSize; ++i) {
    //     if (input[i] != output[i]) {
    //         // this is a bug in the library, please report
    //         LogPrintf("The output does not correspond to the input!\n");
    //         return false;
    //     }
    // }

    //Write logic to form the block and flush it to disk
    CBlock decode_block;
    // size_t offset=0;
    // unpack(output,offset,decode_block);

    CDataStream ss(output, SER_NETWORK, PROTOCOL_VERSION);
    ss >> decode_block;
    LogPrint("raptor", "Block header after unpacking %d\n", decode_block.GetBlockHeader().GetHash().ToString());

    return true;

}


uint32_t CalculateTotalSymbolSize(RaptorQ::Encoder<typename std::vector<uint8_t>::iterator,typename std::vector<uint8_t>::iterator>& enc, std::vector<uint8_t>& input)
{
    return enc.set_data (input.begin(), input.end());

}


uint16_t CalculateMinSymbols(uint16_t nSymbolSize, std::vector<uint8_t>& input)
{
    auto min_symbols = (input.size()*sizeof(uint8_t)) / nSymbolSize;
    if ((input.size() * sizeof(uint8_t)) % nSymbolSize != 0)
        ++min_symbols;

    return min_symbols;
}

uint16_t CalculateBlockSizeForRaptorSymbol(uint16_t min_symbols)
{
    RaptorQ::Block_Size block = RaptorQ::Block_Size::Block_10;
    for (auto blk : *RaptorQ::blocks)
    {
        // RaptorQ::blocks is a pointer to an array, just scan it to find your
        // block.
        if (static_cast<uint16_t> (blk) >= min_symbols) {
            block = blk;
            break;
        }

    }

    return static_cast<uint16_t> (block);
}


bool test_raptor (const uint32_t nSize, std::mt19937_64 &rnd, float drop_probability, const uint8_t overhead)
{
    CBlockIndex* current_block = chainActive.Tip(); 
    CBlock cBlock;

    bool ret = ReadBlockFromDisk(cBlock, current_block->GetBlockPos(), Params().GetConsensus()); 

    std::vector<uint8_t> input; 

    pack (input, cBlock);

    // symbol size in bytes
    const uint16_t nSymbolSize = 16;

    // how many symbols do we need to encode all our input in a single block?
    auto min_symbols = (input.size() * sizeof(uint8_t)) / nSymbolSize;
    if ((input.size() * sizeof(uint8_t)) % nSymbolSize != 0)
        ++min_symbols;

    // convert "symbols" to a typesafe equivalent, RaptorQ::Block_Size
    // This is needed becouse not all numbers are valid block sizes, and this
    // helps you choose the right block size

    RaptorQ::Block_Size block = RaptorQ::Block_Size::Block_10;
    for (auto blk : *RaptorQ::blocks) 
    {
        // RaptorQ::blocks is a pointer to an array, just scan it to find your
        // block.
        if (static_cast<uint16_t> (blk) >= min_symbols) {
            block = blk;
            break;
        }
    }


    // now initialize the encoder.
    // the input for the encoder is std::vector<uint8_t>
    // the output for the encoder is std::vector<uint8_t>
    // yes, you can have different types, but most of the time you will
    // want to work with uint8_t
    RaptorQ::Encoder<typename std::vector<uint8_t>::iterator,typename std::vector<uint8_t>::iterator> enc (block, nSymbolSize);

    // give the input to the encoder. the encoder answers with the size of what
    // it can use
    if (enc.set_data (input.begin(), input.end()) != nSize) {
        LogPrint("raptor","Could not give data to the encoder :(\n");
        return false;
    }

    // actual symbols. you could just use static_cast<uint16_t> (blok)
    // but this way you can actually query the encoder.
    uint16_t _symbols = enc.symbols();
    // print some stuff in output
    
    LogPrint("raptor", "Size: %d, symbols: %d, symbol size: %d \n", nSize, static_cast<uint32_t> (_symbols), static_cast<int32_t>(enc.symbol_size()) );

    // RQ need to do its magic on the input before you can ask the symbols.
    // multiple ways to do this are available.
    // The simplest is to run the computation and block everything until
    // the work has been done. Not a problem for small sizes (<200),
    // but big sizes will take **a lot** of time, consider running this with the
    // asynchronous calls
    if (!enc.compute_sync()) {
        // if this happens it's a bug in the library.
        // the **Decoder** can fail, but the **Encoder** can never fail.
        LogPrint("raptor","Enc-RaptorQ failure! really bad!\n");
        return false;
    }


    // // the probability that a symbol will be dropped.
    if (drop_probability > static_cast<float> (90.0))
        drop_probability = 90.0;   // this is still too high probably.


    // we will store here all encoded and transmitted symbols
    // std::pair<symbol id (esi), symbol data>
    using symbol_id = uint32_t;
    std::vector<std::pair<symbol_id, std::vector<uint8_t>>> received;
    {
        // in this block we will generate the symbols that will be sent to
        // the decoder.
        // a block of size X will need at least X symbols to be decoded.
        // we will randomly drop some symbols, but we will keep generating
        // repari symbols until we have the required number of symbols.

        std::uniform_real_distribution<float> drop_rnd (0.0, 100.0);
        uint32_t received_tot = 0;

        // Now get the source symbols.
        // source symbols are specials because they contain the input data
        // as-is, so if you get all of these, you don't need repair symbols
        // to make sure that we are using the decoder, drop the first
        // source symbol.
        auto source_sym_it = enc.begin_source();
        ++source_sym_it; // ignore the first soure symbol (=> drop it)
        source_sym_it++;
        for (; source_sym_it != enc.end_source(); ++source_sym_it) {
            // we save the symbol here:
            // make sure the vector has enough space for the symbol:
            // fill it with zeros for the size of the symbol
            std::vector<uint8_t> source_sym_data (nSymbolSize, 0);

            // save the data of the symbol into our vector
            auto it = source_sym_data.begin();
            auto written = (*source_sym_it) (it, source_sym_data.end());
            if (written != nSymbolSize) {
                // this can only happen if "source_sym_data" did not have
                // enough space for a symbol (here: never)
                LogPrintf("written %d -vs- nSymbolSize %d Could not get the whole source symbol!\n",written,nSymbolSize);
                return false;
            }

            // can we keep this symbol or do we randomly drop it?
            float dropped = drop_rnd (rnd);
            if (dropped <= drop_probability) {
                continue; // start the cycle again
            }

            // good, the symbol was received.
            ++received_tot;
            // add it to the vector of received symbols
            symbol_id tmp_id = (*source_sym_it).id();
            received.emplace_back (tmp_id, std::move(source_sym_data));
        }

        // LogPrintf("Source Packet lost: %d\n" << enc.symbols() - received.size());

        //--------------------------------------------
        // we finished working with the source symbols.
        // now we need to transmit the repair symbols.
        auto repair_sym_it = enc.begin_repair();
        auto max_repair = enc.max_repair(); // RaptorQ can theoretically handle
        // infinite repair symbols
        // but computers are not so infinite

        // we need to have at least enc.symbols() + overhead symbols.
        for (; received.size() < (enc.symbols() + overhead) &&
                repair_sym_it != enc.end_repair (max_repair);
                ++repair_sym_it) {
            // we save the symbol here:
            // make sure the vector has enough space for the symbol:
            // fill it with zeros for the size of the symbol
            std::vector<uint8_t> repair_sym_data (nSymbolSize, 0);

            // save the data of the symbol into our vector
            auto it = repair_sym_data.begin();
            auto written = (*repair_sym_it) (it, repair_sym_data.end());
            if (written != nSymbolSize) {
                LogPrintf("written %d -vs- nSymbolSize %d Could not get the whole source symbol!\n", written, nSymbolSize);
                // this can only happen if "repair_sym_data" did not have
                // enough space for a symbol (here: never)
                return false;
            }

            // can we keep this symbol or do we randomly drop it?
            float dropped = drop_rnd (rnd);
            if (dropped <= drop_probability) {
                continue; // start the cycle again
            }

            // good, the symbol was received.
            ++received_tot;
            // add it to the vector of received symbols
            symbol_id tmp_id = (*repair_sym_it).id();
            received.emplace_back (tmp_id, std::move(repair_sym_data));

        }
        if (repair_sym_it == enc.end_repair (enc.max_repair())) {
            // we dropped waaaay too many symbols!
            // should never happen in real life. it means that we do not
            // have enough repair symbols.
            // at this point you can actually start to retransmit the
            // repair symbols from enc.begin_repair(), but we don't care in
            // this example
            LogPrintf("Maybe losing %d % is too much?\n", drop_probability);
            return false;
        }
    }

    // Now we all the source and repair symbols are in "received".
    // we will use those to start decoding:


    // define "Decoder_type" to write less afterwards
    using Decoder_type = RaptorQ::Decoder<typename std::vector<uint8_t>::iterator,typename std::vector<uint8_t>::iterator>;
    Decoder_type dec (block, nSymbolSize, Decoder_type::Report::COMPLETE);
    // "Decoder_type::Report::COMPLETE" means that the decoder will not
    // give us any output until we have decoded all the data.
    // there are modes to extract the data symbol by symbol in an ordered
    // an unordered fashion, but let's keep this simple.


    // we will store the output of the decoder here:
    // note: the output need to have at least "nSize" bytes, and
    // we fill it with zeros
    std::vector<uint8_t> output (nSize, 0);

    // now push every received symbol into the decoder
    for (auto &rec_sym : received) {
        // as a reminder:
        //  rec_sym.first = symbol_id (uint32_t)
        //  rec_sym.second = std::vector<uint8_t> symbol_data
        symbol_id tmp_id = rec_sym.first;
        auto it = rec_sym.second.begin();
        auto err = dec.add_symbol (it, rec_sym.second.end(), tmp_id);
        if (err != RaptorQ::Error::NONE && err != RaptorQ::Error::NOT_NEEDED) {
            // When you add a symbol, you can get:
            //   NONE: no error
            //   NOT_NEEDED: libRaptorQ ignored it because everything is
            //              already decoded
            //   INITIALIZATION: wrong parameters to the decoder contructor
            //   WRONG_INPUT: not enough data on the symbol?
            //   some_other_error: errors in the library
            LogPrintf("error adding?\n");
            return false;
        }
    }

    // by now we now there will be no more input, so we tell this to the
    // decoder. You can skip this call, but if the decoder does not have
    // enough data it sill wait forever (or until you call .stop())
    dec.end_of_input (RaptorQ::Fill_With_Zeros::NO);
    // optional if you want partial decoding without using the repair
    // symbols
    // std::vector<bool> symbols_bitmask = dec.end_of_input (
    //                                          RaptorQ::Fill_With_Zeros::YES);

    // decode, and do not return until the computation is finished.
    auto res = dec.wait_sync();
    if (res.error != RaptorQ::Error::NONE) {
        LogPrintf( "Couldn't decode.\n");
        return false;
    }

    // now save the decoded data in our output
    size_t decode_from_byte = 0;
    size_t skip_bytes_at_begining_of_output = 0;
    auto out_it = output.begin();
    auto decoded = dec.decode_bytes (out_it, output.end(), decode_from_byte,
            skip_bytes_at_begining_of_output);
    // "decode_from_byte" can be used to have only a part of the output.
    // it can be used in advanced setups where you ask only a part
    // of the block at a time.
    // "skip_bytes_at_begining_of_output" is used when dealing with containers
    // which size does not align with the output. For really advanced usage only
    // Both should be zero for most setups.


    if (decoded.written != nSize) {
        if (decoded.written == 0) {
            // we were really unlucky and the RQ algorithm needed
            // more symbols!
            LogPrintf( "Couldn't decode, RaptorQ Algorithm failure. Can't Retry.\n");
        } else {
            // probably a library error
            LogPrintf( "Partial Decoding? This should not have happened: decoded-wriiten %d vs nSize %s \n ", decoded.written, nSize);
        }
        return false;
    } else {
        LogPrintf( "Decoded: %d\n ", nSize) ;
    }

    // byte-wise check: did we actually decode everything the right way?
    for (uint64_t i = 0; i < nSize; ++i) {
        if (input[i] != output[i]) {
            // this is a bug in the library, please report
            LogPrintf("The output does not correspond to the input!\n");
            return false;
        }
    }

    return (true && ret);

}

