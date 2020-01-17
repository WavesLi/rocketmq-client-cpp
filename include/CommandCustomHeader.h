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
#ifndef __COMMAND_CUSTOM_HEADER_H__
#define __COMMAND_CUSTOM_HEADER_H__

#include <map>
#include <string>

#include "RocketMQClient.h"

namespace Json {
class Value;
}

namespace rocketmq {

class ROCKETMQCLIENT_API CommandCustomHeader {
 public:
  virtual ~CommandCustomHeader() = default;

  // write CustomHeader to extFields (map<string,string>)
  virtual void Encode(Json::Value& extFields) {}

  virtual void SetDeclaredFieldOfCommandHeader(std::map<std::string, std::string>& requestMap) {}
};

}  // namespace rocketmq

#endif  // __COMMAND_CUSTOM_HEADER_H__
