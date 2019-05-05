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

#ifndef NETWORK_DELAYCONTROLLER_H
#define NETWORK_DELAYCONTROLLER_H

#include "NetworkSample.h"
#include "NetworkTargetRateController.h"
namespace webrtc {
    class NetworkDelayController : public INetworkRateController, public IEstimatorLogger {
        //--
        static const float MONITOR_DHAT_ALPHA_SNAP;
        static const float MONITOR_DHAT_ALPHA_FAST;
        static const float MONITOR_DHAT_ALPHA_MED;
        static const float MONITOR_DHAT_ALPHA_SLOW;

        static const float MONITOR_DHAT_NQ_DZ;
        static const int   MONITOR_DHAT_NQ_LEN = 18;
        static const float MONITOR_DHAT_NQ_LUT[2*MONITOR_DHAT_NQ_LEN];
        float mDhat_nq;

        //-- delay surge filters:
        AlphaSwitch *mAlphaMinMax, *mAlphaMinMSE, *mAlphaMaxMSE, *mAlphaAvg;
        //--
        VarEstimator *mDhat_n;
        float mDhatMSE_q;

        SimpleIIR *mDhatMSE_ne;
        float mDhatMSE_qe;

        VarEstimator *mDhatMin_n;
        float mDhatMinMSE_q;

        VarEstimator *mDhatMax_n;
        float mDhatMaxMSE_q;

        float mDhatRef_n;
        //--
        void updateSeverityLevelRate(const TargetRateInfo& trInfo);

        //-- "Thx", Threshold Xrossing
        enum { eDelayThxNone, eDelayThxDetect} mDelayThxState;
        void updateThresholdCrossingRate(TargetRateInfo& trInfo);

        //-- convergence data:
        static const int MONITOR_DHAT_CL_WINDOW=300;
        static const float MONITOR_DHAT_CL_SIGMA_THRESH;
        TimeVal mDhatClTime;
        int mDhatMinSigCount, mDhatMaxSigCount;
        int mDhatMinTotCount, mDhatMaxTotCount;
        int mDhatConverged, mDhatConvLock, mDhatConvUnlock;
        void updateConvergence(const NetworkSample& ns, const TargetRateInfo& trInfo);

        //-- outlier data:
        static const int MONITOR_DHAT_NX_SIGMA_SAT=3;
        static const int MONITOR_DHAT_OUTLIER_WINDOW = 10;
        TimeVal mDhatOutlierTime;
        int mDhatOutlier, mDhatOutlierLock;
        void updateOutlier(const NetworkSample& ns, const TargetRateInfo& trInfo);

        public:
        NetworkDelayController(float Rmin = MinMaxRate::MONITOR_MIN_BITRATE_KBPS, float Rmax = MinMaxRate::MONITOR_MAX_BITRATE_KBPS) ;
        ~NetworkDelayController();

        //--
        void processDelay(const NetworkSample&, TargetRateInfo& trInfo);

        //-- INetworkRateController::
        void updateRate(const NetworkSample& ns, TargetRateInfo& trInfo);
        virtual void setRateLimits(float rmin, float rmax) {
            INetworkRateController::setRateLimits(rmin, rmax);
            mRateLimit.adjust(mRateTarget);
        }

        //-- IEstimatorLogger::
        void updateTrace();

        //--
        void setDelayDetectParams(float alphaAvg, float alphaMinMax, float alphaMinMSE, float alphaMaxMSE,
                //-- OBSELETE:
                float alphaRef, float alphaBw) {
            //--
            mAlphaAvg->setAlpha(alphaAvg);
            mAlphaMinMax->setAlpha(alphaMinMax);
            mAlphaMaxMSE->setAlpha(alphaMaxMSE);
            mAlphaMinMSE->setAlpha(alphaMinMSE);
            //-- OBSELETE: mAlphaRef->setAlpha(alphaRef);
            //-- OBSELETE: moved to NetworkSample, mAlphaBw = alphaBw;
        }

    };
}
#endif //NETWORK_DELAYCONTROLLER_H
