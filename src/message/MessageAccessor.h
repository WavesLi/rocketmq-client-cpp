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
#ifndef __MESSAGE_ACCESSOR_H__
#define __MESSAGE_ACCESSOR_H__

#include "MQMessage.h"

namespace rocketmq {

class MessageAccessor {
 public:
  static inline void clearProperty(MQMessage& msg, const std::string& name) { msg.clearProperty(name); }

  static inline void setProperties(MQMessage& msg, std::map<std::string, std::string>&& properties) {
    msg.setProperties(std::forward<std::map<std::string, std::string>>(properties));
  }

  static inline void putProperty(MQMessage& msg, const std::string& name, const std::string& value) {
    msg.putProperty(name, value);
  }

  static inline const std::string& getReconsumeTime(MQMessage& msg) {
    return msg.getProperty(MQMessageConst::PROPERTY_RECONSUME_TIME);
  }

  static inline const std::string& getMaxReconsumeTimes(MQMessage& msg) {
    return msg.getProperty(MQMessageConst::PROPERTY_MAX_RECONSUME_TIMES);
  }

  static inline void setConsumeStartTimeStamp(MQMessage& msg, const std::string& propertyConsumeStartTimeStamp) {
    putProperty(msg, MQMessageConst::PROPERTY_CONSUME_START_TIMESTAMP, propertyConsumeStartTimeStamp);
  }
};

}  // namespace rocketmq

#endif  // __MESSAGE_ACCESSOR_H__
