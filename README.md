# Wrapper library for memset and secure memset
A simple implementation of ``memset()`` and ``secure_memset()`` / ``memset_explicit()`` functions with the purpose of being easy to use drop-in wrappers.

## Why?
Modern compilers have an optimization known as "Dead Store Elimination" (DSE). This optimization is entirely based around the idea that if a given resource is no longer going to be used (eg: no further memory reads are performed after a certain point in the program), then any final writes that are performed to said memory that do not have any visible usage or side effect can be completely discarded and optimized away.

For instance:
```c
int main() {
  int x = get_some_data();
  memset(&x, 0, sizeof x); // This call gets optimized away, because noone reads x afterward, so
                           // it does NOT have a visible side effect that changes the behaviour of the program.
  return 0;
}
```

For the most part, this optimization is great. The behaviour of the program remains the same, and now it is more performant, because useless instructions are discarded!

But sadly, this optimization is not always desirable, and it thus comes at a great cost in certain security sensitive scenarios. For instance, if sensitive information is temporarily stored in memory for a given secure operation, to ensure that the data cannot be leaked from a crash dump or other forms of memory treading exploits, what most secure programs do is ``memset()`` or ``bzero()`` the buffer that contained the secure information, setting all the bytes to some value like 0, so as to ensure that the data is deleted and no longer accessible. Sadly, the compiler can see this as a dead store if no further reads with operations with side effects are performed, and thus, it performs DSE, making it so that the secret remains intact in memory, exposing sensitive information to bad actors.

The C standard added functions such as ``memset_explicit()`` and ``memset_s()`` for this very purpose, these being functions that the standard mandates that they cannot be optimized away by the compiler and must always appear in the final program when called, but most libraries and compilers do not support them yet even in the year 2026, which has made compiling code that requires safe memset a pain in the ass.

So, the purpose of this library is to offer a drop in replacement, because apparently the standard library has a solution, but everyone seems to refuse to actually implement it.


# Notes regarding implementation details for safe memzero:
Here are some notes and explanations regarding why I implemented things the way that I did.

I know that people are very opinionated about how software should work, so I feel like I must justify why is it that I have not chosen a different path for many decisions on this library.

In short, the answer to any question you could have is either because I do not have more time to work on this, or because any other clever optimization that you could think of actually leads to verifiably slower code, or it has some other cost that I do not like.

As a quick TL;DR for a specific example, platform specific functionality such as ``SecureZeroMemory()`` from Windows is slower than their internal UCRT ``memset()`` implementation due to how it is internally implemented, so it is not worth it to add support for it when a single function pointer dereference is going to have a smaller overhead than using their slower non-optimizable secure memset variant.

## WIN32 ZeroMemory and SecureZeroMemory support:
There is probably no need to add WIN32 specific support. I would like to do so, since, surprisingly, this is actually the implementation that will give me the least issues and be the most stable out of all the platform specific stuff, but it still has some major issues that makes it not worth it.

For starters, ``ZeroMemory()`` is just a wrapper macro around ``memset()``, so obviously it makes no sense to include a whole host of Windows specific headers, with all of their heavy machinery and global namespace pollution, just to include this fucking macro.

Then we have ``SecureZeroMemory()``, which is just a macro that calls the internal function ``RtlSecureZeroMemory()``. This specific function is not that bad, but does its purpose, but again, it requires including a whole host of Windows specific headers, and most people would rather have that on a separate translation unit and wrap around it. The function lives in WinNT.h, but this header cannot be included on its own directly. It is an internal header expected to be included by a larger chain of include dependencies from Windows.h, so you will need even more polution of the global namespace just for the sake of getting a 5 line function to work, which is absolute bonkers.

Not to mention, ``RtlSecureZeroMemory()`` is actually verifiably slower than the UCRT ``memset()``implementation. This is because, under the hood, ``RtlSecureZeroMemory()`` is implemented as follows, as ripped straight from WinNT.h:
```c
FORCEINLINE PVOID RtlSecureZeroMemory( _Out_writes_bytes_all_(cnt) PVOID ptr, _In_ SIZE_T cnt )
{
	volatile char *vptr = (volatile char *)ptr;
	#if defined(_M_AMD64)
		__stosb((PBYTE )((DWORD64)vptr), 0, cnt);
	#else
		while (cnt) {
		#if !defined(_M_CEE) && (defined(_M_ARM) || defined(_M_ARM64))
				__iso_volatile_store8(vptr, 0);
		#else
				*vptr = 0;
		#endif
			vptr++;
			cnt--;
		}
	#endif // _M_AMD64
	return ptr;
}
```

