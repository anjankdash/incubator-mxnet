/*!
 * Copyright (c) 2015 by Contributors
 * \file kvstore.h
 * \brief key-value store interface for mxnet
 */
#ifndef MXNET_PS_H_
#define MXNET_PS_H_
#include "dmlc/io.h"
#include "narray.h"

#if DMLC_USE_CXX11
#include <functional>
#endif  // DMLC_USE_CXX11

namespace mxnet {

/*! \brief forward declaration */
class KVStoreBase;

/**
 * \brief distributed key-value store
 *
 * A distributed key-value store for data synchronization over multiple
 * devices/machines. It supports user-defined updater
 *
 * Example to implement allreduce
 * \code
 *   NArray data;
 *   // init data...
 *   KVStore store;
 *   store.Push(0, data);
 *   store.Pull(0, &data);
 *   data.Wait();
 * \endcode
 *
 * Example to implement asynchronous SGD
 * \code
 *   Worker store;
 *   auto updater = [](const NArray& recv, NArray* weight) {
 *     *weight += 0.1 * recv; // recv is grad
 *   }
 *   store.Register(false, updater);
 *
 *   NArray weight, grad;
 *   if (store.GetRank() == 0) {
 *     store.Init(0, weight);
 *   }
 *   store.Pull(0, &weight);
 *   // compute grad
 *   store.Push(0, grad);
 */
class KVStore {
 public:
  /**
   * \brief get singleton instance
   */
  static KVStore* Get() { static KVStore store; return &store; }

  /**
   * \brief Init with the local devices
   */
  void InitDevices(const std::vector<Context>& devices);

  /**
   * \brief  data
   *
   * init a key-value pair. One must insert before push and pull
   */
  void Init(int key, const NArray& value);

  /*!
   * \brief push data to the store
   *
   * Push the key-value pair (\a key, \a value) to the store.  This
   * function returns after adding a push operator to the engine. Any following
   * operator requiring writing \a value will be blocked until the actual push is
   * finished.
   *
   * One can wait the push is finished via `data.Wait()`
   *
   * For each push, a user-defined updater is called to merge
   * the value sent to the one maintained by itself.
   *
   * For a given \a key, the \a value should be always has the same size over
   *
   * \param key the key for pushing
   * \param value the value for pushing
   */
  void Push(int key, const NArray& value);

  /*!
   * \brief pull data from the server nodes
   *
   * Pull the \a value associated with the \a key from the store.  This
   * function returns after adding a pull operator to the engine. Any following
   * operator requiring reading \a data will be blocked until the actual pull is
   * finished.
   *
   * One can wait the pull is finished via `data.Wait()`
   *
   * Before sending back the value, the store will wait all pushed issued by
   * this worker on \a key have been applied (updater has been triggered)
   * and \a value is initialized
   *
   * \param key the key for pulling
   * \param value data for pulling, should be pre-allocated
   */
  void Pull(int key, NArray* value);

  /**
   * \brief clear all data stored, handles registered, and devices binded
   */
  void Clear();

#if DMLC_USE_CXX11
  /**
   * \brief user-defined updater
   */
  using Updater = std::function<void(const NArray&, NArray*)>;

  /**
   * \brief returns the default updater, which is ASSIGN
   */
  static Updater DefaultUpdater() {
    return [](const NArray& a, NArray* b) {
      CopyFromTo(a, b);
    };
  }

  /**
   * \brief set an updater
   *
   * The server allows user-defined handle to modify the data.  Given a key,
   * assume \a x is the received value and \a y is the value stored on the server
   * node. The server updates \a y by `h(x, &y)`. The default \a h is ASSIGN,
   * namely `*y = x`.
   *
   * The handle is triggered in two ways:
   *
   * - online: \a h is called every time when \a x is received from a worker. It
   * is often used for asynchronous optimization.
   *
   * - batch: \a h is called after data have been aggregated over all
   * workers. Assume \f$ x_i \f$ is received from worker i. Then the server
   * first computes \f$\sum_{i=0}^n x = x_i\f$, and then applies \a h. It is often
   * used for synchronous optimization
   *
   * Must be called before \ref Init
   * \param batch true for batch, false for online
   * \param updt user-defined updater, default is assign
   */
  void SetUpdater(const Updater& updt);
#endif  // DMLC_USE_CXX11

  /**
   * \brief set aggregator
   * The aggregator first aggregate all pushed data among all devices before
   * applying the updater
   *
   * The aggregator is enabled in default
   *
   * \param aggregator false to disable
   */
  void SetAggregator(bool aggregator);

  /*! \brief Gets rank of this node in its group, which is in [0, GroupSize) */
  int GetRank();

  /*! \brief Get the number of nodes in this group. */
  int GetGroupSize();

 private:
  DISALLOW_COPY_AND_ASSIGN(KVStore);
  KVStore() : store_(NULL) { }
  ~KVStore();
  KVStoreBase* store_;
};

}  // namespace mxnet
#endif  // MXNET_PS_H_
