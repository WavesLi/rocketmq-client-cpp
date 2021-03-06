/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef ROCKETMQ_PRODUCER_DEFAULTMQPRODUCERCONFIGIMPL_HPP_
#define ROCKETMQ_PRODUCER_DEFAULTMQPRODUCERCONFIGIMPL_HPP_

#include <algorithm>  // std::min, std::max

#include "DefaultMQProducerConfig.h"
#include "MQClientConfigImpl.hpp"

namespace rocketmq {

/**
 * DefaultMQProducerConfigImpl - implement for DefaultMQProducerConfig
 */
class DefaultMQProducerConfigImpl : virtual public DefaultMQProducerConfig, public MQClientConfigImpl {
 public:
  DefaultMQProducerConfigImpl()
      : max_message_size_(1024 * 1024 * 4),         // 4MB
        compress_msg_body_over_howmuch_(1024 * 4),  // 4KB
        compress_level_(5),
        send_msg_timeout_(3000),
        retry_times_(2),
        retry_times_for_async_(2),
        retry_another_broker_when_not_store_ok_(false) {}

  virtual ~DefaultMQProducerConfigImpl() = default;

  int max_message_size() const override { return max_message_size_; }
  void set_max_message_size(int maxMessageSize) override { max_message_size_ = maxMessageSize; }

  int compress_msg_body_over_howmuch() const override { return compress_msg_body_over_howmuch_; }
  void set_compress_msg_body_over_howmuch(int compressMsgBodyOverHowmuch) override {
    compress_msg_body_over_howmuch_ = compressMsgBodyOverHowmuch;
  }

  int compress_level() const override { return compress_level_; }
  void set_compress_level(int compressLevel) override {
    if ((compressLevel >= 0 && compressLevel <= 9) || compressLevel == -1) {
      compress_level_ = compressLevel;
    }
  }

  int send_msg_timeout() const override { return send_msg_timeout_; }
  void set_send_msg_timeout(int sendMsgTimeout) override { send_msg_timeout_ = sendMsgTimeout; }

  int retry_times() const override { return retry_times_; }
  void set_retry_times(int times) override { retry_times_ = std::min(std::max(0, times), 15); }

  int retry_times_for_async() const override { return retry_times_for_async_; }
  void set_retry_times_for_async(int times) override { retry_times_for_async_ = std::min(std::max(0, times), 15); }

  bool retry_another_broker_when_not_store_ok() const override { return retry_another_broker_when_not_store_ok_; }
  void set_retry_another_broker_when_not_store_ok(bool retryAnotherBrokerWhenNotStoreOK) override {
    retry_another_broker_when_not_store_ok_ = retryAnotherBrokerWhenNotStoreOK;
  }

 protected:
  int max_message_size_;                // default: 4 MB
  int compress_msg_body_over_howmuch_;  // default: 4 KB
  int compress_level_;
  int send_msg_timeout_;
  int retry_times_;
  int retry_times_for_async_;
  bool retry_another_broker_when_not_store_ok_;
};

}  // namespace rocketmq

#endif  // ROCKETMQ_PRODUCER_DEFAULTMQPRODUCERCONFIGIMPL_HPP_
