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
#ifndef __QUERY_RESULT_H__
#define __QUERY_RESULT_H__

#include "MQMessageExt.h"

namespace rocketmq {

class ROCKETMQCLIENT_API QueryResult {
 public:
  QueryResult(uint64_t indexLastUpdateTimestamp, const std::vector<MQMessageExtPtr>& messageList) {
    m_indexLastUpdateTimestamp = indexLastUpdateTimestamp;
    m_messageList = messageList;
  }

  uint64_t getIndexLastUpdateTimestamp() { return m_indexLastUpdateTimestamp; }

  std::vector<MQMessageExtPtr>& getMessageList() { return m_messageList; }

 private:
  uint64_t m_indexLastUpdateTimestamp;
  std::vector<MQMessageExtPtr> m_messageList;
};

}  // namespace rocketmq

#endif  // __QUERY_RESULT_H__