As can be seen from the code, this function just calls ``__stosb()`` if AMD64 support is available, that is, if the code is being compiled for a 64 bit Windows system. All stosb does is generate a rep instruction which repeats storage of 0 values in memory for however many bytes the selected memory region is. This is obviously faster than a raw for loop, and it is fine for relatively small buffers, but after a certain point, it becomes obviously slower than the optimized implementation of ``memset()``, which uses vector instructions when possible and ensures memory alignment constraints.

If the platform is not 64 bit, it performs a naive loop just like my simple, unoptimized and lazy freestanding implementation, which will probably be optimized a little bit by the compiler, but it will remain a byte by byte zeroing operation because the pointer is marked volatile, so obviously slow as fuck compared to standard ``memset()`` implementation techniques.

Meanwhile, Windows' UCRT default implementation of ``memset()`` is actually quite well optimized and has runtime CPU detection, which is what literally every single respectable C standard library implementation of ``memset()`` does in this day and age.

This means that the trick of using a volatile pointer to the ``memset()`` function is actually faster for larger buffers. It just has the cost of a single pointer to a function dereference.

Maybe with some buffer size heuristic, a decision could be made at runtime to choose between the two, but again, that's not worth it in my opinion because of all the other overhead that comes with Windows.h related fuckery.

## Linux/BSD bzero and explicit_bzero support:
On Unix environments such as Linux and BSD, the presence of non-standard C functions such as ``bzero()`` and ``explicit_bzero()`` is entirely dependant on the standard library implementation that you are currently using.

This is an issue that does not exist in Windows, because on Windows, there's only a single major and officially provided standard library implementation. On Linux and BSD, there are multiple different C standard library implementations, and each of those comes with a different set of extensions and functionality.

This additional freedom is great for the most part, but for the purposes of this library, it means that it is not safe to rely on the presence of those functions, as each library offers a different set of non-standard functions.

Unlike other standard library and compiler specific features, these library dependant details cannot be detected through compiler defined macros, so it would require build system specific utilities or user defined macros to be able to detect which specific implementation to use.

In the future, I will probably provide build flags that can be configured by the user to signal which specific implementation to use, allowing internall calls to ``bzero()`` and ``explicit_bzero()`` if the user wishes to do so, but for now, the library is implemented with the objective of allowing the code to compile everywhere out of the box, so things that require external detection methods rather than compiler or header defined macros are going to be ignored for now, until I have time to properly implement support for them.

## C11, C23 or C++26 support for memset_explicit and memset_s:
It is quite sad, but as it stands today, even in the year 2026, checking for C11, C23 and C++26 support is not enough to determine if we can access ``memset_s()`` and ``memset_explicit()`` safely.

This is because these two functions are barely implemented by any of the standard library implementations, as mentioned before.

The specific reasons for which ``memset_explicit()`` is barely offered is unknown to me, but when it comes to ``memset_s()``, I know that it is basically impossible to find anywhere because it is defined in Annex K of the C standard, and thus, it is optional, but in an "all or nothing" manner. It would be reasonable to expect this one extremely useful function to be implemented, but sadly, if you want to be certified for standards compliance, you must be aware of the fact that, if any Annex K functionality is implemented, the standard mandates full Annex K implementation for standards compliance, which is why noone bothers implementing ``memset_s()``, because that is one of the few useful features from that shitty annex, but if implemented, standard library implementors would see themselves forced to implement the rest, and nobody wants to do that.

Thus, they have already made the call to simply ignore it, making it extremely rare to find an implementation that supports ``memset_s()`` even partially.

Also, a lot of standards committee members have begun to to ask for ``memset_s()`` to be deprecated and removed from the standard, so it would not be safe for anyone to rely on this function continuing to exist in the coming future. Yes, this is truly a shitshow.

