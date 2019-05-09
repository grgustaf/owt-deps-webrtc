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

#ifndef TIME_SAMPLE_H
#define TIME_SAMPLE_H

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <vector>
#include <limits>

#ifdef ANDROID
#include <sys/time.h>
#else
#include <time.h>
#include <windows.h>
 // Windows
#include "WinSock2.h"
#if 0 // struct already defined in Windows SDK
struct timeval {
  long tv_sec;
  long tv_usec;
};
#endif
struct timezone
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};
#if defined(_MSC_VER) || defined(__BORLANDC__)
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif
#endif //ANDROID


class TimeVal {
#ifdef SIM
    static struct timeval mSimTime;
#endif
    struct timeval mTv;
    public:
    TimeVal() { mTv.tv_sec = 0; mTv.tv_usec = 0; }

    void getTimeOfDay() {
#ifdef SIM
        mTv = mSimTime;
#else
#ifdef ANDROID
      gettimeofday(&mTv, NULL);
#else
      FILETIME        ft;
      LARGE_INTEGER   li;
      __int64         t;
      //static int      tzflag;  //Jianlin: commenting out for un-used variable warning

      //if (&mTv)      //Jianlin: commenting out as this always evaluate to true, avoid build warnings.
      {
        GetSystemTimeAsFileTime(&ft);
        li.LowPart = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        t = li.QuadPart;       /* In 100-nanosecond intervals */
        t -= EPOCHFILETIME;     /* Offset to the Epoch time */
        t /= 10;                /* In microseconds */
        mTv.tv_sec = (long)(t / 1000000);
        mTv.tv_usec = (long)(t % 1000000);
      }
#endif //ANDROID
#endif //SIM
    }
    unsigned int timeStamp() const { return 1000 * (mTv.tv_sec) + (mTv.tv_usec)/1000; }
    //-- diff to past-ref
    int diff(const TimeVal& pastRef) const { return (mTv.tv_sec - pastRef.mTv.tv_sec)*1000.0 + (mTv.tv_usec - pastRef.mTv.tv_usec)/1000.0; }
#ifdef SIM
    void setTimestamp(unsigned int rxTs) {
        mTv.tv_sec = rxTs/1000;
        mTv.tv_usec = (rxTs % 1000) * 1000;
#ifdef SIM
        mSimTime = mTv;
#endif
    }
#endif
};

template <class T> class TimeSample : public TimeVal {
    T mSample; //FIXME: templated
    public:
    TimeSample() { mSample = T(0); }
    //-- TimeSample(T sample) { mSample = sample; }
    TimeSample(const TimeVal& tv, const T& sample) : TimeVal(tv), mSample(sample) {}
    TimeSample(const TimeSample<T>& ts)  : TimeVal(ts), mSample(ts.mSample) {}

    operator T() const { return mSample; }
    const T& val() { return mSample; }
    const T& operator =(const T& sample) { mSample = sample; return sample; }
};
typedef TimeSample<float> FloatTimeSample;

template <class T> class TickWindow {
    public:
        static const int MONITOR_IIR_TICK_PERIOD=100; // [ms]
        static const int MONITOR_IIR_TICK_NUM = 10; // ~1sec = 10x100ms
    private:
        TimeSample<T> *mTickVec;
        TimeSample<T> mTickSample;
        int mNum, mHead;
        int mTickPeriod;
        int mTickLen, mTickCnt;
        T mMedSample, mAvgSample, mMinSample, mMaxSample;
        //--
        static const int MONITOR_TICK_LOOKAHEAD=4;
        T *mMinLookAhead;
        T mTopAscentLookAhead; //-- Largest(mMinLookAhead[:])
        void updateMinLookAhead(const T&val);
    public:
        TickWindow(int n = MONITOR_IIR_TICK_NUM, int msTick = MONITOR_IIR_TICK_PERIOD) {
            mTickVec = NULL;
            mMinLookAhead = NULL;
            initWindow(n, msTick);
        }
        ~TickWindow() {
            delete[] mTickVec;
            delete mMinLookAhead;
        }
        //--
        void initWindow(int n, int msTick) {
            mNum = n;
            mHead = 0;
            if (mTickVec) {
                delete[] mTickVec; // FIXME: vector resize
                delete mMinLookAhead;
            }
            mTickVec = new TimeSample<T>[n];
            mMinLookAhead = new T[n];
            for(int i=0; i<n; i++) { mTickVec[i] = T(0), mMinLookAhead[i] = T(0); }
            mTickPeriod = msTick;
            mTickLen = mTickCnt = 0;
            mMedSample = mMinSample = mMaxSample = mAvgSample = 0;
        }
        //--
        bool push(const TimeSample<T>& ts);
        void restat();
#ifndef STD_VEC
        static int t_cmp(const void *, const void *);
#endif
        const T tw_min() const { return mMinSample; }
        const T tw_max() const { return mMaxSample; }
        const T med() const { return mMedSample; }
        const T avg() const { return mAvgSample; }
        int ticks() { return mTickCnt; }
        const T top() const { return mTopAscentLookAhead; }
};

