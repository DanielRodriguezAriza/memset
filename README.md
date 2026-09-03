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


# Notes regarding implementation details:
Here are some notes and explanations regarding why I implemented things the way that I did.

I know that people are very opinionated about how software should work, so I feel like I must justify why is it that I have not chosen a different path for many decisions on this library.

In short, the answer to any question you could have is either because I do not have more time to work on this, or because any other clever optimization that you could think of actually leads to verifiably slower code, or it has some other cost that I do not like.

As a quick TL;DR for a specific example, platform specific functionality such as ``SecureZeroMemory()`` from Windows is slower than their internal UCRT ``memset()`` implementation due to how it is internally implemented, so it is not worth it to add support for it when a single function pointer dereference is going to have a smaller overhead than using their slower non-optimizable secure memset variant.

## WIN32 specific SecureZeroMemory support:
There is probably no need to add WIN32 specific support, RtlSecureZeroMemory is
actually slower than the default UCRT Windows implementation of memset, since
under the hood, it just calls __stosb() if AMD64 support is available, otherwise,
it performs a naive loop just like my freestanding implementation.
Meanwhile, Windows' implementation of memset is quite optimized and has CPU detection,
like most other platform C library implementations of memset in this day and age.
This means that the volatile pointer trick is actually faster for larger buffers.
It just has the cost of a single pointer to a function.
Maybe with some buffer size heuristic, a decision to choose between the two could
be made at runtime, but that's not worth it in my opinion.

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

## Summary:
For now, this is the best I could come up with without overthinking too much.

