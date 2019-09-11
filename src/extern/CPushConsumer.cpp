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

#include "CPushConsumer.h"

#include <map>

#include "CCommon.h"
#include "CMessageExt.h"
#include "DefaultMQPushConsumer.h"
#include "MQClientErrorContainer.h"

using namespace rocketmq;

class MessageListenerInner : public MessageListenerConcurrently {
 public:
  MessageListenerInner() {}

  MessageListenerInner(CPushConsumer* consumer, MessageCallBack pCallback) {
    m_pconsumer = consumer;
    m_pMsgReceiveCallback = pCallback;
  }

  ~MessageListenerInner() {}

  ConsumeStatus consumeMessage(const std::vector<MQMessageExt*>& msgs) {
    // to do user call back
    if (m_pMsgReceiveCallback == nullptr) {
      return RECONSUME_LATER;
    }
    for (size_t i = 0; i < msgs.size(); ++i) {
      CMessageExt* message = (CMessageExt*)msgs[i];
      if (m_pMsgReceiveCallback(m_pconsumer, message) != E_CONSUME_SUCCESS) {
        return RECONSUME_LATER;
      }
    }
    return CONSUME_SUCCESS;
  }

 private:
  MessageCallBack m_pMsgReceiveCallback;
  CPushConsumer* m_pconsumer;
};

class MessageListenerOrderlyInner : public MessageListenerOrderly {
 public:
  MessageListenerOrderlyInner(CPushConsumer* consumer, MessageCallBack pCallback) {
    m_pconsumer = consumer;
    m_pMsgReceiveCallback = pCallback;
  }

  ConsumeStatus consumeMessage(const std::vector<MQMessageExt*>& msgs) {
    if (m_pMsgReceiveCallback == nullptr) {
      return RECONSUME_LATER;
    }
    for (size_t i = 0; i < msgs.size(); ++i) {
      CMessageExt* message = (CMessageExt*)msgs[i];
      if (m_pMsgReceiveCallback(m_pconsumer, message) != E_CONSUME_SUCCESS) {
        return RECONSUME_LATER;
      }
    }
    return CONSUME_SUCCESS;
  }

 private:
  MessageCallBack m_pMsgReceiveCallback;
  CPushConsumer* m_pconsumer;
};

std::map<CPushConsumer*, MessageListenerInner*> g_ListenerMap;
std::map<CPushConsumer*, MessageListenerOrderlyInner*> g_OrderListenerMap;

CPushConsumer* CreatePushConsumer(const char* groupId) {
  if (groupId == NULL) {
    return NULL;
  }
  DefaultMQPushConsumer* defaultMQPushConsumer = new DefaultMQPushConsumer(groupId);
  defaultMQPushConsumer->setConsumeFromWhere(CONSUME_FROM_LAST_OFFSET);
  return (CPushConsumer*)defaultMQPushConsumer;
}

int DestroyPushConsumer(CPushConsumer* consumer) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  delete reinterpret_cast<DefaultMQPushConsumer*>(consumer);
  return OK;
}

int StartPushConsumer(CPushConsumer* consumer) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  try {
    ((DefaultMQPushConsumer*)consumer)->start();
  } catch (std::exception& e) {
    MQClientErrorContainer::setErr(std::string(e.what()));
    return PUSHCONSUMER_START_FAILED;
  }
  return OK;
}

int ShutdownPushConsumer(CPushConsumer* consumer) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->shutdown();
  return OK;
}

int SetPushConsumerGroupID(CPushConsumer* consumer, const char* groupId) {
  if (consumer == NULL || groupId == NULL) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->setGroupName(groupId);
  return OK;
}

const char* GetPushConsumerGroupID(CPushConsumer* consumer) {
  if (consumer == NULL) {
    return NULL;
  }
  return ((DefaultMQPushConsumer*)consumer)->getGroupName().c_str();
}

int SetPushConsumerNameServerAddress(CPushConsumer* consumer, const char* namesrv) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->setNamesrvAddr(namesrv);
  return OK;
}

// Deprecated
int SetPushConsumerNameServerDomain(CPushConsumer* consumer, const char* domain) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  return Not_Support;
}

int Subscribe(CPushConsumer* consumer, const char* topic, const char* expression) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->subscribe(topic, expression);
  return OK;
}