typedef TickWindow<float> FloatTickWindow ;


template<class T>
bool TickWindow<T>::push(const TimeSample<T>& ts) {
    bool tick=false;
    if (!mTickSample.timeStamp()) mTickSample = ts;
    if (ts.diff(mTickSample) > mTickPeriod) {
        tick = true;
        mTickSample = T(mTickSample)/mTickLen;
        updateMinLookAhead(mMinLookAhead[mHead] = T(mTickSample));
        mTickVec[mHead++] = mTickSample;
        mHead %= mNum;
        //--
        mTickSample = ts;
        mTickLen = 1;
        mTickCnt++;
        //--
        restat();
    } else {
        mTickSample = T(mTickSample) + T(ts);
        mTickLen++;
    }
    return tick;
}

template<class T>
int TickWindow<T>::t_cmp(const void *a, const void *b) {
    T arg1 = *(reinterpret_cast<const T*>(a));
    T arg2 = *(reinterpret_cast<const T*>(b));
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

template<class T>
void TickWindow<T>::restat() {
#ifdef STD_VEC
    std::vector<T> *vec = new std::vector<T>(mNum);
#endif
    mAvgSample = 0;
    mMinSample = std::numeric_limits<T>::max();
    mMaxSample = -(std::numeric_limits<T>::max()); //-- FIXME: overflow for int, lowest() in c++11
    mTopAscentLookAhead = -(std::numeric_limits<T>::max());
    T* t_vec = new T[mNum];
    int t_num = mNum;
    if (mTickCnt < mNum) t_num = mTickCnt;
    for(int i=0; i<t_num; i++) {
        T sample = mTickVec[i];
        t_vec[i] = sample;
        mAvgSample += sample;
#ifdef STD_VEC
        (*vec)[i] = sample; //-- ->push_back(T(mTickVec[i]));
#endif
        if (sample < mMinSample) mMinSample = sample;
        if (sample > mMaxSample) mMaxSample = sample;

        if (mMinLookAhead[i] >= sample && mTopAscentLookAhead < sample) mTopAscentLookAhead = sample;
    }
    mAvgSample /= t_num;
#ifdef STD_VEC
    std::sort(vec->begin(), vec->end());
    mMedSample = (*vec)[t_num/2];
#else
    qsort(t_vec, t_num, sizeof(T), t_cmp);
    mMedSample = t_vec[t_num/2];
#endif
    //-- delete t_vec;
    delete[] t_vec;
}

template <class T>
void TickWindow<T>::updateMinLookAhead(const T& val) {
    int nlka = MONITOR_TICK_LOOKAHEAD;
    if (nlka > mTickCnt) nlka = mTickCnt;
    int hd = mHead - nlka; //most recent
    if (hd < 0) hd += mNum;
    for(int i=0; i<nlka; i++) {
        if (mMinLookAhead[hd] > val) mMinLookAhead[hd] = val;
        hd = (hd+1)%mNum;
    }
}

#endif // TIME_SAMPLE_H
