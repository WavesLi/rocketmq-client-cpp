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
#ifndef __MQ_CLIENT_H__
#define __MQ_CLIENT_H__

#include "MQAdmin.h"
#include "MQClientConfig.h"
#include "ServiceState.h"

namespace rocketmq {

class MQClientInstance;

enum elogLevel {
  eLOG_LEVEL_FATAL = 1,
  eLOG_LEVEL_ERROR = 2,
  eLOG_LEVEL_WARN = 3,
  eLOG_LEVEL_INFO = 4,
  eLOG_LEVEL_DEBUG = 5,
  eLOG_LEVEL_TRACE = 6,
  eLOG_LEVEL_LEVEL_NUM = 7
};

class ROCKETMQCLIENT_API MQClient : virtual public MQAdmin, public MQClientConfig {
 public:
  MQClient() : MQClient(nullptr) {}
  MQClient(RPCHookPtr rpcHook) : MQClientConfig(rpcHook), m_serviceState(CREATE_JUST), m_clientFactory(nullptr) {}

  // log configuration interface, default LOG_LEVEL is LOG_LEVEL_INFO, default
  // log file num is 3, each log size is 100M
  void setLogLevel(elogLevel inputLevel);
  elogLevel getLogLevel();
  void setLogFileSizeAndNum(int fileNum, long perFileSize);  // perFileSize is MB unit

  std::vector<MQMessageQueue> getTopicMessageQueueInfo(const std::string& topic);

 public:  // MQAdmin
  void createTopic(const std::string& key, const std::string& newTopic, int queueNum) override;
  int64_t searchOffset(const MQMessageQueue& mq, uint64_t timestamp) override;
  int64_t maxOffset(const MQMessageQueue& mq) override;
  int64_t minOffset(const MQMessageQueue& mq) override;
  int64_t earliestMsgStoreTime(const MQMessageQueue& mq) override;
  MQMessageExtPtr viewMessage(const std::string& offsetMsgId) override;
  QueryResult queryMessage(const std::string& topic,
                           const std::string& key,
                           int maxNum,
                           int64_t begin,
                           int64_t end) override;

 public:
  virtual void start();
  virtual void shutdown();

  MQClientInstance* getFactory() const;
  virtual bool isServiceStateOk();

 protected:
  ServiceState m_serviceState;
  MQClientInstance* m_clientFactory;  // factory
};

}  // namespace rocketmq

#endif  // __MQ_CLIENT_H__
