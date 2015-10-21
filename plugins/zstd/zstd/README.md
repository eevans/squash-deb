 **Zstd**, short for Zstandard, is a new lossless compression algorithm, which provides both good compression ratio _and_ speed for your standard compression needs. "Standard" translates into everyday situations which neither look for highest possible ratio nor extreme speed.

It is provided as a BSD-license package, hosted on Github.

|Branch      |Status   |
|------------|---------|
|master      | [![Build Status](https://travis-ci.org/Cyan4973/zstd.svg?branch=master)](https://travis-ci.org/Cyan4973/zstd) |
|dev         | [![Build Status](https://travis-ci.org/Cyan4973/zstd.svg?branch=dev)](https://travis-ci.org/Cyan4973/zstd) |

For a taste of its performance, here are a few benchmark numbers, completed on a Core i7-5600U @ 2.6 GHz, using [fsbench 0.14.3](http://encode.ru/threads/1371-Filesystem-benchmark?p=34029&viewfull=1#post34029), an open-source benchmark program by m^2.

|Name            | Ratio | C.speed | D.speed |
|----------------|-------|--------:|--------:|
|                |       |   MB/s  |  MB/s   |
| [zlib 1.2.8] -6| 3.099 |    21   |   320   |
| **zstd**       |**2.871**|**255**| **531** |
| [zlib 1.2.8] -1| 2.730 |    70   |   300   | 
| [LZ4] HC r131  | 2.720 |    25   |  2100   |
| QuickLZ 1.5.1b6| 2.237 |   370   |   415   |
| LZO 2.06       | 2.106 |   400   |   580   |
| Snappy 1.1.0   | 2.091 |   330   |  1100   |
| [LZ4] r131     | 2.101 |   450   |  2100   |
| LZF 3.6        | 2.077 |   200   |   560   |

[zlib 1.2.8]:http://www.zlib.net/
[LZ4]:http://www.lz4.org/

An interesting feature of zstd is that it can qualify as both a reasonably strong compressor and a fast one.

Zstd delivers high decompression speed, at more than >500 MB/s per core.
Obviously, your exact mileage will vary depending on your target system.

Zstd compression speed will be configurable to fit different situations.
The first available version is the fast one, at ~250 MB/s per core, which is suitable for a few real-time scenarios.
But similar to [LZ4], zstd can offer derivatives trading compression time for compression ratio, keeping decompression properties intact. "Offline compression", where compression time is of little importance because the content is only compressed once and decompressed many times, will likely prefer this setup.

Note that high compression derivatives still have to be developed.
It's a complex area which will require time and benefit from contributions.


Another property zstd is developed for is configurable memory requirement, with the objective to fit into low-memory configurations, or servers handling many connections in parallel.

Zstd entropy stage is provided by [Huff0 and FSE, from Finite State Entrop library](https://github.com/Cyan4973/FiniteStateEntropy).

Zstd has not yet reached "stable" status. Specifically, it doesn't guarantee yet that its current compressed format will remain stable and supported in future versions. It may still change to adapt further optimizations still being investigated. However, the library starts to be pretty robust, able to withstand hazards situations, including invalid input. The library reliability has been tested using [Fuzz Testing](https://en.wikipedia.org/wiki/Fuzz_testing), using both [internal tools](programs/fuzzer.c) and [external ones](http://lcamtuf.coredump.cx/afl). Therefore, you can now safely test zstd, even within production environments.

"Stable Format" is projected sometimes early 2016.

### Branch Policy
The "dev" branch is the one where all contributions will be merged before reaching "master". If you plan to propose a patch, please commit into the "dev" branch or its own feature branch. Direct commit to "master" are not permitted.
