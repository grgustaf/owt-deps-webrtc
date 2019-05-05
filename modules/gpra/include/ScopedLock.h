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

#ifndef _SCOPED_LOCK_H_
#define _SCOPED_LOCK_H_

#ifdef ANDROID
#include <utils/Thread.h>
typedef android::Mutex ScopedLock;
#else
//#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
//class ScopedLock {
//    webrtc::CriticalSectionWrapper* mutex_;
//    public:
//    ScopedLock() { mutex_ = webrtc::CriticalSectionWrapper::CreateCriticalSection(); }
//    ~ScopedLock() { delete mutex_; }
//    webrtc::CriticalSectionScoped* newLock() { return new webrtc::CriticalSectionScoped(mutex_); }
//    class Autolock {
//        webrtc::CriticalSectionScoped *lock_;
//        public:
//        Autolock(ScopedLock& scopedLock) { lock_ = scopedLock.newLock(); }
//        ~Autolock() { delete lock_; }
//    };
//};
#endif

#endif //_SCOPED_LOCK_H_
