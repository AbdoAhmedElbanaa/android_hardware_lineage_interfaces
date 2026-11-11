/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.lineage.livedisplay-impl-sdm"

#include "livedisplay/sdm/PictureAdjustment.h"
#include "livedisplay/sdm/Utils.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <unistd.h>

#include <cmath>

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace sdm {

using ::android::OK;
using ::android::base::ReadFileToString;
using ::android::base::Trim;
using ::android::base::WriteStringToFile;

namespace {
constexpr const char* kKcalEnablePath = "/sys/devices/platform/kcal_ctrl.0/kcal_enable";
constexpr const char* kKcalHuePath = "/sys/devices/platform/kcal_ctrl.0/kcal_hue";
constexpr const char* kKcalSatPath = "/sys/devices/platform/kcal_ctrl.0/kcal_sat";
constexpr const char* kKcalValPath = "/sys/devices/platform/kcal_ctrl.0/kcal_val";
constexpr const char* kKcalContPath = "/sys/devices/platform/kcal_ctrl.0/kcal_cont";

float ReadKcalNode(const char* path, float fallback) {
    std::string tmp;
    if (ReadFileToString(path, &tmp, true)) {
        return std::stof(Trim(tmp));
    }
    return fallback;
}

bool WriteKcalNode(const char* path, float value) {
    int32_t int_val = static_cast<int32_t>(std::lround(value));
    return WriteStringToFile(std::to_string(int_val), path, true);
}
}  // anonymous namespace

PictureAdjustment::PictureAdjustment(std::shared_ptr<SDMController> controller)
    : controller_(controller),
      has_kcal_(access(kKcalHuePath, F_OK) == 0 && access(kKcalSatPath, F_OK) == 0 &&
                access(kKcalValPath, F_OK) == 0 && access(kKcalContPath, F_OK) == 0) {
    if (!isReady()) {
        LOG(FATAL) << "PictureAdjustment backend not ready, exiting.";
    }
    if (has_kcal_) {
        default_pa_ = HSIC{0.0f, 255.0f, 255.0f, 255.0f, 255.0f};
        WriteStringToFile("1", kKcalEnablePath, true);
    } else {
        updateDefaultPictureAdjustment();
    }
}

bool PictureAdjustment::isReady() {
    if (has_kcal_) {
        return true;
    }

    if (!controller_) {
        return false;
    }

    if (utils::CheckFeatureVersion(controller_, utils::FEATURE_VER_SW_PA_API) != OK) {
        return false;
    }

    HsicRanges r{};
    if (controller_->getGlobalPaRange(&r) != OK) {
        return false;
    }

    return r.hue.max != 0 && r.hue.min != 0 && r.saturation.max != 0.f && r.saturation.min != 0.f &&
           r.intensity.max != 0.f && r.intensity.min != 0.f && r.contrast.max != 0.f &&
           r.contrast.min != 0.f;
}

std::optional<HSIC> PictureAdjustment::getPictureAdjustmentInternal() {
    if (has_kcal_) {
        return HSIC{
                ReadKcalNode(kKcalHuePath, 0.0f),
                ReadKcalNode(kKcalSatPath, 255.0f),
                ReadKcalNode(kKcalValPath, 255.0f),
                ReadKcalNode(kKcalContPath, 255.0f),
                255.0f,
        };
    }

    HsicConfig config{};
    if (controller_ && controller_->getGlobalPaConfig(&config) == OK) {
        return HSIC{static_cast<float>(config.data.hue), config.data.saturation,
                    config.data.intensity, config.data.contrast, config.data.saturation_threshold};
    }

    return {};
}

void PictureAdjustment::updateDefaultPictureAdjustment() {
    if (has_kcal_) {
        default_pa_ = HSIC{0.0f, 255.0f, 255.0f, 255.0f, 255.0f};
        return;
    }
    if (auto pa = getPictureAdjustmentInternal()) {
        default_pa_ = *pa;
    }
}

// Methods from ::aidl::vendor::lineage::livedisplay::BnPictureAdjustment follow.
ndk::ScopedAStatus PictureAdjustment::getHueRange(FloatRange* _aidl_return) {
    if (has_kcal_) {
        *_aidl_return = FloatRange{.max = 1536.0f, .min = 0.0f, .step = 1.0f};
        return ndk::ScopedAStatus::ok();
    }

    HsicRanges r{};
    if (!controller_ || controller_->getGlobalPaRange(&r) != OK) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *_aidl_return = FloatRange{
            .max = static_cast<float>(r.hue.max),
            .min = static_cast<float>(r.hue.min),
            .step = static_cast<float>(r.hue.step),
    };

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PictureAdjustment::getSaturationRange(FloatRange* _aidl_return) {
    if (has_kcal_) {
        *_aidl_return = FloatRange{.max = 383.0f, .min = 128.0f, .step = 1.0f};
        return ndk::ScopedAStatus::ok();
    }

    HsicRanges r{};
    if (!controller_ || controller_->getGlobalPaRange(&r) != OK) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *_aidl_return = FloatRange{
            .max = r.saturation.max,
            .min = r.saturation.min,
            .step = r.saturation.step,
    };

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PictureAdjustment::getIntensityRange(FloatRange* _aidl_return) {
    if (has_kcal_) {
        *_aidl_return = FloatRange{.max = 383.0f, .min = 128.0f, .step = 1.0f};
        return ndk::ScopedAStatus::ok();
    }

    HsicRanges r{};
    if (!controller_ || controller_->getGlobalPaRange(&r) != OK) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *_aidl_return = FloatRange{
            .max = r.intensity.max,
            .min = r.intensity.min,
            .step = r.intensity.step,
    };

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PictureAdjustment::getContrastRange(FloatRange* _aidl_return) {
    if (has_kcal_) {
        *_aidl_return = FloatRange{.max = 383.0f, .min = 128.0f, .step = 1.0f};
        return ndk::ScopedAStatus::ok();
    }

    HsicRanges r{};
    if (!controller_ || controller_->getGlobalPaRange(&r) != OK) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *_aidl_return = FloatRange{
            .max = r.contrast.max,
            .min = r.contrast.min,
            .step = r.contrast.step,
    };

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PictureAdjustment::getSaturationThresholdRange(FloatRange* _aidl_return) {
    if (has_kcal_) {
        *_aidl_return = FloatRange{.max = 383.0f, .min = 128.0f, .step = 1.0f};
        return ndk::ScopedAStatus::ok();
    }

    HsicRanges r{};
    if (!controller_ || controller_->getGlobalPaRange(&r) != OK) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *_aidl_return = FloatRange{
            .max = r.saturation_threshold.max,
            .min = r.saturation_threshold.min,
            .step = r.saturation_threshold.step,
    };

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PictureAdjustment::getPictureAdjustment(HSIC* _aidl_return) {
    if (auto pa = getPictureAdjustmentInternal()) {
        *_aidl_return = *pa;
        return ndk::ScopedAStatus::ok();
    }

    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus PictureAdjustment::getDefaultPictureAdjustment(HSIC* _aidl_return) {
    *_aidl_return = default_pa_;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PictureAdjustment::setPictureAdjustment(const HSIC& hsic) {
    if (has_kcal_) {
        WriteStringToFile("1", kKcalEnablePath, true);
        bool ok = WriteKcalNode(kKcalHuePath, std::clamp(hsic.hue, 0.0f, 1536.0f)) &&
                  WriteKcalNode(kKcalSatPath, std::clamp(hsic.saturation, 128.0f, 383.0f)) &&
                  WriteKcalNode(kKcalValPath, std::clamp(hsic.intensity, 128.0f, 383.0f)) &&
                  WriteKcalNode(kKcalContPath, std::clamp(hsic.contrast, 128.0f, 383.0f));
        if (!ok) {
            LOG(ERROR) << "Failed to write KCAL HSIC nodes";
            return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
        }
        return ndk::ScopedAStatus::ok();
    }

    HsicConfig config = {0,
                         {static_cast<int32_t>(hsic.hue), hsic.saturation, hsic.intensity,
                          hsic.contrast, hsic.saturationThreshold}};
    if (!controller_ || controller_->setGlobalPaConfig(&config) != OK) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace sdm
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
