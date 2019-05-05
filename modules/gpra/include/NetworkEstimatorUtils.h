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

#ifndef NETORK_ESTIMATOR_UTILS
#define NETORK_ESTIMATOR_UTILS

#include <string>
#include "TimeSample.h"

class IEstimatorLogger {
    std::string mTraceStr;
    std::string mTitleStr;
    protected:
    virtual void logTitle(const std::string&tit) { mTitleStr = tit; }
    virtual void logTrace(const std::string&trc) { mTraceStr = trc; }
    public:
    IEstimatorLogger() : mTraceStr("nil"), mTitleStr("empty") {}
    virtual ~IEstimatorLogger() {}

    virtual const char *titleStr() { return mTitleStr.c_str(); }
    virtual const char *traceStr() { return mTraceStr.c_str(); }

    virtual void updateTrace() = 0;
};

class AlphaSwitch {
    float mAlpha;
    float mAlphaSlow, mAlphaMed, mAlphaFast;
    public:
    AlphaSwitch(float alphaSlow, float alphaMed=0, float alphaFast=0) : mAlpha(alphaSlow), mAlphaSlow(alphaSlow), mAlphaMed(alphaMed), mAlphaFast(alphaFast) {}
    float select(int isMed=0, int isFast=0) { mAlpha = isFast ? mAlphaFast : isMed ? mAlphaMed : mAlphaSlow; return mAlpha; }
    operator float() { return mAlpha; }
    void setAlpha(float alphaSlow) {
        mAlpha = mAlphaSlow = alphaSlow;
    }
    void setAlpha(float alphaSlow, float alphaMed, float alphaFast) {
        setAlpha(alphaSlow);
        mAlphaMed = alphaMed; mAlphaFast = alphaFast;
    }

};

class SimpleIIR {
    float mReg;
    float mAlpha;
    AlphaSwitch *mAlphaSwitch;
    float mDecayTao;
    TimeVal mDecayTime;
    FloatTickWindow *tickWindow;
    public:
    SimpleIIR(float alpha, float initReg=0, float decayTao=0) : mReg(initReg), mAlpha(alpha), mAlphaSwitch(NULL), mDecayTao(decayTao) {
        tickWindow = new FloatTickWindow();
    }
    SimpleIIR(AlphaSwitch* alphaSwitch, float initReg=0, float decayTao=0) : mReg(initReg), mAlphaSwitch(alphaSwitch), mDecayTao(decayTao) {
        tickWindow = new FloatTickWindow();
        if (mAlphaSwitch) mAlpha = float(*mAlphaSwitch);
    }
    virtual ~SimpleIIR() {}
    operator float() { return mReg; }
    virtual float processSample(float sample);
    virtual float processSample(FloatTimeSample sample);
    virtual float processSampleDecay(FloatTimeSample sample);
    //--
    void initTickWindow(int n, int msTick) { tickWindow->initWindow(n, msTick); }
    const FloatTickWindow& window() const { return *tickWindow; }
};

class VarEstimator {
    SimpleIIR *avgIIR, *mseIIR, *upperIIR, *lowerIIR;
    AlphaSwitch *alphaAvg, *alphaMse;
    float mRootMse; // aka "var"
    FloatTickWindow *varTickWindow;
    protected:
    void initVarWindow(int n, int msTick) { varTickWindow->initWindow(n, msTick); }
    public:
    VarEstimator(float alphaAvg_, float alphaMse_ = -1);
    VarEstimator(AlphaSwitch* alphaAvg_, AlphaSwitch* alphaMse_ = NULL);
    virtual ~VarEstimator();
    //--
    virtual void processSample(FloatTimeSample sample);
    virtual void processSample(float sample);
    //--
    operator float() { return avg(); }
    float avg() const { return float(*avgIIR); }
    float mse() const { return float(*mseIIR); }
    float var() const { return mRootMse; }
#if 1 //-- upper/lower boundary estimation
    virtual float upper() const { return float(*upperIIR); }
    virtual float lower() const { return float(*lowerIIR); }
#endif
    //--
    const FloatTickWindow& avgWindow() const { return avgIIR->window(); }
    const FloatTickWindow& varWindow() const { return *varTickWindow; }
};

class NetworkSample;

// With TransportCC feedback we have the actual packet delay
// hence dont need to have the 100ms offset
#define GOODPUT_100MS_OFFSET 0

class BwEstimator : public VarEstimator {
    public:
        static const int MONITOR_BWEST_WINDOW = 100; //-- 50; // [ms]
        static const int MONITOR_BWEST_TICKS_NUM = 40; // ~4sec back
        static const int MONITOR_BWEST_ERROR = 100; // [ms] -- //-- below a specific threshold the VBW wont meet the required threshold
        static const float MONITOR_BWEST_ALPHA;
        static const float MONITOR_BWEST_ALPHAVAR;
        //--
        static const int MONITOR_BWEST_GOODPUT_THRESH = 100+GOODPUT_100MS_OFFSET; //-- [ms] 200 ms
        static const int MONITOR_BWEST_MAX_THRESH = 100000; //-- [ms] FIXME: ~100 sec ridiculously large value, better use MAXINT
    private:
        TimeVal mRateBwTime;
        int mEstWindow;
        int mLateThresh;
        int mRateBwDelta;

    public:
        BwEstimator(float alphaAvg_ = MONITOR_BWEST_ALPHA, float alphaVar_ = MONITOR_BWEST_ALPHAVAR, int estWindow = MONITOR_BWEST_WINDOW, int lateThresh = MONITOR_BWEST_MAX_THRESH);
        virtual ~BwEstimator() {}
        virtual void processBw(NetworkSample& ns);
        //--
};

#endif // NETORK_ESTIMATOR_UTILS
