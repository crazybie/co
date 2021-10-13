#pragma once
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <vector>

namespace co {

using std::exception;
using std::exception_ptr;
using std::list;
using std::move;

template <typename... A>
using Action = std::function<void(A...)>;

template <typename T>
using Func = std::function<T>;

using ErrorCb = Action<exception_ptr>;

template <typename T>
using Ptr = std::shared_ptr<T>;

template <int N>
struct Id : Id<N - 1> {};

template <>
struct Id<0> {};

using PromiseBasePtr = Ptr<class PromiseBase>;

////////////////////////////////////////////////////
// Executor

class Executor {
 public:
  static Executor*& instance() {
    static Executor* i;
    return i;
  }
  Executor() { instance() = this; }
  virtual ~Executor() { instance() = nullptr; }
  virtual bool updateAll();
  virtual void add(const PromiseBasePtr& i) { pool.push_back(i); }
  virtual void remove(const PromiseBasePtr& i) { pool.remove(i); }

 private:
  std::list<PromiseBasePtr> pool;
};

////////////////////////////////////////////////////

class PromiseBase : public std::enable_shared_from_this<PromiseBase> {
 public:
  enum class State { Failed, Completed, Inprogress } state = State::Inprogress;

  void onError(Action<const exception&> cb) {
    if (state == State::Failed) {
      try {
        std::rethrow_exception(excep);
      } catch (const exception& e) {
        cb(e);
      }
    } else
      errorCb = cb;
  }
  void checkError() {
    if (state == State::Failed) std::rethrow_exception(excep);
  }
  virtual void update() = 0;

 protected:
  PromiseBase() {}
  void rejected(exception_ptr e) {
    state = State::Failed;
    excep = e;
    if (errorCb) onError(errorCb);
  }

 protected:
  PromiseBasePtr subFsm;
  exception_ptr excep;
  Action<const exception&> errorCb;
};

////////////////////////////////////////////////////

template <typename T>
class Promise : public PromiseBase {
 public:
  template <typename F>
  Promise(F&& f)
      : fsm(move(f)),
        callResolved{[this](T v) { resolved(v); }},
        callError{[this](exception_ptr err) { rejected(err); }} {
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
    if (okCb) okCb(value);
  }

 private:
  T value;
  Action<T> okCb;
  Action<T> callResolved;
  ErrorCb callError;
  Func<PromiseBasePtr(const Action<T>& onOK, const ErrorCb& onErr)> fsm;
};

//////////////////////////////////////////////////////////////////////////

template <typename T>
using PromisePtr = Ptr<Promise<T>>;

bool Executor::updateAll() {
  auto inprogress = false;
  for (auto i = pool.begin(); i != pool.end();) {
    auto& pro = *i;
    switch (pro->state) {
      case PromiseBase::State::Inprogress: {
        inprogress = true;
        pro->update();
      } break;
      case PromiseBase::State::Completed: {
        i = pool.erase(i);
        continue;
      } break;
    }
    ++i;
  }
  return inprogress;
}

}  // namespace co

// https://stackoverflow.com/questions/57137351/line-is-not-constexpr-in-msvc
#define _CoCat(X, Y) _CoCat2(X, Y)
#define _CoCat2(X, Y) X##Y
#define _CoMsvcConstexprLine int(_CoCat(__LINE__, U))

// Args Count
#define _CoMsvcExpand(...) __VA_ARGS__
#define _CoDelay(X, ...) _CoMsvcExpand(X(__VA_ARGS__))
#define _CoEvaluateCount(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, \
                         _13, _14, _15, _16, _17, _18, _19, _20, _21, _22,  \
                         _23, _24, _25, _26, _27, _28, _29, _30, N, ...)    \
  N

#define _CoArgsCount(...)                                                     \
  _CoMsvcExpand(_CoEvaluateCount(__VA_ARGS__, 30, 29, 28, 27, 26, 25, 24, 23, \
                                 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12,  \
                                 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))

// API

#define CoAwait(...) \
  _CoMsvcExpand(     \
      _CoDelay(_CoAwaitChoose, _CoArgsCount(__VA_ARGS__))(__VA_ARGS__))
#define _CoAwaitChoose(N) CoAwait##N

#define CoTryAwait(...) \
  _CoMsvcExpand(        \
      _CoDelay(_CoTryAwaitChoose, _CoArgsCount(__VA_ARGS__))(__VA_ARGS__))
#define _CoTryAwaitChoose(N) CoTryAwait##N

#define CoFunc(...)                                        \
  __VA_ARGS__ __co_ret_type(co::Id<_CoMsvcConstexprLine>); \
  co::PromisePtr<__VA_ARGS__>

#define CoBegin                                                            \
  using __ret_t = decltype(__co_ret_type(co::Id<_CoMsvcConstexprLine>{})); \
  CoBeginImp(__ret_t)

// implementation

#define CoBeginImp(...)            \
  auto __co_state = 0;             \
  co::PromiseBasePtr __co_promise; \
    auto __ret = std::make_shared<co::Promise<__VA_ARGS__>>([=](const co::Action<__VA_ARGS__>& __onOk, const co::ErrorCb& __onErr) mutable->co::PromiseBasePtr { \
        try { \
            switch ( __co_state ) { case 0:
///
#define CoAwait1(expr)            \
  do {                            \
    __co_state = __LINE__;        \
    return __co_promise = expr;   \
    case __LINE__:                \
      __co_promise->checkError(); \
  } while (0)

#define CoTryAwait2(expr, catchBlock) \
  do {                                \
    try {                             \
      __co_state = __LINE__;          \
      return __co_promise = expr;     \
    } catch catchBlock;               \
    break;                            \
    case __LINE__:                    \
      try {                           \
        __co_promise->checkError();   \
      } catch catchBlock              \
      ;                               \
  } while (0)

#define CoAwait2(var, expr)                                           \
  do {                                                                \
    __co_state = __LINE__;                                            \
    return __co_promise = expr;                                       \
    case __LINE__:                                                    \
      using __ty = decltype(expr)::element_type;                      \
      var = std::static_pointer_cast<__ty>(__co_promise)->getValue(); \
  } while (0)

#define CoTryAwait3(var, expr, catchBlock)                              \
  do {                                                                  \
    try {                                                               \
      __co_state = __LINE__;                                            \
      return __co_promise = expr;                                       \
    } catch catchBlock;                                                 \
    break;                                                              \
    case __LINE__:                                                      \
      try {                                                             \
        using __ty = decltype(expr)::element_type;                      \
        var = std::static_pointer_cast<__ty>(__co_promise)->getValue(); \
      } catch catchBlock                                                \
      ;                                                                 \
  } while (0)

#define CoReturn(...)    \
  do {                   \
    __onOk(__VA_ARGS__); \
    return nullptr;      \
  } while (0)

#define CoEnd                           \
  }                                     \
  ; /* end switch */                    \
  } /* end try */                       \
  catch (...) {                         \
    __onErr(std::current_exception());  \
  }                                     \
  return nullptr;                       \
  });                                   \
  co::Executor::instance()->add(__ret); \
  return __ret;

namespace co {

inline CoFunc(bool) all(const list<PromiseBasePtr>& l) {
  CoBegin;
  auto done = true;
  for (auto& i : l) {
    if (i->state == PromiseBase::State::Inprogress) {
      done = false;
      break;
    }
  }
  if (done) CoReturn(true);
  CoEnd;
}

}