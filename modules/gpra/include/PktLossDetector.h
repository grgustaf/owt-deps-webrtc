//INTEL_ANDROID
/*
 * INTEL CONFIDENTIAL
 * Copyright 2013 Intel Corporation All Rights Reserved.
 *
 * The source code, information and material ("Material") contained herein is owned
 * by Intel Corporation or its suppliers or licensors, and title to such Material
 * remains with Intel Corporation or its suppliers or licensors. The Material contains
 * proprietary information of Intel or its suppliers and licensors. The Material is
 * protected by worldwide copyright laws and treaty provisions. No part of the Material
 * may be used, copied, reproduced, modified, published, uploaded, posted, transmitted,
 * distributed or disclosed in any way without Intel's prior express written permission.
 * No license under any patent, copyright or other intellectual property rights in the
 * Material is granted to or conferred upon you, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing.
 *
 * Unless otherwise agreed by Intel in writing, you may not remove or alter this notice or any
 * other notice embedded in Materials by Intel or Intel's suppliers or licensors in any way.
 */

#ifdef _WINDOWS
#include <Windows.h>
#endif

#include <deque>
#include <math.h>
#ifdef ANDROID
#include <cstdint>
#else
#include <stdint.h>
#include <stdlib.h>
#endif
#ifndef PktLossDetector_H
#define PktLossDetector_H

const uint32_t LOCAL_HISTORY_LENGTH_MS = 500;
const int WRAP_AROUND_THRESHOLD = 0xFED3; //allow for missing sequence up to 300 packets when wrap around
const int MAX_RTP_SEQ_NUM = 0xFFFF;
const int MAX_RUN_OLD_PACKETS = 10; //used to detect a glitch in the packet numbering so reset can be preformed

class PktLossDetector;
class PktEntry;
class PktEntryBuffer;
class PktStats;
class PktStatSample;

class PktStatSample{
    public:
        int mNumPktsArrived;
        int mNumPktsDropped;
        int mNumPktsLost;

        PktStatSample() : mNumPktsArrived(0), mNumPktsDropped(0), mNumPktsLost(0) {}

        void reset(){
            mNumPktsArrived=0;
            mNumPktsDropped=0;
            mNumPktsLost=0;
        }
};

class PktStats{
    protected:

        PktStatSample mPktStatSampleList[2];
        int mCurrentSlot;

        int mNumPktsArrived;
        uint32_t mLastTimeIntervalReset;

    public:
        int mNumPktsDropped;
        int mNumPktsLost;

        PktStats(){
            mCurrentSlot = 0;
            mLastTimeIntervalReset = 0L;
            mNumPktsArrived=0;
            mNumPktsDropped=0;
            mNumPktsLost=0;
        }

        void   addPkt(uint32_t aTimeReceived);
        double getPLR();
        double getPDR();
        void   reset();
        void   clearRecentLossHistory(){mPktStatSampleList[mCurrentSlot].mNumPktsLost=0;}
        void   addLostPkts(int aNumLostPkts);
        void   addDroppedPkts(int aNumDroppedPkts);
};

class PktEntry {
    public:
        int  mSeqnum;
        uint32_t mTimeReceived;
        uint32_t mLateness;

        PktEntry(int aSeqnum, uint32_t aTimeReceived, uint32_t aLateness){
            mSeqnum = aSeqnum;
            mTimeReceived = aTimeReceived;
            mLateness = aLateness;
        }
        bool operator < (const PktEntry& another) const
        {
            int theDiff = mSeqnum - another.mSeqnum;
            if(abs(theDiff) > WRAP_AROUND_THRESHOLD){
                //consider this a wrap-around, with or without packet loss, means that bigger number is older
                return (mSeqnum > another.mSeqnum);
            }else{
                return (mSeqnum < another.mSeqnum);
            }
        }
};

class PktEntryBuffer {

    protected:
        int mMaxNumPkts;
        int mOldestAllowedPktSeqnum;
        int mRunOfOldPkts;
    public:
        std::deque<PktEntry> mPktEntryList;


        PktEntryBuffer(int aNumEntries) : mMaxNumPkts(aNumEntries), mOldestAllowedPktSeqnum(-1), mRunOfOldPkts(0) {}

        void reset()
        { mPktEntryList.clear();  mOldestAllowedPktSeqnum = -1; mRunOfOldPkts=0;}

        void setOldestAllowedPktSeqnum(int aOldestAllowedPktSeqnum)
        {mOldestAllowedPktSeqnum = aOldestAllowedPktSeqnum;}

        void addPkt(int aSeqnum, uint32_t aTimeReceived, uint32_t aPktLateness);

        bool hasOnlyOneEntry()
        { return (mPktEntryList.size() == 1) ? true : false; }

        bool hasBeenDroppedAlready(int aSeqnum);

        int  removeOldestPkt(PktStats& aPktStats, uint32_t aMaxJitterLengthMs);

        int  purgePkts(PktStats& aPktStats, uint32_t aMaxJitterLengthMs);


};

class PktLossDetector {

    protected:
        uint32_t mMaxJitterLengthMs;  //If packet is more than x ms late, consider it dropped.
        PktStats mPktStats;
        PktEntryBuffer mPktEntryBuffer;

    public:
        static const char* const LOG_APP;
        static const uint32_t DEFAULT_MAX_JITTER_IN_MS = 100;
        static const int DEFAULT_MAX_INTER_PACKET_SEQUENCE_GAP = 10;

        PktLossDetector() : mMaxJitterLengthMs(DEFAULT_MAX_JITTER_IN_MS), mPktEntryBuffer(DEFAULT_MAX_INTER_PACKET_SEQUENCE_GAP) {}
        PktLossDetector(uint32_t aMaxJitterLengthMs, int aMaxInterPktSequenceGap) :
            mMaxJitterLengthMs(aMaxJitterLengthMs), mPktEntryBuffer(aMaxInterPktSequenceGap) {}

        void reset(){ mPktEntryBuffer.reset(); mPktStats.reset();}

        double getPLR(){return mPktStats.getPLR();}
        double getPDR(){return mPktStats.getPDR();}
        void clearRecentLossHistory(){mPktStats.clearRecentLossHistory();}
        static int calcRTPSeqDiff(int aSeqnumOld, int aSeqnumNew);

        // Add the next packet to the detector.
        // The detector returns the current packet loss estimate (in number of packets)

        int addPkt(int aSeqnum, uint32_t aTimestampRx, uint32_t aPktLateness);
        int getNumLostPkts() { return mPktStats.mNumPktsLost;}
        int getNumDroppedPkts() { return mPktStats.mNumPktsDropped; }
};

#endif //PktLossDetector_H


