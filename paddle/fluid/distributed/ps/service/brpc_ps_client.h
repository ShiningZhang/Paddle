// Copyright (c) 2020 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <ThreadPool.h>
#include <memory>
#include <string>
#include <vector>

#include "brpc/channel.h"
#include "brpc/controller.h"
#include "brpc/server.h"
#include "paddle/fluid/distributed/ps/service/brpc_utils.h"
#include "paddle/fluid/distributed/ps/service/ps_client.h"
#include "paddle/fluid/framework/channel.h"
#include "paddle/fluid/framework/lod_tensor.h"
#include "paddle/fluid/framework/scope.h"
#include "paddle/fluid/framework/tensor_util.h"

namespace brpc {
class Channel;
class Controller;
}  // namespace brpc
namespace google {
namespace protobuf {
class Closure;
class RpcController;
}  // namespace protobuf
}  // namespace google

namespace paddle {
namespace distributed {

struct Region;

class DownpourPsClientService : public PsService {
 public:
  DownpourPsClientService() {}
  virtual ~DownpourPsClientService() {}

  virtual int32_t configure(PSClient *client, size_t rank_id) {
    _client = client;
    _rank = rank_id;
    return 0;
  }
  void service(::google::protobuf::RpcController *controller,
               const PsRequestMessage *request, PsResponseMessage *response,
               ::google::protobuf::Closure *done) override;

 protected:
  size_t _rank;
  PSClient *_client;
};

class DownpourBrpcClosure : public PSClientClosure {
 public:
  DownpourBrpcClosure(size_t num, PSClientCallBack callback)
      : PSClientClosure(callback) {
    _waiting_num = num;

    _cntls.resize(num);
    _requests.resize(num);
    _responses.resize(num);
    for (size_t i = 0; i < num; ++i) {
      _cntls[i].reset(new brpc::Controller());
    }
  }
  virtual ~DownpourBrpcClosure() {}
  void Run() override {
    if (_waiting_num.fetch_sub(1) == 1) {
      _callback(this);
      delete this;
    }
  }
  PsRequestMessage *request(size_t i) { return &_requests[i]; }
  PsResponseMessage *response(size_t i) { return &_responses[i]; }
  brpc::Controller *cntl(size_t i) { return _cntls[i].get(); }
  int check_response(size_t request_idx, int cmd_id);
  int check_save_response(size_t request_idx, int cmd_id);
  std::string get_response(size_t request_idx, int cmd_id);

 private:
  std::atomic<int32_t> _waiting_num;
  std::vector<PsRequestMessage> _requests;
  std::vector<PsResponseMessage> _responses;
  std::vector<std::shared_ptr<brpc::Controller>> _cntls;
};

struct SharedSparsePushData {
  SharedSparsePushData() {}
  ~SharedSparsePushData() noexcept {}
  size_t kv_num;
  std::vector<uint64_t> key_list;
  std::vector<std::string> value_list;
};
struct SparsePushTaskData {
  std::vector<SharedSparsePushData> shared_data;  // sparse数据按key hash分片
};

// push sparse 对象池
struct SparseTaskPool {
  std::shared_ptr<SparsePushTaskData> get() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_pool.empty()) {
      return std::make_shared<SparsePushTaskData>();
    } else {
      auto ret = _pool.back();
      _pool.pop_back();
      return ret;
    }
  }
  void push(std::shared_ptr<SparsePushTaskData> data) {
    std::lock_guard<std::mutex> lock(_mutex);
    _pool.push_back(std::move(data));
  }
  std::vector<std::shared_ptr<SparsePushTaskData>> _pool;
  std::mutex _mutex;
};

template <class T>
struct array_deleter {
  void operator()(T *&x) const { delete[] x; }  // NOLINT
};

class BrpcPsClient : public PSClient {
 public:
  BrpcPsClient() {}
  virtual ~BrpcPsClient() {
    if (_running) {
      flush();
      _running = false;
    }
    if (_async_push_dense_thread.joinable()) {
      _async_push_dense_thread.join();
    }
    if (_async_push_sparse_thread.joinable()) {
      _async_push_sparse_thread.join();
    }
    if (_server_started) {
      _server.Stop(1000);
      _server.Join();
      _server_started = false;
    }
  }
  virtual int32_t create_client2client_connection(
      int pserver_timeout_ms, int pserver_connect_timeout_ms, int max_retry);
  std::future<int32_t> shrink(uint32_t table_id,
                              const std::string threshold) override;
  std::future<int32_t> load(const std::string &epoch,
                            const std::string &mode) override;
  std::future<int32_t> load(uint32_t table_id, const std::string &epoch,
                            const std::string &mode) override;

  std::future<int32_t> save(const std::string &epoch,
                            const std::string &mode) override;

  std::future<int32_t> save(uint32_t table_id, const std::string &epoch,
                            const std::string &mode) override;

  std::future<int32_t> clear() override;

  std::future<int32_t> clear(uint32_t table_id) override;

  std::future<int32_t> stop_server() override;

  std::future<int32_t> start_profiler() override;
  std::future<int32_t> stop_profiler() override;

  void finalize_worker() override;

  virtual std::future<int32_t> pull_dense(Region *regions, size_t region_num,
                                          size_t table_id);

  virtual std::future<int32_t> push_dense_param(const Region *regions,
                                                size_t region_num,
                                                size_t table_id);

