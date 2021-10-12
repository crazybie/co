//#include "stdafx.h"

#include <uv.h>

#include <chrono>
#include <iostream>
#include <map>
#include <thread>

#include "coroutine.h"

using namespace co;

using std::map;
using std::string;

enum class ErrorCode {
  OK,
  Timeout,
  InvalidParam,
};

struct ExceptionWithCode : std::runtime_error {
  int code;
  ExceptionWithCode(const char* const msg, int _code)
      : std::runtime_error(msg), code(_code) {}

  static std::exception_ptr make(const char* msg, int code) {
    return std::make_exception_ptr(ExceptionWithCode(msg, code));
  }
};

template <typename R, typename F, typename... Args>
PromisePtr<R> promised(F f, Args... args) {
  CoBeginImp(R) {
    __co_state++;
    f(args..., [=](ErrorCode err, R r) {
      if (err != ErrorCode::OK) {
        __onErr(ExceptionWithCode::make("promised error", (int)err));
      } else {
        __onOk(r);
      }
    });
  }
  CoEnd
}

template <typename F>
void delay(int ms, F f) {
  std::thread([=] {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    f();
  }).detach();
}

void GetUrl(string url, co::Action<ErrorCode, string> cb) {
  printf("fetching url:%s\n", url.c_str());
  delay(3000, [=] {
    if (url.find("http") != 0) return cb(ErrorCode::InvalidParam, string{});
    cb(ErrorCode::OK, "hello, world. from url:" + url);
    // cb(ErrorCode::Timeout, {});
  });
}

struct AuthData {
  int uid;
  int sid;
  string account;
};

class Handler {
 public:
  CoFunc(map<string, int>) login(int randCode) {
    PromisePtr<int> p1, p2;
    CoBegin;

    CoAwait(callInSequence());

    puts("call in parallel");
    p1 = call("getUID", randCode);
    p2 = call("getSID", randCode);
    CoAwait(co::all({p1, p2}));

    CoReturn(map<string, int>{
        {"uid", p1->getValue()},
        {"sid", p2->getValue()},
    });

    CoEnd;
  };

  CoFunc(int) call(string cmd, int acc) {
    printf("call:%s\n", cmd.c_str());
    CoBegin;
    if (cmd == "getUID")
      delay(3000, [=] { CoReturn(110); });
    else if (cmd == "getSID")
      delay(3000, [=] { CoReturn(220); });
    else
      throw std::logic_error("unknown args:" + cmd);

    CoEnd;
  }

  CoFunc(bool) callInSequence() {
    puts("call in sequence");
    string v;

    CoBegin;

    CoTryAwait(
        v, promised<string>(GetUrl, "ttp://test2"), (ExceptionWithCode & e) {
          v = "";
          printf("exception: %s, code: %d\n", e.what(), e.code);
        });
    if (v.length()) printf("getUrl:%s\n", v.c_str());

    CoAwait(v, promised<string>(GetUrl, "http://test1"));
    printf("getUrl:%s\n", v.c_str());

    CoReturn(true);

    CoEnd;
  }
};

void testLogin() {
  Handler h;

  auto prom = h.login(3);
  prom->onDone([](map<string, int> d) {
    printf("login done: uid=%d, sid=%d\n", d["uid"], d["sid"]);
  });
  prom->onError([](const std::exception& e) {
    printf("login error: %s\n", e.what());
    if (auto i = dynamic_cast<const ExceptionWithCode*>(&e)) {
      printf("error code:%d", i->code);
    }
  });
}

template <typename R, typename F, typename Req, typename Cvt, typename... Args>
PromisePtr<R> uv_promised(F* f, void (*del)(Req*), Cvt rc, Args... args) {
  Req req;
  CoBeginImp(R);

  __co_state++;
  auto cb = [=](Req* req) mutable {
    rc(req, __onOk, __onErr);
    del(req);
  };
  using CB = decltype(cb);
  req.data = new CB(cb);
  auto ret = f(uv_default_loop(), &req, args..., [](Req* req) {
    auto cb = (CB*)req->data;
    (*cb)(req);
    delete cb;
  });
  if (ret < 0) __onErr(ExceptionWithCode::make("promised error", ret));
  CoEnd;
}

PromisePtr<int> fs_open(const char* fname, int mode) {
  return uv_promised<int>(
      uv_fs_open, uv_fs_req_cleanup,
      [](uv_fs_t* req, Action<int> ok, ErrorCb err) {
        if (req->result < 0)
          return err(
              ExceptionWithCode::make("fs_open error", (int)req->result));
        ok((int)req->result);
      },
      fname, mode, 0);
}

PromisePtr<bool> fs_close(uv_file fd) {
  return uv_promised<bool>(
      uv_fs_close, uv_fs_req_cleanup,
      [](uv_fs_t* req, Action<bool> ok, ErrorCb err) { ok(true); }, fd);
};

PromisePtr<string> fs_read(uv_file fd, int offset) {
  string buffer;
  buffer.resize(255);
  auto iov = uv_buf_init(&buffer[0], buffer.size() - 1);
  return uv_promised<string>(
      uv_fs_read, uv_fs_req_cleanup,
      [=, buffer2 = move(buffer)](uv_fs_t* req, Action<string> ok,
                                  ErrorCb err) mutable {
        if (req->result < 0)
          return err(
              ExceptionWithCode::make("fs_read error", (int)req->result));
        buffer2[req->result] = 0;
        if (req->result == 0) return ok({});
        ok(buffer2);
      },
      fd, &iov, 1, offset);
}

CoFunc(string) fs_read_all(const char* fname) {
  string content, cur;
  int fd;
  CoBegin;

  CoAwait(fd, fs_open(fname, O_RDONLY));
  for (;;) {
    CoAwait(cur, fs_read(fd, content.length()));
    if (cur.length() == 0) break;
    content += cur;
  }
  CoAwait(fs_close(fd));
  CoReturn(content);

  CoEnd;
};

void testLibuv() {
  auto p = fs_read_all(__FILE__);
  p->onDone([](string s) { printf("done: sz=%zd\n", s.length()); });
  p->onError([](const std::exception& e) {
    printf("libuv error: %s\n", e.what());
    if (auto i = dynamic_cast<const ExceptionWithCode*>(&e)) {
      printf("error code:%d", i->code);
    }
  });
}

int main() {
  Executor sc;

  // testLogin();

  for (auto i = 0; i < 500; i++) testLibuv();

  int cnt = 0;
  while (sc.updateAll()) {
    printf("\r%c", "-\\|-/"[cnt++ % 5]);
    // std::this_thread::sleep_for(std::chrono::milliseconds(30));
    uv_run(uv_default_loop(), UV_RUN_ONCE);
  }
}
