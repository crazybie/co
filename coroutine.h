#pragma once
#include <exception>
#include <functional>
#include <list>
#include <memory>

namespace co {
using namespace std;

template <typename... A>
using Action = std::function<void(A...)>;

using ErrorCb = Action<exception_ptr>;

template <typename T>
using Ptr = std::shared_ptr<T>;

template <int N>
struct Id : Id<N - 1> {};

template <>
struct Id<0> {};

// FIX msvc:
// https://stackoverflow.com/questions/57137351/line-is-not-constexpr-in-msvc
#define _CoCat(X, Y) _CoCat2(X, Y)
#define _CoCat2(X, Y) X##Y
#define _CO_LINE int(_CoCat(__LINE__, U))

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

#define CoAwait(...) \
  _CoMsvcExpand(     \
      _CoDelay(_CoAwaitChoose, _CoArgsCount(__VA_ARGS__))(__VA_ARGS__))
#define _CoAwaitChoose(N) CoAwait##N

#define CoTryAwait(...) \
  _CoMsvcExpand(        \
      _CoDelay(_CoTryAwaitChoose, _CoArgsCount(__VA_ARGS__))(__VA_ARGS__))
#define _CoTryAwaitChoose(N) CoTryAwait##N

#define CoFunc(...)                       \
  __VA_ARGS__ __co_ret(co::Id<_CO_LINE>); \
  co::PromisePtr<__VA_ARGS__>

#define CoBegin                                         \
  using __ret = decltype(__co_ret(co::Id<_CO_LINE>{})); \
  CoBeginImp(__ret)

#define CoBeginImp(...)               \
  auto __state = 0;                   \
  co::Ptr<co::PromiseBase> __promise; \
    auto ret = std::make_shared<co::Promise<__VA_ARGS__>>([=](const co::Action<__VA_ARGS__>& __onOk, const co::ErrorCb& __onErr) mutable->co::Ptr<co::PromiseBase> { \
        try { \
            switch ( __state ) { case 0:
///
#define CoAwait1(expr)         \
  do {                         \
    __state = __LINE__;        \
    return __promise = expr;   \
    case __LINE__:             \
      __promise->checkError(); \
  } while (0)

#define CoTryAwait2(expr, catchBlock) \
  do {                                \
    try {                             \
      __state = __LINE__;             \
      return __promise = expr;        \
    } catch catchBlock;               \
    break;                            \
    case __LINE__:                    \
      try {                           \
        __promise->checkError();      \
      } catch catchBlock              \
      ;                               \
  } while (0)

#define CoAwait2(var, expr)                                        \
  do {                                                             \
    __state = __LINE__;                                            \
    return __promise = expr;                                       \
    case __LINE__:                                                 \
      using __ty = decltype(expr)::element_type;                   \
      var = std::static_pointer_cast<__ty>(__promise)->getValue(); \
  } while (0)

#define CoTryAwait3(var, expr, catchBlock)                           \
  do {                                                               \
    try {                                                            \
      __state = __LINE__;                                            \
      return __promise = expr;                                       \
    } catch catchBlock;                                              \
    break;                                                           \
    case __LINE__:                                                   \
      try {                                                          \
        using __ty = decltype(expr)::element_type;                   \
        var = std::static_pointer_cast<__ty>(__promise)->getValue(); \
      } catch catchBlock                                             \
      ;                                                              \
  } while (0)

#define CoReturn(...)    \
  do {                   \
    __onOk(__VA_ARGS__); \
    return nullptr;      \
  } while (0)

#define CoEnd                          \
  }                                    \
  ; /* end switch */                   \
  } /* end try */                      \
  catch (...) {                        \
    __onErr(std::current_exception()); \
  }                                    \
  return nullptr;                      \
  });                                  \
  co::Executor::instance()->add(ret);  \
  return ret;

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
  virtual void add(Ptr<PromiseBase> i) { pool.push_back(i); }
  virtual void remove(Ptr<PromiseBase> i) { pool.remove(i); }

 private:
  std::list<Ptr<PromiseBase>> pool;
};

class PromiseBase : public std::enable_shared_from_this<PromiseBase> {
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
    if (state == State::Failed) rethrow_exception(excep);
  }
  virtual void update() = 0;

 protected:
  PromiseBase() {}
  PromiseBase(const PromiseBase& r) = delete;
  PromiseBase(PromiseBase&& r) = delete;
  void rejected(exception_ptr e) {
    state = State::Failed;
    excep = e;
    if (errorCb) onError(errorCb);
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
  std::function<Ptr<PromiseBase>(const Action<T>& onOK, const ErrorCb& onErr)>
      fsm;
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

inline static PromisePtr<bool> all(const list<Ptr<PromiseBase>>& l) {
  CoBeginImp(bool) {
    auto allDone = true;
    for (auto& i : l) {
      if (i->state == PromiseBase::State::Inprogress) {
        allDone = false;
        break;
      }
    }
    if (allDone) CoReturn(true);
  }
  CoEnd
}
}  // namespace co
