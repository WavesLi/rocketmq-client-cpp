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
#include "MQVersion.h"

namespace rocketmq {

const int MQVersion::CURRENT_VERSION = MQVersion::V4_6_0;
const std::string MQVersion::CURRENT_LANGUAGE = "CPP";

const char* MQVersion::GetVersionDesc(int value) {
  int currentVersion = value;
  if (value <= V3_0_0_SNAPSHOT) {
    currentVersion = V3_0_0_SNAPSHOT;
  }
  if (value >= HIGHER_VERSION) {
    currentVersion = HIGHER_VERSION;
  }
  return RocketMQCPPClientVersion[currentVersion];
}

}  // namespace rocketmq
