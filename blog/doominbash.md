# I put Doom in Bash
## heres how.

So let's first establish a baseline, what does it 
mean to put *doom in bash*? Rule of thumb, if it 
is in [GNU Coreutils] which is:
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

And for Doom, I will be using a modified version 
of the original 1993 game. This is no pregnancy
test, we can run real Doom.

## Where it started

One of my friends was the catalyst. Velzie had
originally created an RISC-V emulator in bash to
run Linux. I saw this and saw a fun weekend
project, that I could quickly do, post on the
internet farm some fake internet validation. I had
heard pretty explicitly that the program may
encounter some bugs but I felt I was ready to take
that task on.

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
into the stack. Then from the stack call the
ecall with the given stack address. This would all
break down if the program did not know where the
stack was with the stack pointer unset. This
worked! I set my stack pointer far away from where
my program was loaded into memory and to my
surprise everything seemed to be in order!

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

## The Debugging, And Optimizations

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
bytes.

## References
[1](https://projectf.io/posts/riscv-cheat-sheet/#rv32-abi-registers)
[2](https://github.com/cnlohr/embeddedDOOM)
[3](https://github.com/mpaland/printf)