  virtual std::future<int32_t> push_dense(const Region *regions,
                                          size_t region_num, size_t table_id);
  void push_dense_task_consume();
  virtual std::future<int32_t> pull_sparse(float **select_values,
                                           size_t table_id,
                                           const uint64_t *keys, size_t num,
                                           bool is_training);
  virtual std::future<int32_t> pull_sparse_param(float **select_values,
                                                 size_t table_id,
                                                 const uint64_t *keys,
                                                 size_t num, bool is_training);

  virtual std::future<int32_t> print_table_stat(uint32_t table_id);

  virtual std::future<int32_t> barrier(size_t table_id, uint32_t barrier_type);

  virtual std::future<int32_t> pull_geo_param(size_t table_id,
                                              std::vector<float> *values,
                                              std::vector<uint64_t> *keys,
                                              int pserver_idx);
  virtual std::future<int32_t> push_global_step(int table_id,
                                                int64_t *total_send_data,
                                                void *done);
  virtual std::future<int32_t> flush();

  std::future<int32_t> send_client2client_msg(int msg_type, int to_client_id,
                                              const std::string &msg) override;

  // for local save sparse
  virtual int32_t recv_and_save_table(const uint64_t table_id,
                                      const std::string &path);

  void print_queue_size();
  void print_queue_size_thread();

 protected:
  virtual size_t get_server_nums() { return _server_channels.size(); }
  inline brpc::Channel *get_sparse_channel(size_t server_id) {
    return _server_channels[server_id][0].get();
  }
  inline brpc::Channel *get_dense_channel(size_t server_id) {
    return _server_channels[server_id][1].get();
  }
  inline brpc::Channel *get_cmd_channel(size_t server_id) {
    return _server_channels[server_id][2].get();
  }
  int32_t initialize() override;

 private:
  // virtual int32_t initialize() override;

  inline uint32_t dense_dim_per_shard(uint32_t dense_dim_total,
                                      uint32_t shard_num) {
    return dense_dim_total / shard_num + 1;
  }

  std::future<int32_t> send_cmd(uint32_t table_id, int cmd_id,
                                const std::vector<std::string> &param);

  std::future<int32_t> send_save_cmd(uint32_t table_id, int cmd_id,
                                     const std::vector<std::string> &param);

  bool _running = false;
  bool _flushing = false;
  std::atomic<uint32_t> _async_call_num;  // 异步请求计数

  // 异步push dense task
  std::thread _async_push_dense_thread;
  typedef AsyncRequestTask<std::shared_ptr<std::vector<float>>> DenseAsyncTask;
  std::unordered_map<uint32_t, paddle::framework::Channel<DenseAsyncTask *>>
      _push_dense_task_queue_map;
  // 异步push sparse task
  std::thread _async_push_sparse_thread;
  typedef AsyncRequestTask<std::shared_ptr<SparsePushTaskData>> SparseAsyncTask;
  std::unordered_map<uint32_t, paddle::framework::Channel<SparseAsyncTask *>>
      _push_sparse_task_queue_map;
  std::unordered_map<uint32_t, uint32_t> _push_sparse_merge_count_map;

  std::thread _print_thread;

  int push_sparse_async_shard_merge(
      std::vector<std::shared_ptr<SparseAsyncTask>> &task_list,       // NOLINT
      std::vector<int> &request_kv_num, int table_id, int shard_idx,  // NOLINT
      ValueAccessor *accessor);

  int push_sparse_async_shard_push(
      std::vector<std::shared_ptr<SparseAsyncTask>> &task_list,       // NOLINT
      std::vector<int> &request_kv_num, int table_id, int shard_idx,  // NOLINT
      DownpourBrpcClosure *closure, ValueAccessor *accessor);

  SparseTaskPool _sparse_task_pool;

  std::vector<std::shared_ptr<brpc::Channel>>
      _client_channels;  // client2client
  std::vector<std::array<std::shared_ptr<brpc::Channel>, 3>>
      _server_channels;  // client2server
  std::future<int32_t> push_dense_raw_gradient(int table_id,
                                               float *total_send_data,
                                               size_t total_send_data_size,
                                               void *done) override;

  std::future<int32_t> push_sparse_raw_gradient(size_t table_id,
                                                const uint64_t *keys,
                                                const float **update_values,
                                                size_t num,
                                                void *done) override;

  std::future<int32_t> push_sparse_raw_gradient_partial(
      size_t table_id, const uint64_t *keys, const float **update_values,
      uint32_t num, void *done, int pserver_idx) override;

  std::future<int32_t> push_sparse_param(size_t table_id, const uint64_t *keys,
                                         const float **update_values,
                                         size_t num, void *done) override;
  std::future<int32_t> push_sparse(size_t table_id, const uint64_t *keys,
                                   const float **update_values,
                                   size_t num) override;
  void push_sparse_task_consume();

 private:
  int32_t start_client_service();

  void push_dense_raw_gradient(std::shared_ptr<DenseAsyncTask> &task,  // NOLINT
                               float *total_send_data,
                               size_t total_send_data_size,
                               DownpourBrpcClosure *closure);
  float _mae = 0;
  float _mse = 0;
  uint16_t _push_times = 0;
  brpc::Server _server;
  DownpourPsClientService _service;
  bool _server_started = false;
  std::atomic_uint grad_num_{0};
};
}  // namespace distributed
}  // namespace paddle
