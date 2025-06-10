export LC_ALL=C

MEMSIZE=640932000

. src/util-pure.sh
. src/fmt.sh
. src/elf.sh
. src/risc.sh


dumpmem() {
    local i
    for ((i=0; i<MEMSIZE; i++)); do
        printf "%02x" $(memreadbyte $i)
    done
}

dodump() {
    dumpmem | fromhex > dump
}

# parseelf $1 < $1

# loadblob $1
# dtb_size=$(stat -c%s $2)
# dtb_ptr=$((MEMSIZE - dtb_size - 192)) # 192 is completely arbitrary i just wanted it to match with minirv32ima
# i=0
# while read int; do
#     MEMORY[i+(dtb_ptr/4)]=$((int))
#     i=$((i + 1))
# done < <(od -t d4 -An -v -w4 $2)

# init 0 $dtb_ptr


. mem.sh
printf "Memory loaded!\n"

REGS[0]=0
REGS[1]=2147647460
REGS[2]=640921312
REGS[3]=0
REGS[4]=0
REGS[5]=0
REGS[6]=3
REGS[7]=0
REGS[8]=2151563264
REGS[9]=0
REGS[10]=0
REGS[11]=70
REGS[12]=2151563264
REGS[13]=1
REGS[14]=0
REGS[15]=2151559168
REGS[16]=4278124287
REGS[17]=66
REGS[18]=2151563264
REGS[19]=0
REGS[20]=2151563264
REGS[21]=2153196796
REGS[22]=4
REGS[23]=12
REGS[24]=2153196460
REGS[25]=0
REGS[26]=2153196460
REGS[27]=2151559168
REGS[28]=4
REGS[29]=32
REGS[30]=9
REGS[31]=0
PC=2147652752

while true; do
    step $1
done


# a=$(readn 4 < test.bin | tohex)
#     echo "$a"
# echo $(((((0x$a)) & 0xff)))
