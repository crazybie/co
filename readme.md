# Tiny Stackless Coroutine in C++11

based on Duff's device(https://en.wikipedia.org/wiki/Duff%27s_device)

## Hightlights
- super lightweight with only one header file.
- support async & await with simple syntax.
- support await all in parallel.
- can very easily convert callback style api to awaitables.
- auto propagate exceptions to caller.
- can custmize the scheduler.
- only need c++11.
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


### License

The MIT License

```
Copyright (C) 2018 soniced@sina.com

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