As far as I'm aware, if I am not mistaken, the support for ``memset_s()`` and ``memset_explicit()`` on C standard library implementations is currently looking something like the following table. If I have made any mistakes when documenting this information, please notify me so that I can properly update this table, and the code so as to ensure that future updates to the code can properly support these platforms.

| Implementation  | memset_explicit | memset_s       |
|-----------------|-----------------|----------------|
| glibc >= 2.43   |     YES         |     NO         |
| FreeBSD 15      |     YES         |     YES        |
| NetBSD 11       |     YES         |     NO         |
| musl            |     NO          |     NO         |
| picolib         |     YES         |  Unknown       |
| MSVC/UCRT       |     NO          |     NO         |

As you can see, only FreeBSD seems to support both functions reliably, so this is quite disheartening to see for such an useful and promissing standard library function, because we pretty much can't use it at all!

## Assembly specific optimizations:
Not worth it, volatile function pointer to memset is the best idea all around because most standard library implementations of memset already have runtime CPU detection to pick an extremely highly optimized routine.

Anything that I could program myself would probably be slower, not because the assembly would be bad, because I could write an extremely optimized assembly routine. The problem would be that it would hardcode one specific code path rather than having every single possible up-to-date CPU architecture specific routine available to pick the best at runtime. Standard library memset has this feature available because thousands of man hours are put into maintaining it, and it is updated every single year to stay up to date with modern CPUs. My implementation would fall behind and become stagnant, so it would be a net loss to use something that would depend on specific hardcoded architectures. What happens when someone wants to support an architecture I did not write a custom routine for, and they do not care about freestanding?

This issue is what happens with most people trying to offer their own hand rolled ``memset_explicit()`` implementations online.

Also, another sad fact is that if you were to match the assembly of memset, the optimizer would instantly recognise the pattern and replace it with a memset call, which would be optimized away with a dead store elimination. So even with ``-ffreestanding`` (or the equivalent non GCC flag) the issue remains the same, the code would be optimized away in secure contexts where the programmer actually knows that DSE is not valid and the call must remain for security purposes.

The only viable path for assembly implementations would be to call memset in raw, and then add a memory barrier, which would potentially be faster than the pointer to memset call, but as I said before, I prefer the convenience of not having to depend on architecture specific instructions, so that the library can support as many platforms as possible out of the box without much effort.

In the future, for the sake of performance, I will slowly add assembly specific implementations for known platforms for the freestanding variant and for ``memset()`` calls with memory barriers, but for now, the main implementation remains fully portable C.

## Why a header only implementation?
I could have separated this between a header and a source file, but my objective is to ensure that this works in all contexts without adding extra overheads in regular memset use cases.

When we have separate header and source files, if we do not compile using LTO and we use separate translation units, then the compiler cannot inspect the internal implementation of each of these functions.

This means that it cannot optimize away the calls, which is precisely what we want for the volatile memset calls. But we do not want to hinder regular memset calls.

Another thing to have into account is that, an alternative design choice, could have been to have something as simple as a header file where we only declare the signature of a ``memset_explicit()`` function, and then on the source file add a simple raw call for ``memset()``, making it so that the assembly is as simple as a jump instruction. Keeping it in separate translation would be enough for the compiler to not be able to determine whether side effects exist or not on this function call, so it could not be optimized away. Obviously, this comes with the issue of making it so that anyone who compiles with LTO without being aware of this would end up having broken code that works but does not actually properly securely zero out memory, defeating entirely the purpose of this library.

That is the reason for which this obvious approach was discarded. Because it would depend entirely on the build approach used by the user, and it would be more fitting of something like an engine or framework than something like a generic library meant for drop-in usage. This library's job is not to dictate how you build your project, so it makes the volatile function pointer compromise for the sake of making it possible to compile and preserve explicit safe memsetting even with -O3 and LTO.

Another approach could be to have an empty function compiled within a separate translation unit and then invoking it as a call after a regular memset, making itso that the compiler cannot tell if there are side effects, so the ``memset()`` call cannot be optimized away, but this remains defeated by LTO.

