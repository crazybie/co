#include <functional>
#include <memory>
#include <list>
#include <exception>

namespace co
{
	using namespace std;

	template<typename... A>
	using Action = function<void(A...)>;

	using ErrorCb = Action<exception_ptr>;

	template<typename T>
	using Ptr = shared_ptr<T>;


#define CoBegin(...) \
    auto __state = 0; \
    co::Ptr<co::PromiseBase> __promise; \
    return std::make_shared<co::Promise<__VA_ARGS__>>([=](const co::Action<__VA_ARGS__>& onOk, const co::ErrorCb& onErr) mutable->co::Ptr<co::PromiseBase> { \
        try { \
            switch ( __state ) { case 0: \

#define CoAwaitData(var, ...) \
                do { __state=__LINE__; return __promise = __VA_ARGS__; case __LINE__: var = std::static_pointer_cast<decltype(__VA_ARGS__)::element_type>(__promise)->getValue(); } while(0)

#define CoAwait(...) \
                do { __state=__LINE__; return __promise = __VA_ARGS__; case __LINE__: __promise->checkError(); } while(0)

#define CoReturn(...) \
                do { onOk(__VA_ARGS__); return nullptr; } while(0) 

#define CoEnd() \
            }; \
        } catch(...) { onErr(std::current_exception()); } return nullptr; \
    });


	class PromiseBase;

	class Scheduler
	{
	public:
		static Scheduler*& instance() { static Scheduler* i; return i; }
		Scheduler() { instance() = this; }
		virtual ~Scheduler() { instance() = nullptr; }
		virtual bool updateAll();
		virtual void add(PromiseBase* i) { pool.push_back(i); }
		virtual void remove(PromiseBase* i) { pool.remove(i); }
	private:
		list<PromiseBase*> pool;
	};


	class PromiseBase
	{
	public:
		enum class State { Failed, Completed, Inprogres } state = State::Inprogres;

		void onError(Action<exception&> cb)
		{
			if (state == State::Failed) {
				try { rethrow_exception(excep); }
				catch (exception& e) { cb(e); }
			}
			else
				errorCb = cb;
		}
		void checkError()
		{
			if (state == State::Failed)
				rethrow_exception(excep);
		}
		virtual void update() = 0;
	protected:
		PromiseBase() { Scheduler::instance()->add(this); }
		PromiseBase(const PromiseBase& r) = delete;
		PromiseBase(PromiseBase&& r) = delete;
		virtual ~PromiseBase()
		{
			auto s = Scheduler::instance();
			if (s) s->remove(this);
		}
		void rejected(exception_ptr e)
		{
			state = State::Failed;
			excep = e;
			if (errorCb) onError(errorCb);
		}
	protected:
		Ptr<PromiseBase> subFsm;
		exception_ptr excep;
		Action<exception&> errorCb;
	};


	template<typename T>
	class Promise : public PromiseBase
	{
	public:
		template<typename F>
		Promise(F&& f) : fsm(move(f))
		{
			callResolved = [this](T v) { resolved(v); };
			callError = [this](exception_ptr err) { rejected(err);  };
			update();
		}
		Promise(nullptr_t) {}
		Promise(T t) { resolved(t); }

		void onDone(Action<T> cb)
		{
			if (state == State::Completed)
				cb(value);
			else
				okCb = cb;
		}
		const T& getValue() { checkError(); return value; }
	protected:
		void update() override
		{
			if (subFsm && subFsm->state == State::Inprogres) {
				return;
			}
			subFsm = fsm(callResolved, callError);
		}
		void resolved(T v)
		{
			value = v;
			state = State::Completed;
			if (okCb) okCb(value);
		}
	private:
		T value;
		Action<T> okCb;
		Action<T> callResolved;
		ErrorCb callError;
		function<Ptr<PromiseBase>(const Action<T>& onOK, const ErrorCb& onErr)> fsm;
	};


	//////////////////////////////////////////////////////////////////////////

	template<typename T>
	using PromisePtr = Ptr<Promise<T>>;

	bool Scheduler::updateAll()
	{
		auto inprogress = false;
		for (auto i : instance()->pool) {
			if (i->state == PromiseBase::State::Inprogres) {
				inprogress = true;
				i->update();
			}
		}
		return inprogress;
	}

	inline static PromisePtr<bool> all(const list<Ptr<PromiseBase>>& l)
	{
		CoBegin(bool)
		{
			auto allDone = true;
			for (auto& i : l) {
				if (i->state == PromiseBase::State::Inprogres) {
					allDone = false;
					break;
				}
			}
			if (allDone) CoReturn(true);
		}
		CoEnd()
	}
}

