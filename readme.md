# Tiny Stackless Coroutine in C++11

## Hightlights
- only one header file.
- support async & await.
- support await all in parallel.
- can custmize the scheduler.

## Limitations
- the default scheduler is single threaded.
- memory optimization.
- optimizate the default schedule algorithm.


## Compiler support
- gcc > 4.8
- clang > 3.4
- vs 2017




Simple usage example:
```c++

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


```

See example.cpp for more samples.