An alternative would be to use weak linkage, which would achieve the same effect as the volatile function pointer to ``memset()``, and with no issues from LTO, tho it will require compiler specific support for it to be supported, so this will be an alternative implementation that will come in the future, but it will not be the main implementation method. The volatile function pointer will remain as the primary fallback.

# Notes regarding implementation details for compiler memory barriers:
Here are some notes regarding the second part of this project, which are the different methods used to implement compiler memory barriers. The generic barrier.h header file defines a generic compiler memory barrier function which can be used as a memory fence to protect from the optimizer any group of instructions that we want. Basically, think of it as a ``DoNotOptimize()`` function aching to the one from Google Benchmark.

Not to be confused with actual hardware barriers. We're talking about compiler level fences that are used to signal to the compiler that certain memory operations cannot be optimized away.

These memory fences do not generate any actual instructions in the final code. They are no-ops, and thus, are only used as a way to signal to the compiler that certain memory operations take place that cannot be optimized away.

Theoretically, this can be used to implement the ``memset_explicit()`` logic, tho I prefer to keep the implementations separate because of the many implementation details that make ``memzero_secure()`` such an interesting specific case, for instance, platform specific functionality that could be exploited to achieve our purposes.

## Non standard compiler specific extensions and tricks
On GCC and clang, we have access to GNU style volatile inline assembly. We can invoke ``__asm__ __volatile__("":::"memory")`` and it will indicate to the compiler that we are performing volatile memory operations on the current context, so no optimizations should take place.

This works great for GCC, as it respects this instruction completely.

Sadly, clang does not. When using max optimizations in clang, ``__asm__ __volatile__("":::"memory")`` is considered to ambiguous and is simply ignored, as it does not mark which specific memory region is it that we want to protect against the optimizer.

Also, in the case of GCC, using ``__asm__ __volatile__("":::"memory")`` in raw can lead to worse performance than originally intended, as any memory access within the current scope can be considered volatile, so it is better to specify which memory region is it that we want to apply this instruction to.

We can use things like:
```c
T *p = whatever();
// Select one of these, each will be better depending on the platform.
__asm__ __volatile__(""::"m"(*p):"memory")
__asm__ __volatile__(""::"r"(p):"memory")
__asm__ __volatile__(""::"g"(p):"memory")
```
Any of these will finally allow both clang, GCC, and any other compiler than implements properly GNU extensions to ensure that the memory operations performed over a given memory region will not be optimized away, achieving the compiler memory fence that we were looking for.

After multiple tests, it seems to me like "m" works best for clang, and "r" works best for GCC. Many other alternatives exist, and the notation changes a bit if we also want to pass references, but for now, this is more than enough for my intents and purposes.

In the case of MSVC, the best we can do is use ``_ReadWriteBarrier()``, which is the Microsoft equivalent to the GNU extension ``__asm__ __volatile__("":::"memory")`` call. Sadly, ``_ReadWriteBarrier()`` was deprecated, and in the last few versions of MSVC, there are many instances where this call alone has literally no effect whatsoever over the generated code, and instructions still remain optimized away, similar to the behaviour observed in clang.

The best almost zero overhead workaround for an arbitrary function call protection barrier that I could find for MSVC is simply performing a volatime memory access to the desired memory, which tricks the compiler into thinking that the entire memory block could be accessed, thus, side effects cannot be optimized away:
```c
T *p = whatever();
*(volatile char *)p;
```

This does generate an additional single byte memory read tho, so it is not entirely zero overhead, but it does not matter much. It is the small price to pay for tricking the compiler into not optimizing away a call that we care about. Sad that this has to happen, but it is what it is.

In the case of GCC, this has a similar effect to MSVC, but having access to ``__asm__ __volatile__``, it makes no sense to want to pay an additional (although tiny) overhead that can be entirely skipped.

In the case of clang, the optimizer is smart enough to see that, despite the memory read being marked as volatile, we're only reading a single byte, so it optimizes away whatever prior calls were made and simply memsets a single byte of the entire buffer, which is pretty funny, but useless for our intents and purposes.