int RegisterMessageCallback(CPushConsumer* consumer, MessageCallBack pCallback) {
  if (consumer == NULL || pCallback == NULL) {
    return NULL_POINTER;
  }
  MessageListenerInner* listenerInner = new MessageListenerInner(consumer, pCallback);
  ((DefaultMQPushConsumer*)consumer)->registerMessageListener(listenerInner);
  g_ListenerMap[consumer] = listenerInner;
  return OK;
}

int RegisterMessageCallbackOrderly(CPushConsumer* consumer, MessageCallBack pCallback) {
  if (consumer == NULL || pCallback == NULL) {
    return NULL_POINTER;
  }
  MessageListenerOrderlyInner* messageListenerOrderlyInner = new MessageListenerOrderlyInner(consumer, pCallback);
  ((DefaultMQPushConsumer*)consumer)->registerMessageListener(messageListenerOrderlyInner);
  g_OrderListenerMap[consumer] = messageListenerOrderlyInner;
  return OK;
}

int UnregisterMessageCallbackOrderly(CPushConsumer* consumer) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  std::map<CPushConsumer*, MessageListenerOrderlyInner*>::iterator iter;
  iter = g_OrderListenerMap.find(consumer);
  if (iter != g_OrderListenerMap.end()) {
    MessageListenerOrderlyInner* listenerInner = iter->second;
    if (listenerInner != NULL) {
      delete listenerInner;
    }
    g_OrderListenerMap.erase(iter);
  }
  return OK;
}

int UnregisterMessageCallback(CPushConsumer* consumer) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  std::map<CPushConsumer*, MessageListenerInner*>::iterator iter;
  iter = g_ListenerMap.find(consumer);

  if (iter != g_ListenerMap.end()) {
    MessageListenerInner* listenerInner = iter->second;
    if (listenerInner != NULL) {
      delete listenerInner;
    }
    g_ListenerMap.erase(iter);
  }
  return OK;
}

int SetPushConsumerMessageModel(CPushConsumer* consumer, CMessageModel messageModel) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->setMessageModel(MessageModel((int)messageModel));
  return OK;
}

int SetPushConsumerThreadCount(CPushConsumer* consumer, int threadCount) {
  if (consumer == NULL || threadCount == 0) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->setConsumeThreadCount(threadCount);
  return OK;
}

int SetPushConsumerMessageBatchMaxSize(CPushConsumer* consumer, int batchSize) {
  if (consumer == NULL || batchSize == 0) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->setConsumeMessageBatchMaxSize(batchSize);
  return OK;
}

int SetPushConsumerMaxCacheMessageSize(CPushConsumer* consumer, int maxCacheSize) {
  if (consumer == NULL || maxCacheSize <= 0) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->setMaxCacheMsgSizePerQueue(maxCacheSize);
  return OK;
}

int SetPushConsumerMaxCacheMessageSizeInMb(CPushConsumer* consumer, int maxCacheSizeInMb) {
  if (consumer == NULL || maxCacheSizeInMb <= 0) {
    return NULL_POINTER;
  }
  return Not_Support;
}
int SetPushConsumerInstanceName(CPushConsumer* consumer, const char* instanceName) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->setInstanceName(instanceName);
  return OK;
}

int SetPushConsumerSessionCredentials(CPushConsumer* consumer,
                                      const char* accessKey,
                                      const char* secretKey,
                                      const char* channel) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  // ((DefaultMQPushConsumer*)consumer)->setSessionCredentials(accessKey, secretKey, channel);
  return OK;
}

int SetPushConsumerLogPath(CPushConsumer* consumer, const char* logPath) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  // Todo, This api should be implemented by core api.
  //((DefaultMQPushConsumer *) consumer)->setInstanceName(instanceName);
  return OK;
}

int SetPushConsumerLogFileNumAndSize(CPushConsumer* consumer, int fileNum, long fileSize) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->setLogFileSizeAndNum(fileNum, fileSize);
  return OK;
}

int SetPushConsumerLogLevel(CPushConsumer* consumer, CLogLevel level) {
  if (consumer == NULL) {
    return NULL_POINTER;
  }
  ((DefaultMQPushConsumer*)consumer)->setLogLevel((elogLevel)level);
  return OK;
}
