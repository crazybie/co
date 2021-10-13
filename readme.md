# Tiny Stackless Coroutine in C++11

based on Duff's device(https://en.wikipedia.org/wiki/Duff%27s_device)

## Hightlights
- super lightweight with only one header file.
- support async & await with simple syntax.
- support await all in parallel.
- can very easily convert callback style api to awaitables.
- auto propagate exceptions to caller.
- can customize the scheduler.
- only need c++14.
- no other dependencies except stl.


## Limitations
- the default scheduler is single threaded.
- memory & performance optimization.


## Compiler support
- gcc > 4.9
- clang > 3.4
- vs 2017 (should work in 2015)




Simple usage example:
```c++

CoFunc(string) fs_read_all(const char* fname)
{
	string content;
	const char* cur;
	int fd;
	CoBegin;
	
	CoAwaitData(fd, fs_open(fname, O_RDONLY));
	for (;;) {
		CoAwaitData(cur, fs_read(fd, content.length()));
		if (cur == nullptr)
			break;
		content += cur;
	}
	CoAwait(fs_close(fd));
	CoReturn(content);
	
	CoEnd()
};


```

See example.cpp for more samples.


### License

[The MIT License](https://github.com/crazybie/co/blob/master/LICENSE)
