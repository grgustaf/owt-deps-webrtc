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

#ifndef NETWORK_SAMPLE_H
#define NETWORK_SAMPLE_H

#include "NetworkEstimatorUtils.h"

#define XABS(__X__) (((__X__) > 0) ? (__X__) : (-(__X__)))

static const int MONITOR_PACKET_OVH = 40;

class NetworkSample : public IEstimatorLogger {
    public:
        NetworkSample();
        ~NetworkSample();

        //--
        void processPacket(bool probeEnabled, const TimeVal& rxTime_, int seqNum_, int payloadSize_, int64_t sent_time, int64_t arrival_time);
        TimeVal rxTime;
        int seqNum;
        int payloadSize;
        int64_t txTs, rxTs;
        int transDelay;
        int mMinTransDel;
        //--
        bool isValidSample;
        float avgTransDelay;

        int median(std::vector<int> &vec);

        //-- IEstimatorLogger
        void updateTrace();

        const BwEstimator *bw() const { return mBwEstimator; }
        const BwEstimator *gp() const { return mGpEstimator; }

    private:
        void rebase(unsigned int txTs_, unsigned int rxTs_);
        bool mRebaseEvent;
        //-- unsigned long mBaseRxTs, mBaseTxTs;
        unsigned long mPrevRxTs, mPrevTxTs;
        long long int mTransMinDelay; //-- min_del

        void updateAvgTransDelay(bool probeEnabled);
        float mAvgTransDelay;
        bool mValidSample;
        int mSumTransDelay, mSumPayload;

        void updateBw();
        BwEstimator *mBwEstimator, *mGpEstimator;
        std::vector<int> mTransDel;
};

#endif // NETWORK_SAMPLE_H
