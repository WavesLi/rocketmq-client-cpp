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

#ifndef __CONSUME_MESSAGE_SERVICE_H__
#define __CONSUME_MESSAGE_SERVICE_H__

#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/thread.hpp>

#include "concurrent/executor.hpp"

#include "Logging.h"
#include "MQMessageListener.h"
#include "PullRequest.h"

namespace rocketmq {

class MQConsumer;

class ConsumeMsgService {
 public:
  ConsumeMsgService() = default;
  virtual ~ConsumeMsgService() = default;
  virtual void start() {}
  virtual void shutdown() {}
  virtual void submitConsumeRequest(PullRequest* request, std::vector<MQMessageExt>& msgs) = 0;
  virtual MessageListenerType getConsumeMsgSerivceListenerType() { return messageListenerDefaultly; }
};

class ConsumeMessageConcurrentlyService : public ConsumeMsgService {
 public:
  ConsumeMessageConcurrentlyService(MQConsumer*, int threadCount, MQMessageListener* msgListener);
  ~ConsumeMessageConcurrentlyService() override;
  void start() override;
  void shutdown() override;
  void submitConsumeRequest(PullRequest* request, std::vector<MQMessageExt>& msgs) override;
  MessageListenerType getConsumeMsgSerivceListenerType() override;

  void ConsumeRequest(PullRequest* request, std::vector<MQMessageExt>& msgs);

 private:
  void resetRetryTopic(vector<MQMessageExt>& msgs);

 private:
  MQConsumer* m_pConsumer;
  MQMessageListener* m_pMessageListener;

  thread_pool_executor m_consumeExecutor;
};

class ConsumeMessageOrderlyService : public ConsumeMsgService {
 public:
  ConsumeMessageOrderlyService(MQConsumer*, int threadCount, MQMessageListener* msgListener);
  ~ConsumeMessageOrderlyService() override;
  void start() override;
  void shutdown() override;
  void submitConsumeRequest(PullRequest* request, std::vector<MQMessageExt>& msgs) override;
  void stopThreadPool();
  MessageListenerType getConsumeMsgSerivceListenerType() override;

  void boost_asio_work();
  void tryLockLaterAndReconsume(PullRequest* request, bool tryLockMQ);
  static void static_submitConsumeRequestLater(void* context,
                                               PullRequest* request,
                                               bool tryLockMQ,
                                               boost::asio::deadline_timer* t);
  void ConsumeRequest(PullRequest* request);
  void lockMQPeriodically(boost::system::error_code& ec, boost::asio::deadline_timer* t);
  void unlockAllMQ();
  bool lockOneMQ(const MQMessageQueue& mq);

 private:
  MQConsumer* m_pConsumer;
  bool m_shutdownInprogress;
  MQMessageListener* m_pMessageListener;
  uint64_t m_MaxTimeConsumeContinuously;
  boost::asio::io_service m_ioService;
  boost::thread_group m_threadpool;
  boost::asio::io_service::work m_ioServiceWork;
  boost::asio::io_service m_async_ioService;
  boost::scoped_ptr<boost::thread> m_async_service_thread;
};

}  // namespace rocketmq

#endif  // __CONSUME_MESSAGE_SERVICE_H__
