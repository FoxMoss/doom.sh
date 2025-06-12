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


. ./mem.sh
printf "Memory loaded!\n"

REGS[0]=0
REGS[1]=2147652896
REGS[2]=640921360
REGS[3]=0
REGS[4]=0
REGS[5]=2152072624
REGS[6]=3
REGS[7]=838860
REGS[8]=640921440
REGS[9]=12
REGS[10]=2152073712
REGS[11]=320
REGS[12]=200
REGS[13]=2152329712
REGS[14]=64000
REGS[15]=4278190080
REGS[16]=64000
REGS[17]=92
REGS[18]=2152073712
REGS[19]=2151563264
REGS[20]=320
REGS[21]=200
REGS[22]=2151563264
REGS[23]=0
REGS[24]=0
REGS[25]=0
REGS[26]=0
REGS[27]=0
REGS[28]=2152071168
REGS[29]=4278190080
REGS[30]=320
REGS[31]=0
PC=2147656964

while true; do
    step $1
done


# a=$(readn 4 < test.bin | tohex)
#     echo "$a"
# echo $(((((0x$a)) & 0xff)))
