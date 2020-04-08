#include <exception>
#include <functional>
#include <list>
#include <memory>
#include "gcptr/gcptr.h"

namespace co {
using namespace std;
using namespace tgc;

template <typename... A>
using Action = gc_function<void(A...)>;

using ErrorCb = Action<exception_ptr>;

template <typename T>
using Ptr = gc<T>;

#define CoBegin(...)                  \
  auto __state = 0;                   \
  co::Ptr<co::PromiseBase> __promise; \
    return gc_new<co::Promise<__VA_ARGS__>>([=](const co::Action<__VA_ARGS__>& __onOk, const co::ErrorCb& __onErr) mutable->co::Ptr<co::PromiseBase> { \
        try { \
            switch ( __state ) { case 0:
///
#define CoAwait(expr)          \
  do {                         \
    __state = __LINE__;        \
    return __promise = expr;   \
    case __LINE__:             \
      __promise->checkError(); \
  } while (0)

#define CoTryAwait(expr, catchBlock) \
  do {                               \
    try {                            \
      __state = __LINE__;            \
      return __promise = expr;       \
    } catch catchBlock;              \
    break;                           \
    case __LINE__:                   \
      try {                          \
        __promise->checkError();     \
      } catch catchBlock             \
      ;                              \
  } while (0)

#define CoAwaitData(var, expr)                                        \
  do {                                                                \
    __state = __LINE__;                                               \
    return __promise = expr;                                          \
    case __LINE__:                                                    \
      using __ty = decltype(expr)::element_type;                      \
      var = tgc::gc_static_pointer_cast<__ty>(__promise)->getValue(); \
  } while (0)

#define CoTryAwaitData(var, expr, catchBlock)                           \
  do {                                                                  \
    try {                                                               \
      __state = __LINE__;                                               \
      return __promise = expr;                                          \
    } catch catchBlock;                                                 \
    break;                                                              \
    case __LINE__:                                                      \
      try {                                                             \
        using __ty = decltype(expr)::element_type;                      \
        var = tgc::gc_static_pointer_cast<__ty>(__promise)->getValue(); \
      } catch catchBlock                                                \
      ;                                                                 \
  } while (0)

#define CoReturn(...)    \
  do {                   \
    __onOk(__VA_ARGS__); \
    return nullptr;      \
  } while (0)

#define CoEnd()                        \
  }                                    \
  ; /* end switch */                   \
  } /* end try */                      \
  catch (...) {                        \
    __onErr(std::current_exception()); \
  }                                    \
  return nullptr;                      \
  });

class PromiseBase;

class Executor {
 public:
  static Executor*& instance() {
    static Executor* i;
    return i;
  }
  Executor() { instance() = this; }
  virtual ~Executor() { instance() = nullptr; }
  virtual bool updateAll();
  virtual void add(const gc<PromiseBase>& i) { pool->push_back(i); }
  virtual void remove(const gc<PromiseBase>& i) { pool->remove(i); }

 private:
  gc_list<PromiseBase> pool = gc_new_list<PromiseBase>();
};

class PromiseBase {
 public:
  enum class State { Failed, Completed, Inprogress } state = State::Inprogress;

  void onError(Action<exception&> cb) {
    if (state == State::Failed) {
      try {
        rethrow_exception(excep);
      } catch (exception& e) {
        cb(e);
      }
    } else
      errorCb = cb;
  }
  void checkError() {
    if (state == State::Failed)
      rethrow_exception(excep);
  }
  virtual void update() = 0;

 protected:
  PromiseBase() { Executor::instance()->add(gc_from(this)); }
  PromiseBase(const PromiseBase& r) = delete;
  PromiseBase(PromiseBase&& r) = delete;
  void rejected(exception_ptr e) {
    state = State::Failed;
    excep = e;
    if (errorCb)
      onError(errorCb);
  }

 protected:
  Ptr<PromiseBase> subFsm;
  exception_ptr excep;
  Action<exception&> errorCb;
};

template <typename T>
class Promise : public PromiseBase {
 public:
  template <typename F>
  Promise(F&& f) : fsm(move(f)) {
    callResolved = [this](T v) { resolved(v); };
    callError = [this](exception_ptr err) { rejected(err); };
    update();
  }
  Promise(nullptr_t) {}
  Promise(T t) { resolved(t); }

  void onDone(Action<T> cb) {
    if (state == State::Completed)
      cb(value);
    else
      okCb = cb;
  }
  const T& getValue() {
    checkError();
    return value;
  }

 protected:
  void update() override {
    if (subFsm && subFsm->state == State::Inprogress) {
      return;
    }
    subFsm = fsm(callResolved, callError);
  }
  void resolved(T v) {
    value = v;
    state = State::Completed;
    if (okCb)
      okCb(value);
  }

 private:
  T value;
  Action<T> okCb;
  Action<T> callResolved;
  ErrorCb callError;
  gc_function<Ptr<PromiseBase>(const Action<T>& onOK, const ErrorCb& onErr)>
      fsm;
};

//////////////////////////////////////////////////////////////////////////

template <typename T>
using PromisePtr = Ptr<Promise<T>>;

bool Executor::updateAll() {
  auto inprogress = false;
  for (auto& i : *pool) {
    if (i->state == PromiseBase::State::Inprogress) {
      inprogress = true;
      i->update();
    }
  }
  return inprogress;
}

inline static PromisePtr<bool> all(const list<Ptr<PromiseBase>>& l) {
  CoBegin(bool) {
    auto allDone = true;
    for (auto& i : l) {
      if (i->state == PromiseBase::State::Inprogress) {
        allDone = false;
        break;
      }
    }
    if (allDone)
      CoReturn(true);
  }
  CoEnd()
}
}  // namespace co
