# I wrote Doom in pure Bash
## Heres how!

So let's first establish a baseline, what does it 
mean to put *doom in bash*? I will be using GNU 
bash, version 5.1.16. And for programs inside of
bash, if it is in [GNU Coreutils] which is:
```
arch b2sum base32 base64 basename basenc cat chcon
chgrp chmod chown chroot cksum comm coreutils cp
csplit cut date dd df dir dircolors dirname du
echo env expand expr factor false fmt fold groups
head hostid hostname id install join kill link ln
logname ls md5sum mkdir mkfifo mknod mktemp mv
nice nl nohup nproc numfmt od paste pathchk pinky
pr printenv printf ptx pwd readlink realpath rm
rmdir runcon seq sha1sum sha224sum sha256sum
sha384sum sha512sum shred shuf sleep sort split
stat stdbuf stty sum sync tac tail tee test
timeout touch tr true truncate tsort tty uname
unexpand uniq unlink uptime users vdir wc who
whoami yes
```
Then the program is "pure bash".

And for Doom, I will be using a modified version 
of the original 1993 game. This is no pregnancy
test, we can run real Doom.

## Where it started

One of my friends was the catalyst. Velzie was
originally in the process of trying to run Linux
in bash via a RISC-V emulator. I saw this and saw
a fun weekend project that I could quickly do,
post on the internet and farm some fake internet
points. I had heard pretty explicitly that the
program may encounter some bugs but I felt I was
ready to take that task on. Linux at that time was
basically already up and running, how much more
work could it possible be?

## Much more work as it turns out...

So I download the emulator and get it running, the
first thing I realize is how demonstrably slow
this process was going to be. It would take a
minute or longer, just to initialize enough memory
to get anything running. So I add a loading bar. 
```
\x1b[1G\x1b[2KReseting memory %i%%
```
The two ANSI escape sequences just rewrite the
previous line, so you can just continuously
reprint the same line with a new number and have
it update! Bash is a very hard language to debug,
you can't just attach GDB and set a breakpoint to
see why it acts in a specific way. In many cases
debug logs are all you have to go by.

Velzie had some rudimentary elf support that they
had then gutted in the pursuit of running Linux.
After removing some of the magic offsets I was
able to run a test program!

With supporting RISC-V you're able to support
a basic system call interface. Velzie had already
implemented a write with system call 64.
```c
void sys_write(const char *str, size_t len) {
  asm volatile("li a7, 64;"
               "mv a1, %[str];"
               "mv a2, %[len];"
               "ecall;"
               :
               : [str] "r"(str), [len] "r"(len)
               : "a7", "a1", "a2");
}
void _start() { sys_write("Hello world!\n", 14); }
```
Aaaand it just printed garbage to the console. The
pointer it was sending as a1 seemed arbitrary.
After staring at the raw assembly for a good long
while, I checked a cheat sheet[1]. I knew what was
supposed to go on here from my experience with
x86. The program was supposed to load the string
into the stack. Then from the stack, call the
ecall with the given stack address. This would all
break down if the program did not know where the
stack was with the stack pointer unset. This
worked! I set my stack pointer far away from where
my program was loaded into memory and to my
surprise everything seemed to be in order!

```
$ src/test.sh test
Hello World!
```

So I went and started a little bash standard 
library. Every time doom needs a functionality we
can add it there. To get a slightly better hello
world program, I copy paste in the musl `strlen`
implementation. So now I can implement `puts` like
so:
```c
int puts(const char *s) {
  sys_write(s, strlen(s));
  sys_write("\n", 1);
  return 0;
}
```
Conveniently `printf` with no formatting options
gets converted into a `puts` by the compiler. So
we can write a very traditional hello world now!

I started with trying to compile this chlohnr's 
doom fork[2] already built for embedded systems.
Despite this I had a long way from the project
compiling. I stated combing through the linking
errors and polyfilling functionality where I
could. For printf I pulled in an implementation
for embedded hardware[3] and hooked it up to my
systems calls. The only other major requirement I
need to implement was character polling, which can
be implemented with a read with a very fast
timeout. From there of course I just spit the
character code into a register.
```bash
IFS= read -r -t 0.001 -n 1 -s holder;
REGS[11]=$(printf "%d" "'$holder")
```

So by a technicality it builds to a working
executable, whats just stopping me from just
running it? So begins the debugging, and
optimizations...

## The debugging and optimizations.

