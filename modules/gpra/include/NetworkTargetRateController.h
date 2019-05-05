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

#ifndef NETWORK_TARGETRATECTL_H
#define NETWORK_TARGETRATECTL_H

#include "NetworkEstimatorUtils.h"
//#include "webrtc/base/logging.h"
struct MinMaxRate {
#ifdef CFG_3G //FIXME: should not be compile option - must be passed from app...
    static const int MONITOR_MAX_BITRATE_KBPS=1000; //-- 725@240p //max send bitrate in BandwidthManagement(kbits)
#else
    static const int MONITOR_MAX_BITRATE_KBPS = 20 * 1000;//6000; //-- 6000@720p //max send bitrate in BandwidthManagement(kbits)
#endif
    static const int MONITOR_MIN_BITRATE_KBPS=64; //max send bitrate in BandwidthManagement(kbits)

    static const int MONITOR_RATE_DEADZONE_KBPS=10; // kbps
    float rMin, rMax;
    MinMaxRate() : rMin(MONITOR_MIN_BITRATE_KBPS), rMax(MONITOR_MAX_BITRATE_KBPS) {}
    MinMaxRate(float _rMin, float _rMax) : rMin(_rMin), rMax(_rMax) {
        if (rMin < 0) rMin = MONITOR_MIN_BITRATE_KBPS; // for scenarios when only max is being set
        if (rMax < 0) rMax = MONITOR_MAX_BITRATE_KBPS; // for scenarios when only min is being set
        if (rMin < MONITOR_MIN_BITRATE_KBPS) rMin = MONITOR_MIN_BITRATE_KBPS;
        if (rMax < 4 * MONITOR_MIN_BITRATE_KBPS) rMax = 4 * MONITOR_MIN_BITRATE_KBPS; //~ 250kbps as a minimum
    }
    void adjust(float& r) const {
        if (r < rMin) r = rMin;
        if (r > rMax) r = rMax;
    }
    float interp(float fx) const {
        return rMin + (rMax-rMin)*fx;
    }
    void scale(float fx) {
        rMax *= fx;
        rMin *= fx;
    }
    bool inRange(float r, float dz=MONITOR_RATE_DEADZONE_KBPS) const {
        if (r >= rMin-dz && r <= rMax+dz) return true;
        return false;
    }
};

struct TargetRateInfo {
    //-- generated on copy from TargetRateController
    TimeVal now;
    bool isProbeEnabled;
    bool isProbeExtEnabled;
    //--
    TimeVal rateUpdateTime;
    float gradRate;

    //-- updated in-process pipeline stages
    MinMaxRate limit;
    float estBw;
    float trigDelay;
    float targetRate;
    bool lossFreeze;

    void limitTarget() { limit.adjust(targetRate); }
    TargetRateInfo() : isProbeEnabled(0), isProbeExtEnabled(0), gradRate(MinMaxRate::MONITOR_MIN_BITRATE_KBPS), estBw(0), trigDelay(0), targetRate(MinMaxRate::MONITOR_MIN_BITRATE_KBPS) {}
};

class INetworkRateController {
    protected:
        MinMaxRate mRateLimit;
        float mRateTarget;

        //-- rate-recovery estimator data:
        static const int MONITOR_RATE_ITER_DELAY = 200;
        static const float MONITOR_RATE_DEC_STEP;
        static const float MONITOR_RATE_INC_STEP;
        float mRateStepDec, mRateStepInc;
        int mRateStepIterDelay;
    public:
        INetworkRateController(float Rmin = MinMaxRate::MONITOR_MIN_BITRATE_KBPS, float Rmax = MinMaxRate::MONITOR_MAX_BITRATE_KBPS) : mRateLimit(Rmin, Rmax) {}
        virtual ~INetworkRateController() {}
        virtual void updateRate(const NetworkSample& ns, TargetRateInfo& trInfo) = 0;

        //-- @params: rmin, rmax in [bps], common to webRTC
        //-- @description: update band limit as [Kbps]
        virtual void setRateLimits(float rmin, float rmax) {
            mRateLimit = MinMaxRate(rmin/1000, rmax/1000);
        }
        //--
        virtual void setRateStepParams(int msIterDelay, float percDecStep, float percIncStep) {
            mRateStepIterDelay = msIterDelay;
            mRateStepDec = percDecStep;
            mRateStepInc = percIncStep;
        }

};

class NetworkTargetRateController : public INetworkRateController, public IEstimatorLogger {
    static const int MONITOR_INITIAL_PROBE_DELAY=3000; // [ms]
    static const int MONITOR_INITIAL_PROBE_EXT_DELAY=2000; // [ms]
    static const int MONITOR_INITIAL_PROBE_KBPS = 10 * 1000;  //1500;  //ALCH try start from above to stay at high bitrate.
    int mInitialProbeDelay;
    int mInitialProbeRate;
    int mRateProbeEnabled, mRateProbeExtEnabled;
    TimeVal mRateProbeTime;

    static const int MONITOR_RATE_TRIGDELAY_FX=3; //-- rate trig update delay multiplier
    static const int MONITOR_RATE_TRIGDELAY_MIN=50; //200; // rate trig update delay limit
    static const int MONITOR_RATE_TRIGDELAY_MAX=400; // rate trig update delay limit
    float mRateUpdateGrad;
    int mRateUpdateDelay;
    TimeVal mRateUpdateTime;

    int mRateTrig;
    TargetRateInfo mRateInfo;
    public:
    NetworkTargetRateController(float Rmin = MinMaxRate::MONITOR_MIN_BITRATE_KBPS, float Rmax = MinMaxRate::MONITOR_MAX_BITRATE_KBPS, int initProbeDelay = MONITOR_INITIAL_PROBE_DELAY, int initProbeKbps = MONITOR_INITIAL_PROBE_KBPS);
    //--
    TargetRateInfo targetRateInfo(TimeVal& now);
    //-- INetworkRateController::
    void updateRate(const NetworkSample& ns, TargetRateInfo& trInfo);
    //-- IEstimatorLogger::
    void updateTrace();

    // Set initial bitrate to max bitrate
    void SetMaxBitrate(unsigned int bitrate) {
        mRateUpdateGrad = bitrate;
    }

    //--
    void estimateRate();
    int getRateGrad() const { return mRateUpdateGrad; }
    int getRateTrig() const { return mRateTrig; }
    void clrRateTrig() { mRateTrig = 0; }
};

#endif //NETWORK_TARGETRATECTL_H
