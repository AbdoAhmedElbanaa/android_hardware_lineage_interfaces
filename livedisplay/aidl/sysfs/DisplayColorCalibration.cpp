/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "DisplayColorCalibrationService"

#include <livedisplay/sysfs/DisplayColorCalibration.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

using ::android::base::ReadFileToString;
using ::android::base::Split;
using ::android::base::StringPrintf;
using ::android::base::Trim;
using ::android::base::WriteStringToFile;

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace sysfs {

namespace {
constexpr const char* kKcalPath = "/sys/devices/platform/kcal_ctrl.0/kcal";
constexpr const char* kKcalMinPath = "/sys/devices/platform/kcal_ctrl.0/kcal_min";
}  // anonymous namespace

ndk::ScopedAStatus DisplayColorCalibration::getMaxValue(int32_t* _aidl_return) {
    *_aidl_return = (this->file_ == kKcalPath) ? 256 : 32768;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DisplayColorCalibration::getMinValue(int32_t* _aidl_return) {
    if (this->file_ == kKcalPath) {
        std::string tmp;
        if (ReadFileToString(kKcalMinPath, &tmp, true)) {
            *_aidl_return = std::stoi(Trim(tmp));
            return ndk::ScopedAStatus::ok();
        }
        *_aidl_return = 20;
        return ndk::ScopedAStatus::ok();
    }
    *_aidl_return = 255;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DisplayColorCalibration::getCalibration(std::vector<int32_t>* _aidl_return) {
    std::string tmp;
    if (!ReadFileToString(this->file_, &tmp, true)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
    }

    std::vector<int32_t> rgb;
    for (auto&& color : Split(Trim(tmp), " ")) {
        if (!color.empty()) {
            rgb.push_back(std::stoi(color));
        }
    }
    *_aidl_return = rgb;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus DisplayColorCalibration::setCalibration(const std::vector<int32_t>& rgb) {
    if (rgb.size() != 3) {
        LOG(ERROR) << "Unrecognized RGB data!";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if (!WriteStringToFile(StringPrintf("%d %d %d", rgb[0], rgb[1], rgb[2]), file_, true)) {
        LOG(ERROR) << "Failed to set DisplayColorCalibration state";
        return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace sysfs
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