My first major problem is loading the program, as
in it takes forever. I had grandfathered in this
system:
```bash
for ((j=0; j<p_memsz; j++)); do
    offs=$((p_vaddr+j))
    if ((j > p_filesz)); then
        memwritebyte $offs 0
    else
        memwritebyte $offs $((0x$(eslice $elfbody $((p_offset+j)) 1)))
    fi
done
```
Which is slow because basically anything written
in mostly bash is slow especially for loops. To
write a singe byte I need to preform an expensive
slicing operation, this simply can't do.
Additionally we need to set a whole range of
values to zero, and if the range is mostly zero
why would the CPU ever want to read from there? So
for the new system I make something more robust,
```bash
while read int;
    memwritebyte $((p_vaddr+i)) $((int))
    i=$((i + 1))
done < <(od -t d1 -An -v -w1 --skip-bytes=$((p_offset)) --read-bytes=$p_filesz "$1")
```
`od` is a utility for dumping files. `-t d1` means
dump as a decimal for every byte. I skip to the
region that the previous code was offset, and read
the appropriate amount of bytes. This dramatically
improves performance, making this one snippet
preform on the timescale of minutes instead of
days. 

If you've been paying attention, which in all 
likelihood you haven't. You may have noticed that
my new implementation neglects to reset certain
bytes. Previously I have mentioned that resetting
the CPU memory takes a significant amount of time
enough to need a progress bar. I was also running
into a third issue, my program would keep
reading out of bounds so I would continually up 
the memory. So to kill three birds with one stone,
I added three lines of code to the byte reading
function:
```bash
function memreadbyte {
    local offs=$1
    local align=$((offs%4))
    offs=$((offs/4))

# +++
    if [ -z "${MEMORY[offs]}" ]; then
      MEMORY[offs]=0
    fi
# +++

    echo $(((MEMORY[offs] >> (align*8)) & 0xFF))
}
```
`-z` just checks if given string has zero length
which `MEMORY[offs]` would produce if it had not
been reset. This dramatically improves performance
and I could now run what I thought would be a
working doom. 

So I try it, and it is just crawling, I don't 
even get to the render loop. I have no proof
either the bash emulator works or my port of doom.
I hate long feedback cycles I like to know
immediately when something starts to fail, I don't
want to be debugging both halves of the codebase 
at once. Velzie's also really uncertain in the
validity of their RISC-V emulation, so they
suggest diffing the output of the bash 
implementation with a known good implementation.
The implementation that Velzie was replicating
mini-rv32ima does not support ELFs which I was
relying on to run Doom. I could either implement
ELF support into mini-rv32ima, compile instead to
a raw bin, or use a separate implementation and
only have to reimplement my custom ecalls. I chose
the last option, it seemed like the path of least
resistance at the time. 

