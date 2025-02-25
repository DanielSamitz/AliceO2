// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
#include "Framework/CallbacksPolicy.h"
#include "Framework/CallbackService.h"
#include "Framework/CompletionPolicy.h"
#include "Framework/ServiceRegistryRef.h"
#include "Framework/TimingInfo.h"
#include "Framework/Logger.h"
#include "Framework/CommonServices.h"
#include "Framework/DataTakingContext.h"
#include "Framework/DefaultsHelpers.h"
#include <cstdlib>
#include <uv.h>

// This is to allow C++20 aggregate initialisation
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

namespace o2::framework
{

CallbacksPolicy epnProcessReporting()
{
  return {
    .matcher = [](DeviceSpec const&, ConfigContext const& context) -> bool {
      static bool report = DefaultsHelpers::deploymentMode() == DeploymentMode::OnlineDDS || (getenv("DPL_REPORT_PROCESSING") != nullptr && atoi(getenv("DPL_REPORT_PROCESSING")));
      return report;
    },
    .policy = [](CallbackService& callbacks, InitContext& context) -> void {
      callbacks.set<CallbackService::Id::PreProcessing>([](ServiceRegistryRef registry, int op) {
        auto& info = registry.get<TimingInfo>();
        if ((int)info.firstTForbit != -1) {
          char const* what = info.isTimer() ? "timer" : "timeslice";
          LOGP(info, "Processing {}:{}, tfCounter:{}, firstTForbit:{}, runNumber:{}, creation:{}, action:{}",
               what, info.timeslice, info.tfCounter, info.firstTForbit, info.runNumber, info.creation, op);
        }
        info.lapse = uv_hrtime();
      });
      callbacks.set<CallbackService::Id::PostProcessing>([](ServiceRegistryRef registry, int op) {
        auto& info = registry.get<TimingInfo>();
        if ((int)info.firstTForbit != -1) {
          char const* what = info.isTimer() ? "timer" : "timeslice";
          LOGP(info, "Done processing {}:{}, tfCounter:{}, firstTForbit:{}, runNumber:{}, creation:{}, action:{}, wall:{}",
               what, info.timeslice, info.tfCounter, info.firstTForbit, info.runNumber, info.creation, op, uv_hrtime() - info.lapse);
        }
      });
    }};
}

std::vector<CallbacksPolicy> CallbacksPolicy::createDefaultPolicies()
{
  return {
    epnProcessReporting()};
}

} // namespace o2::framework
