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

#ifndef NETWORK_LOSSCONTROLLER_H
#define NETWORK_LOSSCONTROLLER_H

#include "NetworkSample.h"
#include "NetworkTargetRateController.h"
#include "PktLossDetector.h"
#include "TimeSample.h"

namespace webrtc {

    class NetworkLossController : public INetworkRateController, public IEstimatorLogger {
        static const int MONITOR_LOSS_DETECT_LEN = 20;
        static const int MONITOR_LOSS_DETECT_THRESH = 5;
        int mLossDetect[MONITOR_LOSS_DETECT_LEN];

        int mPrevSeqNum, mLossDetectSum, mLossDetectIdx;
        int mLossDetectFreeze;

        //-- IFOX: LossDetector
        static const float MONITOR_PLR_DET_THRESH;
        //#ifdef CFG_3G
        //#else
        //  static const float MONITOR_PLR_DET_THRESH = 0.015;
        //#endif

        static const int MONITOR_PLR_DET_DELAY_WINDOW = 100;
        static const int MONITOR_PLR_DET_SNIPG = 10;
        float mPlrThresh;
        float mPlrCnt, mPlrRef;

        float mPacketLossRateFloor, mPacketLossRateLimit, mPacketLossRateRecover;
        float mGpEfficiency;
        PktLossDetector *mPktLossDetector_p;

        static const int MONITOR_RATE_LIMIT_ITER_DELAY = 400;
        static const float MONITOR_RATE_LIMIT_DEC_STEP;
        static const float MONITOR_RATE_LIMIT_INC_STEP;
        //-- inherited from INetworkRateController:
        //-- float mRateLimitDec, mRateLimitInc;
        //-- int mRateLimitIterDelay;

        static const int MONITOR_RATE_LIMIT_TRACK_DELAY = 300;
        static const int MONITOR_RATE_LIMIT_PROBE_DELAY = 1000;
        static const int MONITOR_RATE_LIMIT_LOSS_DELAY = 3000;
        static const float MONITOR_RATE_LIMIT_PROBE_KBPS;
        static const float MONITOR_RATE_LIMIT_PROBE_THRESH;
        TimeVal mLossTime, mTrackingTime, mProbeTime ;
        int mRateLimitProbeDelay;
        float mRateLimitProbeKbps;
        float mRateLimitProbeThresh;
        void detectLossRate(const NetworkSample&, TargetRateInfo& trInfo);
        void recoverRate(const NetworkSample&, TargetRateInfo& trInfo);

        enum { eLossNone, eLossDetect, eLossRecover, eLossProbe } mLossState;
        public:
        NetworkLossController(float Rmin = MinMaxRate::MONITOR_MIN_BITRATE_KBPS, float Rmax = MinMaxRate::MONITOR_MAX_BITRATE_KBPS);
        ~NetworkLossController();

        void processLoss(const NetworkSample& ns, TargetRateInfo& trInfo);

        //-- INetworkRateController::
        void updateRate(const NetworkSample& ns, TargetRateInfo& trInfo);
        //--
        virtual void setRateLimits(float rmin, float rmax) {
            INetworkRateController::setRateLimits(rmin, rmax);
            mRateLimit.adjust(mPacketLossRateRecover);
            mRateLimit.adjust(mPacketLossRateLimit);
            mRateLimit.adjust(mPacketLossRateFloor);
        }

        //-- IEstimatorLogger::
        void updateTrace();

        //-- config params
        void setPacketLossParams(float plrThresh) {
            mPlrThresh = plrThresh;
        }

        void setRateLimitParams(int msIterDelay, float percDecStep, float percIncStep, float kbpsProbeStep, float percProbeThresh) {
            //-- inherited from INetworkRateController
            setRateStepParams(msIterDelay, percDecStep, percIncStep);
            //--
            mRateLimitProbeKbps = kbpsProbeStep;
            mRateLimitProbeThresh = percProbeThresh;
        }


    };

}
#endif //NETWORK_LOSSCONTROLLER_H