My new base is rv32emu. rv32emu is a pure C
implementation of a RISC-V CPU with support for
ELFS, and a couple of other cool features I had to
disable for compatibility sake. I take some time
to replicate my previous work. It's mostly smooth
sailing, as I had already implemented everything I
needed in bash. I adjusted the stacks to be the
same, and I add rv32ima.sh's dump format to
rv32ima which looks something like this:
```
PC: 80024d3c  [0xfd9ff06f] Z:00000000 ra:80024c54
 sp:2633af10  gp:00000000 tp:00000000 t0:804605e0
 t1:80450d59  t2:000ccccc s0:802405c3 s1:00000000
 a0:00000140  a1:00000039 a2:0000000b a3:00000140
 a4:80459f99  a5:80243954 a6:80450d20 a7:802439cc
 s2:00000000  s3:80570a84 s4:803f1000 s5:803e4000
 s6:803e4000  s7:00000000 s8:00000000 s9:00000000
s10:00000000 s11:00000000 t3:000000ff t4:ffffffff
 t5:000000ef  t6:00000000
```
So I run my modified doom through the new and
improved non-bash RISC-V emulator. Good news, the
dumps align perfectly. Bad news, Doom still
doesn't work. It's still difficult to debug,
because I have no GDB, just loads of printfs. I
narrow the issue down to one location. The code
that takes Doom's arbitrary data format --also 
called lumps-- and draws them to screen. Here's
the bugged code with the relevant function 
signatures:
```c
void V_DrawPatchDirect(int x, int y, int scrn, patch_t *patch);
oid *W_CacheLumpName(char *name, int tag);
void M_DrawMainMenu(void) {
  V_DrawPatchDirect(94, 2, 0, W_CacheLumpName("M_DOOM", PU_CACHE));
}
```
W_CacheLumpName is a function that takes the name
you have given it in this case "M_DOOM". Chlonr
had written some code to automatically cache the
results of this function to cut down on memory
usage. It does this by first having Doom run on a
host architecture. This host version of doom is
cut back to not run the game at all, but to
instead extract all needed lumps from the WAD and
store them in a C file. So when the embedded
version would load the data, it wouldn't need to
convert the WAD directly into structs, the host
had already done that for you. If you've spent
enough time with C, this should ringing alarm
bells already. Lets take the example that everyone
else gives:
```c
struct A {
  uint32_t a; // 4 bytes
  uint32_t b; // 4 bytes
};

struct B {
  char a;     // 1 byte
    // 3 padding bytes
  uint32_t b; // 4 bytes
};
int main(int argc, char *argv[]) {
  printf("sizeof(A)=%lu\n", sizeof(struct A)); // sizeof(A)=8
  printf("sizeof(B)=%lu\n", sizeof(struct B)); // sizeof(B)=8
}
```
Despite struct B on paper storing less, they both
end up storing the same amount. Why? Because in
struct B to make sure data stayed aligned to 4 
byte boundaries it pushes B to the next word.
Because a read across two words would be slower
and harder to process by the CPU. We can quickly
see that this padding theory is correct if we find the
distances between the two structs.
```c
struct A a = {};
struct B b = {};
printf("A: &b-&a=%ld\n", (void *)&a.b - (void *)&a.a); // A: &b-&a=4
printf("B: &b-&a=%ld\n", (void *)&b.b - (void *)&b.a); // B: &b-&a=4
```
This behavior happens on both our emulated RISC-V
compiler riscv32-unknown-linux-gnu-gcc and my host
compiler, so something must've been different with
his compilation setup. Luckily the fix is quite
simple. If we tell the compiler to just compile it
the naive way, it yields the expected results.
```c
struct A {
  uint32_t a; // 4 bytes
  uint32_t b; // 4 bytes
} __attribute__((packed));

struct B {
  char a;     // 1 byte
  uint32_t b; // 4 bytes
} __attribute__((packed));
```
```
Output:
sizeof(A)=8
sizeof(B)=5
A: &b-&a=4
B: &b-&a=1
```
This of course could cause issues as GNU states:
> Use of __attribute__((packed)) often results in
fields that don’t have the normal alignment for
their types. Taking the address of such a field
can result in an invalid pointer because of its
improper alignment. Dereferencing such a pointer
can cause a SIGSEGV signal on a machine that
doesn’t, in general, allow unaligned pointers.

[5]

But we can side step those issues, since we are
the CPU if they so arise. Which at the current
moment they haven't.

After this I had a few bugs that must've appeared
while trying to make buildable, so I revert the
doom portion of the codebase. From there I just
replicate each change I make onto the original X11
compatible codebase and my own. With a little
rewrite of the fixed division code. Doom now works
on a RISC-V emulator.

So that's it we're done? Just run it on the bash
RISC-V emulator, what are we waiting for? 

## Waiting, just a lot of waiting.

At this point my workflow changes a bit. I have
a couple terminal windows open at all times. One that's
providing dumped state for every instruction run,
and another just running Doom without logs. Bash
while slow, doesn't need much memory or CPU, so
it's trivial to spin up 5, 10, or even 20
concurrent instances of bash, but we only need 2.
Then I have a third window, with the `objdump` of
my Doom executable checking the program counter
against the location in the executable. If I
notice a certain function is being run a 
significant amount of times, I take that logic and
reimplement it in bash where it will hopefully be
slightly faster. This work of course gets
replicated across to the C implementation except I 
have to now convert my bash back into C. Currently
I have three reimplementation `memcpy`, and two
calls for code drawing and finding lumps. I have
one last tool under my belt that might be one of
the biggest, save states. 

Since the C implementation, should be 1:1 with the
bash version. I can dump the entire memory into a
file and save the registers. From there all I need
to do is convert that data into bash and I have
save states working! This has the added bonus of
making the program able to run completely in bash
without needing to read from any files. Which is
how I can bundle everything up as a singe
distributable bash file.

## So that's it?

Yeah pretty much. I learned a lot about bash in
making this project, and though it's not going to
change someone's life I was able to use some
skills I had picked up just for odd projects. I
also learned a good deal about the RISC-V
instruction set. For each epiphany I had
throughout this entire process I had spent an
unreal amount of time slowly putting down
printfs and making educated guesses. It'll be
weird to be finally free of this project as it's
consumed a good portion of the last month of my
life. Setting up tests to go over night, waking up
in the middle of the night and checking if it was
a success, then the next morning making
improvements.
## References
[1](https://projectf.io/posts/riscv-cheat-sheet/#rv32-abi-registers)
[2](https://github.com/cnlohr/embeddedDOOM)
[3](https://github.com/mpaland/printf)
[4](https://github.com/sysprog21/rv32emu)
[5](https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Packed-Structures.html)
