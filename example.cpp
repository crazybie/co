//#include "stdafx.h"

#include <iostream>
#include <map>
#include <thread>
#include <chrono>
#include "coroutine.h"

using co::PromisePtr;
using std::map;
using std::string;


enum class ErrorCode
{
	OK,
	Timeout,
};

struct ExceptionWithCode : std::runtime_error
{
	int code;
	ExceptionWithCode(const char* const msg, int _code)
		: std::runtime_error(msg), code(_code) {}
};

template<typename R, typename F, typename... Args>
PromisePtr<R> promised(F f, Args... args)
{
	CoBegin(R)
	{
		__state = 1;
		f(args..., [=](ErrorCode err, R r) {
			if (err != ErrorCode::OK) {
				onErr(co::exceptionPtr(ExceptionWithCode("promised error", (int)err)));
			}
			else {
				onOk(r);
			}
		});
	}
	CoEnd()
}

template<typename F>
void delay(int ms, F f)
{
	std::thread([=] {
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
		f();
	}).detach();
}

void GetUrl(string url, co::Action<ErrorCode, string> cb)
{
	printf("fetching url:%s\n", url.c_str());
	delay(3000, [=] {
		cb(ErrorCode::OK, "hello, world. from url:" + url);
		//cb(ErrorCode::Timeout, {});
	});
}

struct AuthData
{
	int uid;
	int sid;
	string account;
};

class Handler
{
public:
	PromisePtr<map<string, int>> login(int randCode)
	{
		PromisePtr<int> p1, p2;

		CoBegin(map<string, int>)
		{
			CoAwait(callInSequence());

			puts("call in parallel");
			p1 = call("getUID", randCode);
			p2 = call("getSID", randCode);
			CoAwait(co::all({ p1, p2 }));

			CoReturn({ {"uid", p1->getValue() },{"sid", p2->getValue() }, });
		}
		CoEnd()
	};

	PromisePtr<int> call(string cmd, int acc)
	{
		printf("call:%s\n", cmd.c_str());
		CoBegin(int)
		{
			if (cmd == "getUID")
				delay(3000, [=] { CoReturn(110); });
			else if (cmd == "getSID")
				delay(3000, [=] { CoReturn(220); });
			else
				throw std::logic_error("unknown args:" + cmd);
		}
		CoEnd()
	}

	PromisePtr<bool> callInSequence()
	{
		puts("call in sequence");
		string v;
		CoBegin(bool)
		{
			CoAwaitData(v, promised<string>(GetUrl, string("http://test1")));
			printf("getUrl:%s\n", v.c_str());

			CoAwaitData(v, promised<string>(GetUrl, string("http://test2")));
			printf("getUrl:%s\n", v.c_str());

			CoReturn(true);
		}
		CoEnd()
	}
};


void testLogin()
{
	Handler h;

	static auto prom = h.login(3);
	prom->onDone([](map<string, int> d) {
		printf("login done: uid=%d, sid=%d\n", d["uid"], d["sid"]);
	});
	prom->onError([](std::exception& e) {
		printf("login error: %s\n", e.what());
		if (auto i = dynamic_cast<ExceptionWithCode*>(&e)) {
			printf("error code:%d", i->code);
		}
	});
}

#include <uv.h>

using namespace co;

template<typename R, typename F, typename Req, typename Cvt, typename... Args>
PromisePtr<R> uv_promised(F* f, void(*del)(Req*), Cvt rc, Args... args)
{
	Req req;
	CoBegin(R)
	{
		__state = 1;
		auto cb = [=](Req* req) { rc(req, onOk, onErr); del(req); };
		using CB = decltype(cb);
		req.data = new CB(cb);
		auto ret = f(uv_default_loop(), &req, args..., [](Req* req) { auto cb = (CB*)req->data; (*cb)(req); delete cb; });
		if (ret < 0)
			onErr(co::exceptionPtr(ExceptionWithCode("promised error", ret)));
	}
	CoEnd()
}

PromisePtr<int> fs_open(const char* fname, int mode) {
	return uv_promised<int>(uv_fs_open, uv_fs_req_cleanup, [](uv_fs_t* req, Action<int> ok, ErrorCb err) {
		if (req->result < 0) return err(exceptionPtr(ExceptionWithCode("fs_open error", (int)req->result)));
		ok((int)req->result);
	}, fname, mode, 0);
}

PromisePtr<bool> fs_close(uv_file fd) {
	return uv_promised<bool>(uv_fs_close, uv_fs_req_cleanup, [](uv_fs_t* req, Action<int> ok, ErrorCb err) {
		ok(true);
	}, fd);
};

PromisePtr<const char*> fs_read(uv_file fd, int offset) {
	static char buffer[255];
	static auto iov = uv_buf_init(buffer, sizeof(buffer) - 1);
	return uv_promised<const char*>(uv_fs_read, uv_fs_req_cleanup, [=](uv_fs_t* req, Action<const char*> ok, ErrorCb err) {
		if (req->result < 0) return err(exceptionPtr(ExceptionWithCode("fs_read error", (int)req->result)));
		buffer[req->result] = 0;
		if (req->result == 0) return ok(nullptr);
		ok(buffer);
	}, fd, &iov, 1, offset);
}

PromisePtr<string> fs_read_all(const char* fname)
{
	string content;
	const char* cur;
	int fd;
	CoBegin(string)
	{
		CoAwaitData(fd, fs_open(fname, O_RDONLY));
		for (;;) {
			CoAwaitData(cur, fs_read(fd, content.length()));
			if (cur == nullptr)
				break;
			content += cur;
		}
		CoAwait(fs_close(fd));
		CoReturn(content);
	}
	CoEnd();
};

list<PromisePtr<string>> tasks;
void testLibuv()
{
	auto p = fs_read_all(__FILE__);
	p->onDone([](string s) {
		printf("done: sz=%d\n", s.length());
	});
	p->onError([](std::exception& e) {
		printf("libuv error: %s\n", e.what());
		if (auto i = dynamic_cast<ExceptionWithCode*>(&e)) {
			printf("error code:%d", i->code);
		}
	});
	tasks.push_back(p);
}

int main()
{
	Scheduler sc;
	for (auto i = 0; i < 500; i++)
		testLibuv();
	//testLogin();

	int cnt = 0;
	for (auto run = true; run;) {
		run = sc.updateAll();

		printf("\r%c", "-\\|-/"[cnt++ % 5]);
		std::this_thread::sleep_for(chrono::milliseconds(30));

		uv_run(uv_default_loop(), UV_RUN_ONCE);
	}
}
