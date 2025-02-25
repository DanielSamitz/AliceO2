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
#ifndef O2_FRAMEWORK_DATASENDER_H_
#define O2_FRAMEWORK_DATASENDER_H_

#include "Framework/RoutingIndices.h"
#include "Framework/SendingPolicy.h"
#include "Framework/ServiceRegistryRef.h"
#include "Framework/Tracing.h"
#include "Framework/OutputSpec.h"
#include <fairmq/Message.h>
#include <fairmq/Parts.h>
#include <string>

#include <cstddef>
#include <mutex>

namespace o2::framework
{

struct ServiceRegistry;
struct DeviceSpec;

/// Allow injecting policies on send
class DataSender
{
 public:
  DataSender(ServiceRegistryRef registry,
             SendingPolicy const& policy);
  void send(fair::mq::Parts&, ChannelIndex index);
  std::unique_ptr<fair::mq::Message> create(RouteIndex index);
  /// Reset the datasender to a clean state
  /// so that we can track what whill be sent in the next
  /// iteration and eventually complain if one of the
  /// Lifetime::Timeframe is not create / sent.
  void reset();
  /// Actually verify if all the selected outputs have been sent.
  /// Notice how I think there are a few cases in which we might
  /// have a false positive, since I one could have a completion policy
  /// which creates two required messages in two different iterations.
  void verifyMissingSporadic() const;

 private:
  FairMQDeviceProxy& mProxy;
  // We need the ServiceRegistry and not a ref, to be able
  // to call the callbacks after sending.
  ServiceRegistryRef mRegistry;
  DeviceSpec const& mSpec;
  std::vector<OutputSpec> mOutputs;
  SendingPolicy mPolicy;
  std::vector<size_t> mDistinctRoutesIndex;

  std::vector<std::string> mMetricsNames;
  std::vector<std::string> mVariablesMetricsNames;
  std::vector<bool> mPresent;
  std::vector<bool> mPresentDefaults;

  TracyLockableN(std::recursive_mutex, mMutex, "data relayer mutex");
};

} // namespace o2::framework

#endif // O2_FRAMEWORK_DATASENDER_H_
