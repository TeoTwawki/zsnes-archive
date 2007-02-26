import array

try:
    import psyco
except:
    pass

def uint8(n):
    n &= 255
    if n >= 128:
        n -= 256
    return n


rules = { "NOP":   lambda op, addr, n: [n],
          "CSWAP": lambda op, addr, n: [(n >> 8) & 255, n & 255],
          "CREL":  lambda op, addr, n: [(n & 255), addr + op.bytes + uint8(n >> 8)],
          "R1":    lambda op, addr, n: [addr + op.bytes + uint8(n >> 8)]
        }

class Op:
    def __init__(self, mnemonic, args, opcode, bytes, rule, klass, argShift="0", argOr="00"):
        self.mnemonic = mnemonic
        if args == '""':
            self.args = ''
        else:
            self.args = args
        self.opcode   = int(opcode, 16)
        self.bytes    = int(bytes)
        self.rule     = rules[rule]
        self.klass    = klass
        self.argShift = int(argShift)
        self.argOr    = int(argOr, 16)

ops = [None]*256

for line in file('tools/emutools/spc_asm.tab'):
    if line[0].isalnum():
        op = Op(*line.split())
        ops[op.opcode] = op
        

def disasm_op(mem, pc):
    def readint(addr, length):
        n = 0
        for i in range(length)[::-1]:
            n = n*256 + mem[addr+i]
        return n

    op = ops[mem[pc]]
    args = op.rule(op, pc, readint(pc+1, op.bytes-1))

    s = '%s ' % op.mnemonic

    for c in op.args:
        if c == '*':
            s += "$%x" % args.pop(0)
        else:
            s += c
    
    return (s, pc+op.bytes)

def disasm(mem, start, stop):
    pc = start
    while pc < stop:
        (op, newpc) = disasm_op(mem, pc)
        print "%04x: %-6s" % (pc, op)
        pc = newpc


if __name__ == '__main__':
    import sys
    mem = array.array('B', file(sys.argv[1]).read()[0x100:0x10100])
    disasm(mem, 0x0000, 0x10000)
