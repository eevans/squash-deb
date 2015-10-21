# bzip2 Plugin #

The bzip2 plugin provides an interface for libbzip2.

For more information about bzip2 and libbzip2 see
http://www.bzip2.org/

## Flushing ##

The bzip2 library claims to support flushing, and as far as I can tell
the encoder does.  However, when you feed all of the compressed data
received from the flush operation to the decoder it will not output
any data.  This violates the core assumption of flushing from Squash's
point of view, and therefore this plugin does not support flushing.
If you know of a way to force the decoder to decode the data please
[let us know](https://github.com/quixdb/squash/issues/116).

## Codecs ##

- bzip2

## Options ##

### Encoder ###

- **level** (integer, 1-9, default 6): Set block size to 100k ... 900k.  1 will
  result in the fastest compression while 9 will result in the
  highest compression ratio.
- **work-factor** (integer, 0-250, default 30): From the [bzip2
  documentation](http://www.bzip.org/1.0.5/bzip2-manual-1.0.5.html):
  > Parameter workFactor controls how the compression phase behaves
  > when presented with worst case, highly repetitive, input data. If
  > compression runs into difficulties caused by repetitive data, the
  > library switches from the standard sorting algorithm to a
  > fallback algorithm. The fallback is slower than the standard
  > algorithm by perhaps a factor of three, but always behaves
  > reasonably, no matter how bad the input.

### Decoder ###

- **small** (boolean, default false): Causes the decoder to use a
    decoding algorithms which requires less memory, but takes about
    twice as long.

## License ##

The bzip2 plugin is licensed under the [MIT
License](http://opensource.org/licenses/MIT), and libbzip2 is licensed
under the following license (which is the
[http://opensource.org/licenses/Zlib](zlib License) with the addition
of clause #1 and the disclaimer at the end, which are from the
[3-clause BSD License](http://opensource.org/licenses/BSD-3-Clause)):

> This program, "bzip2", the associated library "libbzip2", and all
> documentation, are copyright (C) 1996-2010 Julian R Seward.  All
> rights reserved.
>
> Redistribution and use in source and binary forms, with or without
> modification, are permitted provided that the following conditions
> are met:
>
> 1. Redistributions of source code must retain the above copyright
>    notice, this list of conditions and the following disclaimer.
>
> 2. The origin of this software must not be misrepresented; you must
>    not claim that you wrote the original software.  If you use this
>    software in a product, an acknowledgment in the product
>    documentation would be appreciated but is not required.
>
> 3. Altered source versions must be plainly marked as such, and must
>    not be misrepresented as being the original software.
>
> 4. The name of the author may not be used to endorse or promote
>    products derived from this software without specific prior written
>    permission.
>
> THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
> OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
> WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
> ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
> DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
> DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
> GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
> INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
> WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
> NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
> SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
