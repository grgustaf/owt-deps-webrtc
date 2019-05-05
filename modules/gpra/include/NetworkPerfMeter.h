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

#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <stdio.h>
#include <time.h>
#include <fstream>
#include <vector>

//-- webrtc:
//#include "webrtc/modules/include/module.h"
//#include "webrtc/modules/include/module_common_types.h"

//-- sim
#include "ScopedLock.h"

//-- ims:
#include "NetworkSample.h"
#include "NetworkLossController.h"
#include "NetworkDelayController.h"
#include "NetworkTargetRateController.h"
#include "NetworkEstimatorUtils.h"
#include "webrtc_defs.h"

//#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
//#include "webrtc/modules/congestion_controller/delay_based_bwe.h"

namespace webrtc {

    struct NetworkChangedStats {
        unsigned int otherEstimateBps;
        unsigned int bitrateBps;
        unsigned int fractionLost;
        unsigned int roundTripTimeMs;
    };

    class NetworkPerfMeter  : public IEstimatorLogger/*, public CallStatsObserver, public Module*/ {
        NetworkSample mSample;
        NetworkLossController mLossController;
        NetworkDelayController mDelayController;
        NetworkTargetRateController mTargetRateController;

        NetworkChangedStats mStats;

        std::ofstream mLogFile;

        bool mEnableLogging;

        //ScopedLock mLogLock;
        void monitorTraces(); // aggregated updateTrace
        void syncMonitorStats(); // sync'ed monitorTraces & save to file

        int mLastEstimatedBitrate;  // used to ensure we only send updated bps
        int64_t mCurrOffsetMs;      // current ref clock offset used by TxCC feedback
        int32_t mDeltaOffsetMs;     // delta due to ref clock diff between Rx and Tx

        public:
        NetworkPerfMeter(bool enableLogging = false);
        ~NetworkPerfMeter();
        //--
        void processTs(int seqNum, int payloadSize, int64_t sent_time, int64_t arrival_time);
        //-- RemoteBitrateEstimatorSingleStream::
        void IncomingPacket(unsigned int ssrc,
                uint16_t sequenceNumber,
                int payload_size,
                int64_t arrival_time,
                int64_t sent_time) {
            processTs(sequenceNumber, payload_size, sent_time, arrival_time);
        }

        void SetMaxBitrate(unsigned int bitrate) {
            if (bitrate) {
              mTargetRateController.SetMaxBitrate(bitrate);
              // Add packet overhead rate and 5% less packet utilization back as RA
              // takes packet overhead into consideration during bitrate estimation
              // Add packet overhead rate = num_packets * per packet OH in bps
              bitrate += (((bitrate*1000)/(1500*8))*MONITOR_PACKET_OVH*8)/1000;
              // Add 5% less packet utilization back
              bitrate = bitrate * 1.05;
              // Update rate limits maximum, setRateLimits expects value in bps
              setRateLimits(MinMaxRate::MONITOR_MIN_BITRATE_KBPS*1000, bitrate*1000);
            }
        }

        void OtherEstimate(unsigned int bitrate_bps) {
            mStats.otherEstimateBps = bitrate_bps;
        }
        bool ValidEstimate() const { return getRateTrig() != 0; }
        bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                unsigned int* bitrate_bps) const {
            if (getRateTrig()) {
                //-- estimateRateKbit() is non-const... need to push break-down to Process/TimeUntilNextProcess
                *bitrate_bps = mTargetRateController.getRateGrad() * 1000;
                //-- clrRateTrig(); is non-const ...
                return true;
            }
            return false;
        }

        // Define similar calls like DelayBasedBwe
        webrtc::DelayBasedBwe::Result IncomingPacketFeedbackVector(
                                const std::vector<webrtc::PacketFeedback>& packet_feedback_vector);
        void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms);
        void SetStartBitrate(int start_bitrate_bps);
        void SetMinBitrate(int min_bitrate_bps);
        int64_t GetProbingIntervalMs() const;
        void SetCurrentOffsetMs(int64_t offset_ms);
        void SetBitrateEstimatorWindowSize(int init_window, int rate_window);
        //-- IEstimatorLogger::
        void updateTrace();
        //-- CallStatsObserver::
        void OnRttUpdate(int64_t rtt_ms) { mStats.roundTripTimeMs = rtt_ms; }
        //-- Module:: -- ?? --
        void Process() {
            estimateRateKbit();
            //-- derive next processing steps
        }
        int64_t TimeUntilNextProcess() {
            //-- arbitrary
            return 5;
        }

        //--
        void setRateLimits(float rmin, float rmax) {
            mDelayController.setRateLimits(rmin, rmax);
            mLossController.setRateLimits(rmin, rmax);
            mTargetRateController.setRateLimits(rmin, rmax);
        }
        //-- Loss params
        void setPacketLossParams(float plrThresh) {
            mLossController.setPacketLossParams(plrThresh);
        }

        void setRateLimitParams(int msIterDelay, float percDecStep, float percIncStep, float kbpsProbeStep, float percProbeThresh) {
            mLossController.setRateLimitParams(msIterDelay, percDecStep, percIncStep, kbpsProbeStep, percProbeThresh);
        }
        //-- Delay params
        void setDelayDetectParams(float alphaAvg, float alphaMinMax, float alphaMinMSE, float alphaMaxMSE, float alphaRef, float alphaBw) {
            mDelayController.setDelayDetectParams(alphaAvg, alphaMinMax, alphaMinMSE, alphaMaxMSE, alphaRef, alphaBw);
        }

        void setRateStepParams(int msIterDelay, float percDecStep, float percIncStep) {
            mDelayController.setRateStepParams(msIterDelay, percDecStep, percIncStep);
        }

        //--
        int getRateTrig() const { return mTargetRateController.getRateTrig(); }
        void clrRateTrig() { mTargetRateController.clrRateTrig(); }

        //--
        int estimateRateKbit();
        void updateStats(const unsigned int bitrateBps,
                const unsigned int fractionLost,
                const unsigned int roundTripTimeMs);
    };
}

#endif // NETWORK_MONITOR_H
