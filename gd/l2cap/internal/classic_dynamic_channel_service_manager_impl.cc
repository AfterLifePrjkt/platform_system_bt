/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "l2cap/internal/classic_dynamic_channel_service_manager_impl.h"

#include "common/bind.h"
#include "l2cap/internal/classic_dynamic_channel_service_impl.h"
#include "l2cap/psm.h"
#include "os/log.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

void ClassicDynamicChannelServiceManagerImpl::Register(
    Psm psm, ClassicDynamicChannelServiceImpl::PendingRegistration pending_registration) {
  if (!IsPsmValid(psm)) {
    std::unique_ptr<ClassicDynamicChannelService> invalid_service(new ClassicDynamicChannelService());
    pending_registration.user_handler_->Post(common::BindOnce(
        std::move(pending_registration.on_registration_complete_callback_),
        ClassicDynamicChannelManager::RegistrationResult::FAIL_INVALID_SERVICE, std::move(invalid_service)));
  } else if (IsServiceRegistered(psm)) {
    std::unique_ptr<ClassicDynamicChannelService> invalid_service(new ClassicDynamicChannelService());
    pending_registration.user_handler_->Post(common::BindOnce(
        std::move(pending_registration.on_registration_complete_callback_),
        ClassicDynamicChannelManager::RegistrationResult::FAIL_DUPLICATE_SERVICE, std::move(invalid_service)));
  } else {
    service_map_.try_emplace(
        psm, ClassicDynamicChannelServiceImpl(pending_registration.user_handler_,
                                              std::move(pending_registration.on_connection_open_callback_)));
    std::unique_ptr<ClassicDynamicChannelService> user_service(
        new ClassicDynamicChannelService(psm, this, l2cap_layer_handler_));
    pending_registration.user_handler_->Post(
        common::BindOnce(std::move(pending_registration.on_registration_complete_callback_),
                         ClassicDynamicChannelManager::RegistrationResult::SUCCESS, std::move(user_service)));
  }
}

void ClassicDynamicChannelServiceManagerImpl::Unregister(Psm psm,
                                                         ClassicDynamicChannelService::OnUnregisteredCallback callback,
                                                         os::Handler* handler) {
  if (IsServiceRegistered(psm)) {
    service_map_.erase(psm);
    handler->Post(std::move(callback));
  } else {
    LOG_ERROR("service not registered psm:%d", psm);
  }
}

bool ClassicDynamicChannelServiceManagerImpl::IsServiceRegistered(Psm psm) const {
  return service_map_.find(psm) != service_map_.end();
}

ClassicDynamicChannelServiceImpl* ClassicDynamicChannelServiceManagerImpl::GetService(Psm psm) {
  ASSERT(IsServiceRegistered(psm));
  return &service_map_.find(psm)->second;
}

std::vector<std::pair<Psm, ClassicDynamicChannelServiceImpl*>>
ClassicDynamicChannelServiceManagerImpl::GetRegisteredServices() {
  std::vector<std::pair<Psm, ClassicDynamicChannelServiceImpl*>> results;
  for (auto& elem : service_map_) {
    results.emplace_back(elem.first, &elem.second);
  }
  return results;
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth