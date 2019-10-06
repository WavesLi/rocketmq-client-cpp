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
#include "MQClientAPIImpl.h"

#include <cassert>
#include <cstring>
#include <typeindex>

#include "AsyncCallbackWrap.h"
#include "ClientRemotingProcessor.h"
#include "MessageBatch.h"
#include "MessageClientIDSetter.h"
#include "PullResultExt.h"
#include "TcpRemotingClient.h"

namespace rocketmq {

MQClientAPIImpl::MQClientAPIImpl(ClientRemotingProcessor* clientRemotingProcessor,
                                 std::shared_ptr<RPCHook> rpcHook,
                                 MQClient* clientConfig)
    : m_remotingClient(new TcpRemotingClient(clientConfig->getTcpTransportWorkerThreadNum(),
                                             clientConfig->getTcpTransportConnectTimeout(),
                                             clientConfig->getTcpTransportTryLockTimeout())) {
  m_remotingClient->registerRPCHook(rpcHook);
  m_remotingClient->registerProcessor(CHECK_TRANSACTION_STATE, clientRemotingProcessor);
  m_remotingClient->registerProcessor(RESET_CONSUMER_CLIENT_OFFSET, clientRemotingProcessor);
  m_remotingClient->registerProcessor(GET_CONSUMER_STATUS_FROM_CLIENT, clientRemotingProcessor);
  m_remotingClient->registerProcessor(GET_CONSUMER_RUNNING_INFO, clientRemotingProcessor);
  m_remotingClient->registerProcessor(NOTIFY_CONSUMER_IDS_CHANGED, clientRemotingProcessor);
  m_remotingClient->registerProcessor(CONSUME_MESSAGE_DIRECTLY, clientRemotingProcessor);
}

MQClientAPIImpl::~MQClientAPIImpl() = default;

void MQClientAPIImpl::start() {
  m_remotingClient->start();
}

void MQClientAPIImpl::shutdown() {
  m_remotingClient->shutdown();
}

void MQClientAPIImpl::updateNameServerAddr(const std::string& addrs) {
  if (m_remotingClient != nullptr) {
    m_remotingClient->updateNameServerAddressList(addrs);
  }
}

void MQClientAPIImpl::createTopic(const std::string& addr, const std::string& defaultTopic, TopicConfig topicConfig) {
  CreateTopicRequestHeader* requestHeader = new CreateTopicRequestHeader();
  requestHeader->topic = topicConfig.getTopicName();
  requestHeader->defaultTopic = defaultTopic;
  requestHeader->readQueueNums = topicConfig.getReadQueueNums();
  requestHeader->writeQueueNums = topicConfig.getWriteQueueNums();
  requestHeader->perm = topicConfig.getPerm();
  requestHeader->topicFilterType = topicConfig.getTopicFilterType();

  RemotingCommand request(UPDATE_AND_CREATE_TOPIC, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      return;
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

SendResult* MQClientAPIImpl::sendMessage(const std::string& addr,
                                         const std::string& brokerName,
                                         const MQMessagePtr msg,
                                         std::unique_ptr<SendMessageRequestHeader> requestHeader,
                                         int timeoutMillis,
                                         CommunicationMode communicationMode,
                                         DefaultMQProducer* producer) {
  return sendMessage(addr, brokerName, msg, std::move(requestHeader), timeoutMillis, communicationMode, nullptr,
                     nullptr, nullptr, 0, producer);
}

SendResult* MQClientAPIImpl::sendMessage(const std::string& addr,
                                         const std::string& brokerName,
                                         const MQMessagePtr msg,
                                         std::unique_ptr<SendMessageRequestHeader> requestHeader,
                                         int timeoutMillis,
                                         CommunicationMode communicationMode,
                                         SendCallback* sendCallback,
                                         TopicPublishInfoPtr topicPublishInfo,
                                         MQClientInstance* instance,
                                         int retryTimesWhenSendFailed,
                                         DefaultMQProducer* producer) {
  int code = SEND_MESSAGE;
  std::unique_ptr<CommandCustomHeader> header;

  if (msg->isBatch()) {
    code = SEND_BATCH_MESSAGE;
    header.reset(SendMessageRequestHeaderV2::createSendMessageRequestHeaderV2(requestHeader.get()));
  } else {
    header = std::move(requestHeader);
  }

  RemotingCommand request(code, header.release());
  request.setBody(msg->getBody());

  switch (communicationMode) {
    case ComMode_ONEWAY:
      m_remotingClient->invokeOneway(addr, request);
      return nullptr;
    case ComMode_ASYNC:
      sendMessageAsync(addr, brokerName, msg, std::move(request), sendCallback, topicPublishInfo, instance,
                       timeoutMillis, retryTimesWhenSendFailed, producer);
      return nullptr;
    case ComMode_SYNC:
      return sendMessageSync(addr, brokerName, msg, request, timeoutMillis);
    default:
      assert(false);
      break;
  }

  return nullptr;
}

void MQClientAPIImpl::sendMessageAsync(const std::string& addr,
                                       const std::string& brokerName,
                                       const MQMessagePtr msg,
                                       RemotingCommand&& request,
                                       SendCallback* sendCallback,
                                       TopicPublishInfoPtr topicPublishInfo,
                                       MQClientInstance* instance,
                                       int64_t timeoutMillis,
                                       int retryTimesWhenSendFailed,
                                       DefaultMQProducer* producer) throw(RemotingException) {
  // delete in future
  auto cbw = new SendCallbackWrap(addr, brokerName, msg, std::forward<RemotingCommand>(request), sendCallback,
                                  topicPublishInfo, instance, retryTimesWhenSendFailed, 0, producer);

  try {
    sendMessageAsyncImpl(cbw, timeoutMillis);
  } catch (RemotingException& e) {
    deleteAndZero(cbw);
    throw e;
  }
}

void MQClientAPIImpl::sendMessageAsyncImpl(SendCallbackWrap* cbw, int64_t timeoutMillis) throw(RemotingException) {
  const auto& addr = cbw->getAddr();
  auto& request = cbw->getRemotingCommand();
  m_remotingClient->invokeAsync(addr, request, cbw, timeoutMillis);
}

SendResult* MQClientAPIImpl::sendMessageSync(const std::string& addr,
                                             const std::string& brokerName,
                                             const MQMessagePtr msg,
                                             RemotingCommand& request,
                                             int timeoutMillis) {
  // block until response
  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  return processSendResponse(brokerName, msg, response.get());
}

SendResult* MQClientAPIImpl::processSendResponse(const std::string& brokerName,
                                                 const MQMessagePtr msg,
                                                 RemotingCommand* response) {
  SendStatus sendStatus = SEND_OK;
  switch (response->getCode()) {
    case FLUSH_DISK_TIMEOUT:
      sendStatus = SEND_FLUSH_DISK_TIMEOUT;
      break;
    case FLUSH_SLAVE_TIMEOUT:
      sendStatus = SEND_FLUSH_SLAVE_TIMEOUT;
      break;
    case SLAVE_NOT_AVAILABLE:
      sendStatus = SEND_SLAVE_NOT_AVAILABLE;
      break;
    case SUCCESS_VALUE:
      sendStatus = SEND_OK;
      break;
    default:
      THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
      return nullptr;
  }

  auto* responseHeader = response->decodeCommandCustomHeader<SendMessageResponseHeader>();
  assert(responseHeader != nullptr);

  MQMessageQueue messageQueue(msg->getTopic(), brokerName, responseHeader->queueId);

  std::string uniqMsgId = MessageClientIDSetter::getUniqID(*msg);

  // MessageBatch
  if (msg->isBatch()) {
    const auto& messages = static_cast<const MessageBatch*>(msg)->getMessages();
    uniqMsgId.clear();
    uniqMsgId.reserve(33 * messages.size());
    for (const auto& message : messages) {
      if (!uniqMsgId.empty()) {
        uniqMsgId.append(",");
      }
      uniqMsgId.append(MessageClientIDSetter::getUniqID(*message));
    }
  }

  SendResult* sendResult =
      new SendResult(sendStatus, uniqMsgId, responseHeader->msgId, messageQueue, responseHeader->queueOffset);
  sendResult->setTransactionId(responseHeader->transactionId);

  return sendResult;
}

PullResult* MQClientAPIImpl::pullMessage(const std::string& addr,
                                         PullMessageRequestHeader* requestHeader,
                                         int timeoutMillis,
                                         CommunicationMode communicationMode,
                                         PullCallback* pullCallback) {
  RemotingCommand request(PULL_MESSAGE, requestHeader);

  switch (communicationMode) {
    case ComMode_ASYNC:
      pullMessageAsync(addr, request, timeoutMillis, pullCallback);
      return nullptr;
    case ComMode_SYNC:
      return pullMessageSync(addr, request, timeoutMillis);
    default:
      assert(false);
      return nullptr;
  }
}

void MQClientAPIImpl::pullMessageAsync(const std::string& addr,
                                       RemotingCommand& request,
                                       int timeoutMillis,
                                       PullCallback* pullCallback) {
  // delete in future
  auto* cbw = new PullCallbackWrap(pullCallback, this);

  try {
    m_remotingClient->invokeAsync(addr, request, cbw, timeoutMillis);
  } catch (RemotingException& e) {
    deleteAndZero(cbw);
    throw e;
  }
}

PullResult* MQClientAPIImpl::pullMessageSync(const std::string& addr, RemotingCommand& request, int timeoutMillis) {
  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  return processPullResponse(response.get());
}

PullResult* MQClientAPIImpl::processPullResponse(RemotingCommand* response) {
  PullStatus pullStatus = NO_NEW_MSG;
  switch (response->getCode()) {
    case SUCCESS_VALUE:
      pullStatus = FOUND;
      break;
    case PULL_NOT_FOUND:
      pullStatus = NO_NEW_MSG;
      break;
    case PULL_RETRY_IMMEDIATELY:
      pullStatus = NO_MATCHED_MSG;
      break;
    case PULL_OFFSET_MOVED:
      pullStatus = OFFSET_ILLEGAL;
      break;
    default:
      THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
  }

  // return of decodeCommandCustomHeader is non-null
  auto* responseHeader = response->decodeCommandCustomHeader<PullMessageResponseHeader>();
  assert(responseHeader != nullptr);

  return new PullResultExt(pullStatus, responseHeader->nextBeginOffset, responseHeader->minOffset,
                           responseHeader->maxOffset, (int)responseHeader->suggestWhichBrokerId, response->getBody());
}

MQMessageExtPtr MQClientAPIImpl::viewMessage(const std::string& addr, int64_t phyoffset, int timeoutMillis) {
  ViewMessageRequestHeader* requestHeader = new ViewMessageRequestHeader();
  requestHeader->offset = phyoffset;

  RemotingCommand request(VIEW_MESSAGE_BY_ID, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      // TODO: ...
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

int64_t MQClientAPIImpl::searchOffset(const std::string& addr,
                                      const std::string& topic,
                                      int queueId,
                                      uint64_t timestamp,
                                      int timeoutMillis) {
  SearchOffsetRequestHeader* requestHeader = new SearchOffsetRequestHeader();
  requestHeader->topic = topic;
  requestHeader->queueId = queueId;
  requestHeader->timestamp = timestamp;

  RemotingCommand request(SEARCH_OFFSET_BY_TIMESTAMP, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      auto* responseHeader = response->decodeCommandCustomHeader<SearchOffsetResponseHeader>();
      assert(responseHeader != nullptr);
      return responseHeader->offset;
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

int64_t MQClientAPIImpl::getMaxOffset(const std::string& addr,
                                      const std::string& topic,
                                      int queueId,
                                      int timeoutMillis) {
  GetMaxOffsetRequestHeader* requestHeader = new GetMaxOffsetRequestHeader();
  requestHeader->topic = topic;
  requestHeader->queueId = queueId;

  RemotingCommand request(GET_MAX_OFFSET, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      auto* responseHeader = response->decodeCommandCustomHeader<GetMaxOffsetResponseHeader>(GET_MAX_OFFSET);
      return responseHeader->offset;
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

int64_t MQClientAPIImpl::getMinOffset(const std::string& addr,
                                      const std::string& topic,
                                      int queueId,
                                      int timeoutMillis) {
  GetMinOffsetRequestHeader* requestHeader = new GetMinOffsetRequestHeader();
  requestHeader->topic = topic;
  requestHeader->queueId = queueId;

  RemotingCommand request(GET_MIN_OFFSET, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      auto* responseHeader = response->decodeCommandCustomHeader<GetMinOffsetResponseHeader>();
      assert(responseHeader != nullptr);
      return responseHeader->offset;
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

int64_t MQClientAPIImpl::getEarliestMsgStoretime(const std::string& addr,
                                                 const std::string& topic,
                                                 int queueId,
                                                 int timeoutMillis) {
  GetEarliestMsgStoretimeRequestHeader* requestHeader = new GetEarliestMsgStoretimeRequestHeader();
  requestHeader->topic = topic;
  requestHeader->queueId = queueId;

  RemotingCommand request(GET_EARLIEST_MSG_STORETIME, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      auto* responseHeader = response->decodeCommandCustomHeader<GetEarliestMsgStoretimeResponseHeader>();
      assert(responseHeader != nullptr);
      return responseHeader->timestamp;
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

void MQClientAPIImpl::getConsumerIdListByGroup(const std::string& addr,
                                               const std::string& consumerGroup,
                                               std::vector<string>& cids,
                                               int timeoutMillis) {
  GetConsumerListByGroupRequestHeader* requestHeader = new GetConsumerListByGroupRequestHeader();
  requestHeader->consumerGroup = consumerGroup;

  RemotingCommand request(GET_CONSUMER_LIST_BY_GROUP, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      auto responseBody = response->getBody();
      if (responseBody != nullptr && responseBody->getSize() > 0) {
        std::unique_ptr<GetConsumerListByGroupResponseBody> body(
            GetConsumerListByGroupResponseBody::Decode(*responseBody));
        cids = body->consumerIdList;
        return;
      }
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

int64_t MQClientAPIImpl::queryConsumerOffset(const std::string& addr,
                                             QueryConsumerOffsetRequestHeader* requestHeader,
                                             int timeoutMillis) {
  RemotingCommand request(QUERY_CONSUMER_OFFSET, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      auto* responseHeader = response->decodeCommandCustomHeader<QueryConsumerOffsetResponseHeader>();
      assert(responseHeader != nullptr);
      return responseHeader->offset;
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

void MQClientAPIImpl::updateConsumerOffset(const std::string& addr,
                                           UpdateConsumerOffsetRequestHeader* requestHeader,
                                           int timeoutMillis) {
  RemotingCommand request(UPDATE_CONSUMER_OFFSET, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      return;
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

void MQClientAPIImpl::updateConsumerOffsetOneway(const std::string& addr,
                                                 UpdateConsumerOffsetRequestHeader* requestHeader,
                                                 int timeoutMillis) {
  RemotingCommand request(UPDATE_CONSUMER_OFFSET, requestHeader);

  m_remotingClient->invokeOneway(addr, request);
}

void MQClientAPIImpl::sendHearbeat(const std::string& addr, HeartbeatData* heartbeatData) {
  RemotingCommand request(HEART_BEAT, nullptr);
  request.setBody(heartbeatData->encode());

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      LOG_INFO("sendheartbeat to broker:%s success", addr.c_str());
      return;
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

void MQClientAPIImpl::unregisterClient(const std::string& addr,
                                       const std::string& clientID,
                                       const std::string& producerGroup,
                                       const std::string& consumerGroup) {
  LOG_INFO("unregisterClient to broker:%s", addr.c_str());
  RemotingCommand request(UNREGISTER_CLIENT, new UnregisterClientRequestHeader(clientID, producerGroup, consumerGroup));

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE:
      LOG_INFO("unregisterClient to:%s success", addr.c_str());
      return;
    default:
      break;
  }

  LOG_WARN("unregisterClient fail:%s, %d", response->getRemark().c_str(), response->getCode());
  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

void MQClientAPIImpl::endTransactionOneway(const std::string& addr,
                                           EndTransactionRequestHeader* requestHeader,
                                           const std::string& remark) {
  RemotingCommand request(END_TRANSACTION, requestHeader);
  request.setRemark(remark);

  m_remotingClient->invokeOneway(addr, request);
}

void MQClientAPIImpl::consumerSendMessageBack(MQMessageExt& msg,
                                              const std::string& consumerGroup,
                                              int delayLevel,
                                              int timeoutMillis) {
  ConsumerSendMsgBackRequestHeader* requestHeader = new ConsumerSendMsgBackRequestHeader();
  // TODO: more fields
  requestHeader->group = consumerGroup;
  requestHeader->offset = msg.getCommitLogOffset();
  requestHeader->delayLevel = delayLevel;

  std::string addr = socketAddress2IPPort(&msg.getStoreHost());
  RemotingCommand request(CONSUMER_SEND_MSG_BACK, requestHeader);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      return;
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

void MQClientAPIImpl::lockBatchMQ(const std::string& addr,
                                  LockBatchRequestBody* requestBody,
                                  std::vector<MQMessageQueue>& mqs,
                                  int timeoutMillis) {
  RemotingCommand request(LOCK_BATCH_MQ, nullptr);
  request.setBody(requestBody->encode());

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      auto requestBody = response->getBody();
      if (requestBody->getSize() > 0) {
        std::unique_ptr<LockBatchResponseBody> body(LockBatchResponseBody::Decode(*requestBody));
        mqs = body->getLockOKMQSet();
      } else {
        mqs.clear();
      }
      return;
    } break;
    default:
      break;
  }

  THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
}

void MQClientAPIImpl::unlockBatchMQ(const std::string& addr,
                                    UnlockBatchRequestBody* requestBody,
                                    int timeoutMillis,
                                    bool oneway) {
  RemotingCommand request(UNLOCK_BATCH_MQ, nullptr);
  request.setBody(requestBody->encode());

  if (oneway) {
    m_remotingClient->invokeOneway(addr, request);
  } else {
    std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(addr, request, timeoutMillis));
    assert(response != nullptr);
    switch (response->getCode()) {
      case SUCCESS_VALUE: {
        return;
      } break;
      default:
        break;
    }

    THROW_MQEXCEPTION(MQBrokerException, response->getRemark(), response->getCode());
  }
}

TopicRouteData* MQClientAPIImpl::getTopicRouteInfoFromNameServer(const std::string& topic, int timeoutMillis) {
  RemotingCommand request(GET_ROUTEINTO_BY_TOPIC, new GetRouteInfoRequestHeader(topic));

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(null, request, timeoutMillis));
  assert(response != nullptr);
  switch (response->getCode()) {
    case TOPIC_NOT_EXIST: {
      break;
    }
    case SUCCESS_VALUE: {
      auto responseBody = response->getBody();
      if (responseBody->getSize() > 0) {
        return TopicRouteData::Decode(*responseBody);
      }
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQClientException, response->getRemark(), response->getCode());
}

TopicList* MQClientAPIImpl::getTopicListFromNameServer() {
  RemotingCommand request(GET_ALL_TOPIC_LIST_FROM_NAMESERVER, nullptr);

  std::unique_ptr<RemotingCommand> response(m_remotingClient->invokeSync(null, request));
  assert(response != nullptr);
  switch (response->getCode()) {
    case SUCCESS_VALUE: {
      auto responseBody = response->getBody();
      if (responseBody->getSize() > 0) {
        return TopicList::Decode(*responseBody);
      }
    }
    default:
      break;
  }

  THROW_MQEXCEPTION(MQClientException, response->getRemark(), response->getCode());
}

int MQClientAPIImpl::wipeWritePermOfBroker(const std::string& namesrvAddr,
                                           const std::string& brokerName,
                                           int timeoutMillis) {
  return 0;
}

void MQClientAPIImpl::deleteTopicInBroker(const std::string& addr, const std::string& topic, int timeoutMillis) {}

void MQClientAPIImpl::deleteTopicInNameServer(const std::string& addr, const std::string& topic, int timeoutMillis) {}

void MQClientAPIImpl::deleteSubscriptionGroup(const std::string& addr,
                                              const std::string& groupName,
                                              int timeoutMillis) {}

std::string MQClientAPIImpl::getKVConfigByValue(const std::string& projectNamespace,
                                                const std::string& projectGroup,
                                                int timeoutMillis) {
  return "";
}

void MQClientAPIImpl::deleteKVConfigByValue(const std::string& projectNamespace,
                                            const std::string& projectGroup,
                                            int timeoutMillis) {}

KVTable MQClientAPIImpl::getKVListByNamespace(const std::string& projectNamespace, int timeoutMillis) {
  return KVTable();
}

}  // namespace rocketmq
