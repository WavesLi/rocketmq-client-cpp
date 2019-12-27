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
#ifndef __MQCLIENTFACTORY_H__
#define __MQCLIENTFACTORY_H__

#include <memory>
#include <mutex>
#include <set>

#include "concurrent/executor.hpp"

#include "CommandHeader.h"
#include "FindBrokerResult.h"
#include "HeartbeatData.h"
#include "MQClientException.h"
#include "MQConsumer.h"
#include "MQMessageQueue.h"
#include "MQProducer.h"
#include "ServiceState.h"
#include "TopicPublishInfo.h"
#include "TopicRouteData.h"

namespace rocketmq {

class MQClientAPIImpl;
class MQAdminImpl;
class ClientRemotingProcessor;
class RebalanceService;
class PullMessageService;

class MQClientInstance;
typedef std::shared_ptr<MQClientInstance> MQClientInstancePtr;

class MQClientInstance {
 public:
  MQClientInstance(MQClient* clientConfig, const std::string& clientId);
  MQClientInstance(MQClient* clientConfig, const std::string& clientId, std::shared_ptr<RPCHook> rpcHook);
  virtual ~MQClientInstance();

  static TopicPublishInfoPtr topicRouteData2TopicPublishInfo(const std::string& topic, TopicRouteDataPtr route);
  static std::vector<MQMessageQueue> topicRouteData2TopicSubscribeInfo(const std::string& topic,
                                                                       TopicRouteDataPtr route);

  std::string getClientId();

  void start();
  void shutdown();
  bool isRunning();

  bool registerProducer(const std::string& group, MQProducer* pProducer);
  void unregisterProducer(const std::string& group);

  bool registerConsumer(const std::string& group, MQConsumer* pConsumer);
  void unregisterConsumer(const std::string& group);

  void updateTopicRouteInfoFromNameServer();
  bool updateTopicRouteInfoFromNameServer(const std::string& topic, bool isDefault = false);

  void sendHeartbeatToAllBrokerWithLock();

  void rebalanceImmediately();
  void doRebalance();

  MQProducer* selectProducer(const std::string& group);
  MQConsumer* selectConsumer(const std::string& group);

  FindBrokerResult* findBrokerAddressInAdmin(const std::string& brokerName);
  std::string findBrokerAddressInPublish(const std::string& brokerName);
  FindBrokerResult* findBrokerAddressInSubscribe(const std::string& brokerName, int brokerId, bool onlyThisBroker);

  void findConsumerIds(const std::string& topic, const std::string& group, std::vector<std::string>& cids);

  std::string findBrokerAddrByTopic(const std::string& topic);

  void resetOffset(const std::string& group,
                   const std::string& topic,
                   const std::map<MQMessageQueue, int64_t>& offsetTable);

  ConsumerRunningInfo* consumerRunningInfo(const std::string& consumerGroup);

 public:
  TopicPublishInfoPtr tryToFindTopicPublishInfo(const std::string& topic);

 public:
  MQClientAPIImpl* getMQClientAPIImpl() const { return m_mqClientAPIImpl.get(); }
  MQAdminImpl* getMQAdminImpl() const { return m_mqAdminImpl.get(); }
  PullMessageService* getPullMessageService() const { return m_pullMessageService.get(); }

 private:
  typedef std::map<std::string, std::map<int, std::string>> BrokerAddrMAP;

  void unregisterClient(const std::string& producerGroup, const std::string& consumerGroup);

  void addBrokerToAddrMap(const std::string& brokerName, const std::map<int, std::string>& brokerAddrs);
  void clearBrokerAddrMap();
  BrokerAddrMAP getBrokerAddrMap();

  // scheduled task
  void startScheduledTask();
  void updateTopicRouteInfoPeriodically();
  void sendHeartbeatToAllBrokerPeriodically();
  void persistAllConsumerOffsetPeriodically();

  // topic route
  bool topicRouteDataIsChange(TopicRouteData* old, TopicRouteData* now);
  TopicRouteDataPtr getTopicRouteData(const std::string& topic);
  void addTopicRouteData(const std::string& topic, TopicRouteDataPtr topicRouteData);

  // heartbeat
  void sendHeartbeatToAllBroker();
  HeartbeatData* prepareHeartbeatData();
  void insertConsumerInfoToHeartBeatData(HeartbeatData* pHeartbeatData);
  void insertProducerInfoToHeartBeatData(HeartbeatData* pHeartbeatData);

  // offset
  void persistAllConsumerOffset();

  // rebalance
  void doRebalanceByConsumerGroup(const std::string& consumerGroup);

  // consumer related operation
  bool addConsumerToTable(const std::string& consumerName, MQConsumer* consumer);
  void eraseConsumerFromTable(const std::string& consumerName);
  int getConsumerTableSize();
  void getTopicListFromConsumerSubscription(std::set<std::string>& topicList);
  void updateConsumerTopicSubscribeInfo(const std::string& topic, std::vector<MQMessageQueue> subscribeInfo);

  // producer related operation
  bool addProducerToTable(const std::string& producerName, MQProducer* producer);
  void eraseProducerFromTable(const std::string& producerName);
  int getProducerTableSize();
  void getTopicListFromTopicPublishInfo(std::set<std::string>& topicList);
  void updateProducerTopicPublishInfo(const std::string& topic, TopicPublishInfoPtr publishInfo);

  // topicPublishInfo related operation
  void addTopicInfoToTable(const std::string& topic, TopicPublishInfoPtr pTopicPublishInfo);
  void eraseTopicInfoFromTable(const std::string& topic);
  TopicPublishInfoPtr getTopicPublishInfoFromTable(const std::string& topic);
  bool isTopicInfoValidInTable(const std::string& topic);

 private:
  std::string m_clientId;
  ServiceState m_serviceState;

  // group -> MQProducer
  typedef std::map<std::string, MQProducer*> MQPMAP;
  MQPMAP m_producerTable;
  std::mutex m_producerTableMutex;

  // group -> MQConsumer
  typedef std::map<std::string, MQConsumer*> MQCMAP;
  MQCMAP m_consumerTable;
  std::mutex m_consumerTableMutex;

  // Topic -> TopicRouteData
  typedef std::map<std::string, TopicRouteDataPtr> TRDMAP;
  TRDMAP m_topicRouteTable;
  std::mutex m_topicRouteTableMutex;

  // brokerName -> [ brokerid : addr ]
  BrokerAddrMAP m_brokerAddrTable;
  std::mutex m_brokerAddrlock;

  // topic -> TopicPublishInfo
  typedef std::map<std::string, TopicPublishInfoPtr> TPMap;
  TPMap m_topicPublishInfoTable;
  std::mutex m_topicPublishInfoTableMutex;

  std::timed_mutex m_lockNamesrv;
  std::timed_mutex m_lockHeartbeat;

  std::unique_ptr<MQClientAPIImpl> m_mqClientAPIImpl;
  std::unique_ptr<MQAdminImpl> m_mqAdminImpl;
  std::unique_ptr<ClientRemotingProcessor> m_clientRemotingProcessor;

  std::unique_ptr<RebalanceService> m_rebalanceService;
  std::unique_ptr<PullMessageService> m_pullMessageService;
  scheduled_thread_pool_executor m_scheduledExecutorService;
};

}  // namespace rocketmq

#endif