## standard C atomic_signal_fence and standard C++ std::atomic_signal_fence
Both the C and C++ standards define the magical function ``atomic_signal_fence`` and ``std::atomic_signal_fence`` respectively. These are compiler memory fences, not to be confused with actual hardware fences, like the ones generated by ``atomic_thread_fence`` and ``std::atomic_thread_fence``.

These memory fences do not generate any actual instructions in the final code. They are no-ops, and thus, are only used as a way to signal to the compiler that certain memory operations take place that cannot be optimized away.

These fence functions take a ``memory_order``, which specifies how the fence should handle memory ordering of instructions when the compiler generates the final assembly code.

Sadly, these fence functions are pretty much useless. For starters, they only work according to the standard under GCC. On clang, they can be trivially optimized away, and on MSVC, they are ignored on 90% of cases, just as ``_ReadWriteBarrier()`` is since its deprecation.

This is because, under the hood, they are nothing more than the equivalent of ``__asm__ __volatile__("":::"memory")`` for both GCC and Clang, and ``_ReadWriteBarrier()`` for MSVC, so despite them being part of the standard, they add nothing.

The built in ``__atomic_signal_fence()`` intrinsic for GCC and clang works exactly the same as the standard ones, so they can be invoked without having to include anything, so that could be seen as a plus, but the non standard ``__asm__ __volatile__`` extensions are just much more powerful and actually do what we want them to.

Another problem of these functions is that they do not take a pointer to the specific buffer that we want to perform the fencing over. This means that, as we have already observed, calls such as ``__atomic_signal_fence(__ATOMIC_ACQ_REL)`` or ``atomic_signal_fence(memory_order_acq_rel)`` (same thing, one using GNU extensions, the other using the C standard), are pretty much just equivalent to ``__asm__ __volatile__("":::"memory")``, which as we already know, only works properly on GCC, and is easily optimized away and ignored by clang.

Another issue is the fact that, even tho this is all part of C11, just like ``memset_explicit()``, many compilers just don't support it yet at all. In the case of the C++ version, it's C++23, and thus, almost noone supports it. MSVC requires compilation in ``/std:c++latest`` mode as of writing this, while both GCC and clang don't really need it because they updated their default compilation flags to be modern enough to support this by default. Not a big deal, but something to consider. Basically, you need some pretty modern standards to be able to compile code that uses these calls. On top of that, they are unreliable and sometimes just don't work at all. So, they are not worth it in my opinion.

## Volatile function pointer trick
Just as the memset volatile function pointer trick, we can do the same with an empty ``dummy()`` barrier function, which we'll call in place just after the function call that we want to protect from being optimized away, passing the corresponding memory address as an argument through a volatile function pointer to ``dummy()``.

It has the exact same function pointer call overhead as described before on the ``memset()`` part of the README, so yeah. In any case, the dummy function is empty and does nothing. It's purpose is just to have a function call to generate a volatile pointer to and pass the protected memory region as an argument to trick the compiler into not being able to optimize away the corresponding call.

This trick is the most standard out of them all, and the one that I would say is guaranteed to work pretty much on any compiler. The other tricks are compiler specific but allow us to get less overhead per call, so it is a good idea to have an implementation that does whatever compiler specific tricks, and then falls back to this.

# Summary:
For now, this is the best I could come up with without overthinking too much, or at least, without overthinking more than I already did.

The ``memset()`` implementation on most C standard library implementations across Linux, BSD, Windows, etc, are all pretty good, and far better than any naive loop with a volatile pointer to the data, so the objective of my implementation is to try to not miss out on the great performance of standard ``memset()`` when trying to perform a secure memset call.

This means that, for the compilers where there exist known compiler memory fences with proper results, we can easily exploit those to get a zero overhead implementation that prevents the ``memset()`` call from being optimized away. Or whatever corresponding function call you want to protect with the generic ``barrier()`` function I made on the barrier header.

Sadly, for unknown compilers, the volatile function pointer to the corresponding function call is the best that I could come up with to get the most standards compliant and cross platform behaviour. It is sad that we have to incurr in the performance cost of a function pointer dereference rather than performing a raw function call, but the C and C++ languages do not have any ways to signal to the compiler that you want a certain call to happen always for whatever purposes.

The compiler is just too darn smart for its own good! Except when it is just too darn stupid for our purposes!

