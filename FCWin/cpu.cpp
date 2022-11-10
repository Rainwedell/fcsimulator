#include "total.h"
#include <QDebug>
// 实现FC模拟器所有的CPU指令
CPU::CPU(): addr_res(0), addr_rel(0), cycles_wait(0), opcode(0), clock_count(0){
    //关联CPU和对应的RAM
    this->p_ram = &Cpubus;
}

void CPU::push_stack(uint8_t value){
    if (reg_sp == 0){
        qDebug() << "cuowu: zhan yijing xieman le" << endl;
        abort();
    }
    p_ram->save(reg_sp + 0x100, value);
    reg_sp--;
}

uint8_t CPU::pull_stack(){
    reg_sp++;
    uint8_t res = p_ram->load(reg_sp + 0x100);
    return res;
}

// 6502有三种中断(按优先度排序, 越后面越优先):

// IRQ/BRK
// NMI
// RESET

// IRQ - Interrupt Request 中断请求 硬件中断请求被执(大致分为Mapper和APU两类)
// BRK - Break 中断指令 当软件中断请求被执行(BRK指令)
// NMI - Non-Maskable Interrupt 不可屏蔽中断 发生在每次垂直空白(VBlank)时, NMI在NTSC制式下刷新次数为 60次/秒, PAL为50次/秒
// RESET在(重新)启动时被触发. ROM被读入内存, 6502跳转至指定的RESET向量
// 也就是说程序一开始执行$FFFC-FFFD指向的地址

// '低地址'在'低地址', '高地址'在'高地址'. 即低8位在\$FFFC, 高8位在\$FFFD.
void CPU::reset(){
    //初始化寄存器值
    reg_a = 0;
    reg_x = 0;
    reg_y = 0;
    reg_sp = 0xfd;
    reg_sf.set_i(true); //开始时屏蔽IRQ中断
    reg_sf.set_u(true);
    uint8_t lo8 = p_ram->load(0xFFFC); //小端序的读取方法，高8位存在高字节里面，低8位存在低字节里面
    uint8_t hi8 = p_ram->load(0xFFFD);
    reg_pc = uint16_t(hi8 << 8) + lo8;
}

void CPU::irq(){
    if (reg_sf.get_i() == 0){ //判断中断是否被屏蔽了。0为允许IRQ中断，1为屏蔽
        //1.入栈（PC和Status寄存器）
        push_stack(reg_pc >> 8);
        push_stack(reg_pc & 0xFF);
        //reg_sf.set_b(false);
        push_stack(reg_sf.data);
        reg_sf.set_i(true);
        //2.从中断地址处获取新的PC值
        uint8_t lo8 = p_ram->load(0xFFFE); //
        uint8_t hi8 = p_ram->load(0xFFFF);
        reg_pc = uint16_t(hi8 << 8) + lo8;
        //3.IRQ中断需要7个周期
        cycles_wait = 7;
    }
}

void CPU::nmi(){
    //1.入栈
    push_stack(reg_pc >> 8);
    push_stack(reg_pc & 0xFF);
    //reg_sf.set_b(false);
    reg_sf.set_b(false);
    reg_sf.set_u(true);
    reg_sf.set_i(true);
    push_stack(reg_sf.data);
    //2.从中断地址处获取新的PC值
    uint8_t lo8 = p_ram->load(0xFFFA); //
    uint8_t hi8 = p_ram->load(0xFFFB);
    reg_pc = uint16_t(hi8 << 8) + lo8;
    //3.NMI周期为7
    //qDebug() << "NMI, reg_pc = " << reg_pc << endl;
    cycles_wait = 7;
}

// 根据文档定义(不需要深究)
void CPU::dma_sleep(){
    if (clock_count & 1){
        //奇数周期需要sleep 514个CPU时钟周期
        cycles_wait += 514;
    }else{
        //偶数周期需要sleep 513个CPU时钟周期
        cycles_wait += 513;
    }
}

int CPU::IMP(){
    return 0;
}

int CPU::IMM(){
    addr_res = reg_pc;
    reg_pc++;
    oprand_for_log = p_ram->load(addr_res);
    return 0;
}

int CPU::ZP0(){
    addr_res = p_ram->load(reg_pc);
    reg_pc++;
    addr_res &= 0x00FF;
    oprand_for_log = uint16_t(addr_res);
    return 0;
}

int CPU::ZPX(){
    oprand_for_log = p_ram->load(reg_pc);
    addr_res = p_ram->load(reg_pc) + reg_x;
    reg_pc++;
    addr_res &= 0x00FF;
    return 0;
}

int CPU::ZPY(){
    oprand_for_log = p_ram->load(reg_pc);
    addr_res = p_ram->load(reg_pc) + reg_y;
    reg_pc++;
    addr_res &= 0x00FF;
    return 0;
}

int CPU::REL(){
    addr_rel = p_ram->load(reg_pc);
    oprand_for_log = uint16_t(addr_rel);
    reg_pc++;
    if (addr_rel & 0x80)
        addr_rel |= 0xFF00;
    return 0;
}

int CPU::ABS(){
    uint8_t lo8 = p_ram->load(reg_pc);
    uint8_t hi8 = p_ram->load(reg_pc + 1);
    reg_pc += 2;
    addr_res = uint16_t(hi8 << 8) + lo8;
    oprand_for_log = uint16_t(addr_res);
    return 0;
}

int CPU::ABX(){
    uint8_t lo8 = p_ram->load(reg_pc);
    uint8_t hi8 = p_ram->load(reg_pc + 1);
    reg_pc += 2;
    addr_res = uint16_t(hi8 << 8) + lo8 + reg_x;
    oprand_for_log = uint16_t((hi8 << 8) + lo8);
    //偏移了X之后如果发生了翻页，则需要多加一个时钟周期
    if ((hi8 << 8) != (addr_res & 0xFF00))
        return 1;
    else
        return 0;
}

int CPU::ABY(){
    uint8_t lo8 = p_ram->load(reg_pc);
    uint8_t hi8 = p_ram->load(reg_pc + 1);
    reg_pc += 2;
    addr_res = uint16_t(hi8 << 8) + lo8 + reg_y;
    oprand_for_log = uint16_t((hi8 << 8) + lo8);
    //偏移了Y之后如果发生了翻页，则需要多加一个时钟周期
    if ((hi8 << 8) != (addr_res & 0xFF00))
        return 1;
    else
        return 0;
}

int CPU::IND(){
    uint8_t p_lo8 = p_ram->load(reg_pc);
    uint8_t p_hi8 = p_ram->load(reg_pc + 1);
    reg_pc += 2;
    uint16_t ptr = uint16_t(p_hi8 << 8) + p_lo8;
    oprand_for_log = ptr;
    if (p_lo8 == 0xFF)
        addr_res = (p_ram->load(ptr & 0xFF00) << 8) + (p_ram->load(ptr + 0));
    else
        addr_res = (p_ram->load(ptr + 1) << 8) + (p_ram->load(ptr));
    return 0;
}

int CPU::IZX(){
    uint8_t ptr = p_ram->load(reg_pc);
    oprand_for_log = ptr;
    reg_pc++;
    uint8_t lo8 = p_ram->load((ptr + reg_x) & 0x00FF);
    uint8_t hi8 = p_ram->load((ptr + reg_x + 1) & 0x00FF);
    addr_res = (hi8 << 8) + lo8;
    return 0;
}

int CPU::IZY(){
    uint8_t ptr = p_ram->load(reg_pc);
    oprand_for_log = ptr;
    reg_pc++;
    uint8_t lo8 = p_ram->load(ptr & 0x00FF);
    uint8_t hi8 = p_ram->load((ptr + 1) & 0x00FF);
    addr_res = (hi8 << 8) + lo8 + reg_y;
    //偏移了Y之后如果发生了翻页，则需要多加一个时钟周期
    if ((hi8 << 8) != (addr_res & 0xFF00))
        return 1;
    else
        return 0;
}

// 累加器,存储器,进位标志C相加,结果送累加器A. 影响FLAG: S(ign), Z(ero), C(arry), (o)V(erflow),
// src = READ(address);
// uint16_t result16 = A + src + (CF ? 1 : 0);
// CHECK_CFLAG(result16>>8);
// uint8_t result8 = result16;
// CHECK_VFLAG(!((A ^ src) & 0x80) && ((A ^ result8) & 0x80));
// A = result8;
// CHECK_ZSFLAG(A);
int CPU::ADC()
{
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
    //2.加法计算，并写入标志位
    uint16_t sum = reg_a + operand + reg_sf.get_c();
    reg_sf.set_c(sum >= 256);
    reg_sf.set_v((reg_a ^ sum) & (operand ^ sum) & 0x80);
    //reg_sf.set_v((~((uint16_t)reg_a ^ (uint16_t)operand) & ((uint16_t)reg_a ^ (uint16_t)sum)) & 0x0080);
    reg_sf.set_z((sum & 0xFF) == 0);
    reg_sf.set_n(sum & 0x80);
    reg_a = sum & 0xFF;
    return 1;
}

// 与运算, 影响FLAG:Z(ero),S(ign),
// A &= READ(address);
// CHECK_ZSFLAG(A);
int CPU::AND(){
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
     //2.计算a寄存器与该数值的与值
    reg_a = reg_a & operand;
    reg_sf.set_z(reg_a == 0);
    reg_sf.set_n(reg_a & 0x80);
    return 1;
}
// 累加器A, 或者存储器单元算术按位左移一位. 最高位移动到C, 最低位0. 影响FLAG: S(ign) Z(ero) C(arry), 
// ASL A:
// CHECK_CFLAG(A>>7);
// A <<= 1;
// CHECK_ZSFLAG(A);

// // 其他情况
// tmp = READ(address);
// CHECK_CFLAG(tmp>>7);
// tmp <<= 1;
// WRITE(address, tmp);
// CHECK_ZSFLAG(tmp);
int CPU::ASL(){
    if (inst_table[opcode].addrmode == &CPU::IMP){
        //IMP(Accumulator)
        uint16_t temp = uint16_t(reg_a << 1);
        reg_sf.set_c(temp >= 0x100);
        reg_sf.set_z((temp & 0x00FF) == 0);
        reg_sf.set_n(temp & 0x80);
        reg_a = temp & 0x00FF;
    }else{
        //其他模式下，先取走操作符，再对操作符赋值，最后再写回去
        uint8_t operand = p_ram->load(addr_res);
        uint16_t temp = uint16_t(operand << 1);
        reg_sf.set_c(temp >= 0x100);
        reg_sf.set_z((temp & 0x00FF) == 0);
        reg_sf.set_n(temp & 0x80);
        p_ram->save(addr_res, temp & 0x00FF);
    }
    return 0;
}

// 如果标志位C(arry) = 0[即没进位]则跳转，否则继续, 影响FLAG: (无)，
// if (!CFLAG) PC = address;
int CPU::BCC(){
    //C=0
    uint8_t cycles_add = 0;
    if (reg_sf.get_c() == 0){
        addr_res = reg_pc + addr_rel;
        //如果新老PC寄存器值不在同一页上，则增加2个时钟周期，否则增加一个时钟周期
        if ((addr_res & 0xFF00) != (reg_pc & 0xFF00))
            cycles_add = 2;
        else
            cycles_add = 1;
        reg_pc = uint16_t(addr_res);
    }
    return -cycles_add;
}

// 如果标志位C(arry) = 1[即进位了]则跳转，否则继续, 影响FLAG: (无)
// if (CFLAG) PC = address;
int CPU::BCS(){
    //C=1 进入分支
    uint8_t cycles_add = 0;
    if (reg_sf.get_c() == 1){
        addr_res = reg_pc + addr_rel;
        //如果新老PC寄存器值不在同一页上，则增加2个时钟周期，否则增加一个时钟周期
        if ((addr_res & 0xFF00) != (reg_pc & 0xFF00))
            cycles_add = 2;
        else
            cycles_add = 1;
        reg_pc = uint16_t(addr_res);
    }
    return -cycles_add;
}

// 分支跳转指令  如果标志位Z(ero) = 1[即相同]则跳转，否则继续, 影响FLAG: (无)，
// if (ZFLAG) PC = address;
// +1s 跳转同一页面 * +2s 跳转不同页面
// 当然, 如果没有实行跳转则花费2周期, 下同.
int CPU::BEQ(){
    //Z=1
    uint8_t cycles_add = 0;
    if (reg_sf.get_z() == 1){
        addr_res = reg_pc + addr_rel;
        //
        if ((addr_res & 0xFF00) != (reg_pc & 0xFF00))
            cycles_add = 2;
        else
            cycles_add = 1;
        reg_pc = uint16_t(addr_res);
    }
    return -cycles_add;
}

int CPU::BIT(){
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
    // 2.计算结果
    reg_sf.set_z((reg_a & operand) == 0);
    reg_sf.set_v(operand & (1 << 6));
    reg_sf.set_n(operand & (1 << 7));
    return 0;
}

// 如果标志位S(ign) = 1[即负数]则跳转，否则继续, 影响FLAG: (无),
// if (SFLAG) PC = address;
int CPU::BMI(){
    //N=1 则进入分支
    uint8_t cycles_add = 0;
    if (reg_sf.get_n() == 1){
        addr_res = reg_pc + addr_rel;
        //如果新老PC寄存器值不在同一页上，则增加2个时钟周期，否则增加一个时钟周期
        if ((addr_res & 0xFF00) != (reg_pc & 0xFF00))
            cycles_add = 2;
        else
            cycles_add = 1;
        reg_pc = uint16_t(addr_res);
    }
    return -cycles_add;
}

// 分支跳转指令  如果标志位Z(ero) = 0[即相同]则跳转，否则继续, 影响FLAG: (无)，
// if ( !ZFLAG) PC = address;
// +1s 跳转同一页面 * +2s 跳转不同页面
// 当然, 如果没有实行跳转则花费2周期, 下同.
int CPU::BNE(){
    //Z=0
    uint8_t cycles_add = 0;
    if (reg_sf.get_z() == 0){
        addr_res = reg_pc + addr_rel;
        //如果新老PC寄存器值不在同一页上，则增加2个时钟周期，否则增加一个时钟周期
        if ((addr_res & 0xFF00) != (reg_pc & 0xFF00))
            cycles_add = 2;
        else
            cycles_add = 1;
        reg_pc = uint16_t(addr_res);
    }
    return -cycles_add;
}
// 如果标志位S(ign) = 1[即正数]则跳转，否则继续, 影响FLAG: (无),
// if (!SFLAG) PC = address;
// +1s 跳转同一页面 * +2s 跳转不同页面
int CPU::BPL(){
    //N=0
    uint8_t cycles_add = 0;
    if (reg_sf.get_n() == 0){
        addr_res = reg_pc + addr_rel;
        //如果新老PC寄存器值不在同一页上，则增加2个时钟周期，否则增加一个时钟周期
        if ((addr_res & 0xFF00) != (reg_pc & 0xFF00))
            cycles_add = 2;
        else
            cycles_add = 1;
        reg_pc = uint16_t(addr_res);
    }
    return -cycles_add;
}

// 强制中断 影响FLAG: I(nterrupt), 
// ++PC;
// PUSH(PC>>8);
// PUSH(PC & 0xFF);
// PUSH(P | FLAG_R | FLAG_B);
// IF = 1;
// PC = READ(IRQ);
// PC |= READ(IRQ + 1) << 8;
int CPU::BRK(){ //
    reg_pc++;
    //1.入栈
    push_stack(reg_pc >> 8);
    push_stack(reg_pc & 0xFF);
    reg_sf.set_b(true);
    reg_sf.set_i(true);
    push_stack(reg_sf.data);
    reg_sf.set_b(false);
    //2.从中断地址获取PC值
    uint8_t lo8 = p_ram->load(0xFFFE); //
    uint8_t hi8 = p_ram->load(0xFFFF);
    reg_pc = uint16_t(hi8 << 8) + lo8;
    return 0;
}

// 如果标志位(o)V(erflow) = 0[即没有溢出]则跳转，否则继续, 影响FLAG: (无),
// if (!VFLAG) PC = address;
int CPU::BVC(){
    //V=0 则进入分支
    uint8_t cycles_add = 0;
    if (reg_sf.get_v() == 0){
        addr_res = reg_pc + addr_rel;
        //如果新老PC寄存器值不在同一页上，则增加2个时钟周期，否则增加一个时钟周期
        if ((addr_res & 0xFF00) != (reg_pc & 0xFF00))
            cycles_add = 2;
        else
            cycles_add = 1;
        reg_pc = uint16_t(addr_res);
    }
    return -cycles_add;
}

// 如果标志位(o)V(erflow) = 1[即溢出]则跳转，否则继续, 影响FLAG: (无),
// if (VFLAG) PC = address;
int CPU::BVS(){
    //V=1 则进入分支
    uint8_t cycles_add = 0;
    if (reg_sf.get_v() == 1){
        addr_res = reg_pc + addr_rel;
        //如果新老PC寄存器值不在同一页上，则增加2个时钟周期，否则增加一个时钟周期
        if ((addr_res & 0xFF00) != (reg_pc & 0xFF00))
            cycles_add = 2;
        else
            cycles_add = 1;
        reg_pc = uint16_t(addr_res);
    }
    return -cycles_add;
}

// 清除进位标志C, 影响FLAG: C(arry), 
// CF = 0;
int CPU::CLC(){
    reg_sf.set_c(false);
    return 0;
}

// 清除十进制模式标志D, 影响FLAG: D(Decimal),
// DF = 0;
int CPU::CLD(){
    reg_sf.set_d(false);
    return 0;
}

// 清除中断禁止标志I, 影响FLAG: I(nterrupt-disable), 
// IF = 0;
int CPU::CLI(){
    reg_sf.set_i(false);
    return 0;
}

// 清除溢出标志V, 影响FLAG: (o)V(erflow),
// VF = 0;
int CPU::CLV(){
    reg_sf.set_v(false);
    return 0;
}

// 比较储存器值与累加器A. 影响FLAG: C(arry), S(ign), Z(ero),
// uint16_t result16 = (uint16_t)A - (uint16_t)READ(address);
// CF = result16 < 0x100;
// CHECK_ZSFLAG((uint8_t)result16);
int CPU::CMP(){
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
    //2.将寄存器A与操作符进行比较
    uint16_t temp = reg_a - operand;
    reg_sf.set_c(reg_a >= operand);
    reg_sf.set_z((temp & 0x00FF) == 0);
    reg_sf.set_n(bool(temp & 0x0080)); //
    return 1;
}

// 比较储存器值与变址寄存器X. 影响FLAG: C(arry), S(ign), Z(ero),
// uint16_t result16 = (uint16_t)X - (uint16_t)READ(address);
// CF = result16 < 0x100;
// CHECK_ZSFLAG((uint8_t)result16);
int CPU::CPX(){
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
    //2.将寄存器X与操作符进行比较
    uint16_t temp = reg_x - operand;
    reg_sf.set_c(reg_x >= operand);
    reg_sf.set_z((temp & 0x00FF) == 0);
    // debug：这里不能写成temp < 0。因为如果temp为-128再往下，<0与&0x0080的计算结果并不一致
    reg_sf.set_n(bool(temp & 0x0080)); 
}

// 比较储存器值与变址寄存器Y. 影响FLAG: C(arry), S(ign), Z(ero),
// uint16_t result16 = (uint16_t)Y - (uint16_t)READ(address);
// CF = result16 < 0x100;
// CHECK_ZSFLAG((uint8_t)result16);
int CPU::CPY(){
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
    //2.将寄存器Y与操作符进行比较
    uint16_t temp = reg_y - operand;
    reg_sf.set_c(reg_y >= operand);
    reg_sf.set_z((temp & 0x00FF) == 0);
    reg_sf.set_n(bool(temp & 0x0080)); //
    return 0;
}

// 存储器单元内容减1, 影响FLAG:Z(ero),S(ign), 
// tmp = READ(address);
// --tmp;
// WRITE(address, tmp);
// CHECK_ZSFLAG(tmp);
int CPU::DEC(){
    //1.取值
    uint8_t operand = p_ram->load(addr_res);
    //2.递减
    uint16_t res = operand - 1;
    p_ram->save(addr_res, res & 0x00FF);
    reg_sf.set_z((res & 0x00FF) == 0);
    reg_sf.set_n(bool(res & 0x0080));
    return 0;
}
// 变址寄存器X内容-1, 影响FLAG:Z(ero),S(ign), 
// --X;
// CHECK_ZSFLAG(X);
int CPU::DEX(){
    reg_x--;
    reg_sf.set_z(reg_x == 0);
    reg_sf.set_n(bool(reg_x & 0x0080));
    return 0;
}

// 变址寄存器Y内容-1, 影响FLAG:Z(ero),S(ign), 
// --Y;
// CHECK_ZSFLAG(Y);
int CPU::DEY(){
    reg_y--;
    reg_sf.set_z(reg_y == 0);
    reg_sf.set_n(bool(reg_y & 0x0080));
    return 0;
}

// 存储器单元与累加器做或运算, 影响FLAG:Z(ero),S(ign), 
// A ^= READ(address);
// CHECK_ZSFLAG(A);
int CPU::EOR(){
    //1.取值
    uint8_t operand = p_ram->load(addr_res);
    //2.异或
    reg_a = reg_a ^ operand;
    reg_sf.set_z(reg_a == 0);
    reg_sf.set_n(bool(reg_a & 0x0080));
    return 1;
}

// 存储器单元内容+1, 影响FLAG:Z(ero),S(ign), 
// tmp = READ(address);
// ++tmp;
// WRITE(address, tmp);
// CHECK_ZSFLAG(tmp);
int CPU::INC(){
    //1取值
    uint8_t operand = p_ram->load(addr_res);
    //2.递增
    uint16_t res = operand + 1;
    p_ram->save(addr_res, res & 0x00FF);
    reg_sf.set_z((res & 0x00FF) == 0);
    reg_sf.set_n(bool(res & 0x0080));
    return 0;
}

// 变址寄存器X内容+1, 影响FLAG:Z(ero),S(ign), 
// ++X;
// CHECK_ZSFLAG(X);
int CPU::INX(){
    //
    reg_x++;
    reg_sf.set_z(reg_x == 0);
    reg_sf.set_n(bool(reg_x & 0x0080));
    return 0;
}

// 变址寄存器Y内容+1, 影响FLAG:Z(ero),S(ign), 
// ++Y;
// CHECK_ZSFLAG(Y);
int CPU::INY(){
    //递增
    reg_y++;
    reg_sf.set_z(reg_y == 0);
    reg_sf.set_n(bool(reg_y & 0x0080));
    return 0;
}

// 无条件跳转, 影响FLAG: (无),
// PC = address;
int CPU::JMP(){
    reg_pc = uint16_t(addr_res);
    return 0;
}

// 跳转至子程序, 记录该条指令最后的地址(即当前PC-1, 或者说JSR代码\$20所在地址+2), 影响FLAG: (无), 
// --PC;
// PUSH(PC >> 8);
// PUSH(PC & 0xFF);
// PC = address;
int CPU::JSR(){
    //1.将pc寄存器的值写入到stkp寄存器（栈指针）里面
    push_stack((reg_pc - 1) >> 8);
    push_stack((reg_pc - 1) & 0xFF);
    //2.将寻址结果赋值给PC寄存器
    reg_pc = uint16_t(addr_res);
    return 0;
}

// 由存储器取数送入累加器A, 影响FLAG: Z(ero),S(ign)
// A = READ(address);
// CHECK_ZSFLAG(A);
int CPU::LDA(){
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
    //2.将操作符赋值给A寄存器
    reg_a = operand;
    reg_sf.set_z(reg_a == 0);
    reg_sf.set_n(bool(reg_a & 0x0080));
    return 1;
}

// 由存储器取数送入变址寄存器X, 影响FLAG: Z(ero),S(ign)
// X = READ(address);
// CHECK_ZSFLAG(X);
int CPU::LDX(){
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
     //2.将操作符赋值给X寄存器
    reg_x = operand;
    reg_sf.set_z(reg_x == 0);
    reg_sf.set_n(bool(reg_x & 0x0080));
    return 1;
}

// 由存储器取数送入变址寄存器Y, 影响FLAG: Z(ero),S(ign), 
// Y = READ(address);
// CHECK_ZSFLAG(Y);
int CPU::LDY(){
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
    //2.将操作符赋值给Y寄存器
    reg_y = operand;
    reg_sf.set_z(reg_y == 0);
    reg_sf.set_n(bool(reg_y & 0x0080));
    return 1;
}

// 累加器A, 或者存储器单元逻辑按位右移一位. 最低位回移进C, 最高位变0,影响FLAG: S(ign) Z(ero) C(arry), 
// // LSR A:
// CHECK_CFLAG(A & 1);
// A >>= 1;
// CHECK_ZSFLAG(A);

// // 其他情况
// tmp = READ(address);
// CHECK_CFLAG(tmp & 1);
// tmp >>= 1;
// WRITE(address, tmp);
// CHECK_ZSFLAG(tmp);
int CPU::LSR(){
    if (inst_table[opcode].addrmode == &CPU::IMP){
        //IMP(Accumulator)累加器寻址模式下，直接赋值给A寄存器
        uint16_t temp = uint16_t(reg_a >> 1);
        reg_sf.set_c(reg_a & 0x0001);
        reg_sf.set_z((temp & 0x00FF) == 0);
        reg_sf.set_n(temp & 0x80);
        reg_a = temp & 0x00FF;
    }else{
        //其他模式下，先取走操作符，再对操作符赋值，最后再写回去
        uint8_t operand = p_ram->load(addr_res);
        uint16_t temp = uint16_t(operand >> 1);
        reg_sf.set_c(operand & 0x0001);
        reg_sf.set_z((temp & 0x00FF) == 0);
        reg_sf.set_n(temp & 0x80);
        p_ram->save(addr_res, temp & 0x00FF);
    }
    return 0;
}
// NOP指令不做任何事，需要两个周期
int CPU::NOP(){
    switch (opcode) {
    case 0x1C:
    case 0x3C:
    case 0x5C:
    case 0x7C:
    case 0xDC:
    case 0xFC:
        return 1;
        break;
    }
    return 0;
}

// 存储器单元与累加器做或运算, 影响FLAG:Z(ero),S(ign), 
// A |= READ(address);
// CHECK_ZSFLAG(A);
int CPU::ORA(){
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
    //2.将寄存器A与操作符进行或操作
    reg_a = reg_a | operand;
    reg_sf.set_z(reg_a == 0);
    reg_sf.set_n(bool(reg_a & 0x0080));
    return 1;
}

// 累加器A压入栈顶(栈指针SP-1). 影响FLAG:(无)
// PUSH(A);
int CPU::PHA(){
    push_stack(reg_a);
    return 0;
}

// 将状态FLAG压入栈顶, 影响FLAG:(无),
// PUSH(P | FLAG_B | FLAG_R);
int CPU::PHP(){
    push_stack(reg_sf.data | (1 << 4) | (1 << 5));
    reg_sf.set_b(false);
    reg_sf.set_u(false);
    return 0;
}

int CPU::PLA(){
    reg_a = pull_stack();
    reg_sf.set_z(reg_a == 0);
    reg_sf.set_n(bool(reg_a & 0x0080));
    return 0;
}
// push的反操作 pop. 会影响S(ign) Z(ero) , 
// A = POP();
// CHECK_ZSFLAG(A);
int CPU::PLP(){
    reg_sf.data = pull_stack();
    reg_sf.set_u(true);
    return 0;
}
// 累加器A, 或者储存器内容 连同C位 按位循环左移一位, 影响FLAG: S(ign) Z(ero) C(arry)
// int16_t src = A_M;
// src <<= 1;
// if (CF) src |= 0x1;
// CHECK_CFLAG(src > 0xff);
// A_M = src;
// CHECK_ZSFLAG(A_M);
int CPU::ROL(){
    if (inst_table[opcode].addrmode == &CPU::IMP){
        //IMP(Accumulator)累加器寻址模式下，直接赋值给A寄存器
        uint16_t temp = uint16_t(reg_a << 1) | reg_sf.get_c(); //
        reg_sf.set_c(temp >= 0x100);
        reg_sf.set_z((temp & 0x00FF) == 0);
        reg_sf.set_n(temp & 0x80);
        reg_a = temp & 0x00FF;
    }else{
        //其他模式下，先取走操作符，再对操作符赋值，最后再写回去
        uint8_t operand = p_ram->load(addr_res);
        uint16_t temp = uint16_t(operand << 1) | reg_sf.get_c();
        reg_sf.set_c(temp >= 0x100);
        reg_sf.set_z((temp & 0x00FF) == 0);
        reg_sf.set_n(temp & 0x80);
        p_ram->save(addr_res, temp & 0x00FF);
    }
    return 0;
}

// 累加器A, 或者储存器内容 连同C位 按位循环右移一位, 影响FLAG: S(ign) Z(ero) C(arry)
// uint16_t src = A_M;
// if (CF) src |= 0x100;
// CF = src & 1;
// src >> 1;
// A_M = src;
// CHECK_ZSFLAG(A_M);
int CPU::ROR(){
    //
    if (inst_table[opcode].addrmode == &CPU::IMP){
        //IMP(Accumulator)累加器寻址模式下，直接赋值给A寄存器
        uint16_t temp = uint16_t(reg_a >> 1) | uint16_t(reg_sf.get_c() << 7); //
        reg_sf.set_c(reg_a & 0x0001);
        reg_sf.set_z((temp & 0x00FF) == 0);
        reg_sf.set_n(temp & 0x80);
        reg_a = temp & 0x00FF;
    }else{
        //其他模式下，先取走操作符，再对操作符赋值，最后再写回去
        uint8_t operand = p_ram->load(addr_res);
        uint16_t temp = uint16_t(operand >> 1) | uint16_t(reg_sf.get_c() << 7);
        reg_sf.set_c(operand & 0x0001);
        reg_sf.set_z((temp & 0x00FF) == 0);
        reg_sf.set_n(temp & 0x80);
        p_ram->save(addr_res, temp & 0x00FF);
    }
    return 0;
}

// 中断返回,
// P = POP();
// // 无视BIT4 BIT5
// RF = 1;
// BF = 0;

// PC = POP();
// PC |= POP() << 8;
int CPU::RTI(){
    //中断返回: 从栈指针里面连续取值，赋值给status和pc寄存器
    reg_sf.data = pull_stack();
    reg_sf.set_b(false);
    reg_sf.set_u(false);
    uint8_t pc_lo8 = pull_stack();
    uint8_t pc_hi8 = pull_stack();
    reg_pc = uint16_t(pc_hi8 << 8) + pc_lo8;
    return 0;
}

// JSR逆操作, 从子程序返回. 返回之前记录的位置+1, 影响FLAG: (无)，
// PC = POP();
// PC |= POP() << 8;
// ++PC;
int CPU::RTS(){
    //子程序退出 从栈指针里面取值，赋值给PC寄存器
    uint8_t pc_lo8 = pull_stack();
    uint8_t pc_hi8 = pull_stack();
    reg_pc = uint16_t(pc_hi8 << 8) + pc_lo8;
    reg_pc++;
    return 0;
}

// 从累加器减去存储器和进位标志C,结果送累加器A. 影响FLAG: S(ign), Z(ero), C(arry), (o)V(erflow),
// src = READ(address);
// uint16_t result16 = A - src - (CF ? 0 : 1);
// CHECK_CFLAG(!(result16>>8));
// uint8_t result8 = result16;
// CHECK_VFLAG(((A ^ result8) & 0x80) && ((A ^ src) & 0x80));
// A = result8;
// CHECK_ZSFLAG(A);
int CPU::SBC()
{
    //1.先取走addr_res对应的数值
    uint8_t operand = p_ram->load(addr_res);
    //2.加法计算，并写入标志位
    uint16_t sub = reg_a - operand - (!reg_sf.get_c());
    //reg_sf.set_c(sub >= 256);
    reg_sf.set_c(!(sub & 0x100));
    reg_sf.set_v((reg_a ^ sub) & ((~operand) ^ sub) & 0x80);
    reg_sf.set_z((sub & 0xFF) == 0);
    reg_sf.set_n(sub & 0x80);
    reg_a = sub & 0x00FF;
    return 1;
}

// 设置进位标志C, 影响FLAG: C(arry), 
//  CF = 1;
int CPU::SEC(){
    reg_sf.set_c(true);
    return 0;
}

// 设置十进制模式标志D, 影响FLAG: D(Decimal),
// DF = 1
int CPU::SED(){
    reg_sf.set_d(true);
    return 0;
}

// 设置中断禁止标志I, 影响FLAG: I(nterrupt-disable), 
// IF = 1;
int CPU::SEI(){
    reg_sf.set_i(true);
    return 0;
}

// 将累加器A的数送入存储器, 影响FLAG:(无), 
// WRTIE(address, A);
int CPU::STA(){
    p_ram->save(addr_res, reg_a);
    return 0;
}

// 将变址寄存器X的数送入存储器, 影响FLAG:(无),
// WRTIE(address, X);
int CPU::STX(){
    p_ram->save(addr_res, reg_x);
    return 0;
}

// 将变址寄存器Y的数送入存储器, 影响FLAG:(无),
// WRTIE(address, Y);
int CPU::STY(){
    p_ram->save(addr_res, reg_y);
    return 0;
}
// 将累加器A的内容送入变址寄存器X, 影响FLAG:Z(ero),S(ign),
// A = X;
// CHECK_ZSFLAG(A);
int CPU::TAX(){
    reg_x = reg_a;
    reg_sf.set_z(reg_x == 0);
    reg_sf.set_n(reg_x & 0x0080);
    return 0;
}
// 将累加器A的内容送入变址寄存器Y, 影响FLAG:Z(ero),S(ign), 
// Y = A;
// CHECK_ZSFLAG(Y);
int CPU::TAY(){
    reg_y = reg_a;
    reg_sf.set_z(reg_y == 0);
    reg_sf.set_n(reg_y & 0x0080);
    return 0;
}

// 将栈指针SP内容送入变址寄存器X, 影响FLAG:Z(ero),S(ign), 
// X = SP;
// CHECK_ZSFLAG(X);
int CPU::TSX(){
    reg_x = reg_sp;
    reg_sf.set_z(reg_x == 0);
    reg_sf.set_n(reg_x & 0x0080);
    return 0;
}

// 将累加器A的内容送入变址寄存器X, 影响FLAG:Z(ero),S(ign), 
// A = X;
// CHECK_ZSFLAG(A);
int CPU::TXA(){
    reg_a = reg_x;
    reg_sf.set_z(reg_a == 0);
    reg_sf.set_n(reg_a & 0x0080);
    return 0;
}

// 将变址寄存器X内容送入栈指针SP, 影响FLAG:无, 
// SP = X;
int CPU::TXS(){
    reg_sp = reg_x;
    return 0;
}

// 将变址寄存器Y的内容送入累加器A, 影响FLAG:Z(ero),S(ign),
// A = Y;
// CHECK_ZSFLAG(A);
int CPU::TYA(){
    reg_a = reg_y;
    reg_sf.set_z(reg_a == 0);
    reg_sf.set_n(reg_a & 0x0080);
    return 0;
}

// 未定义指令
int CPU::XXX(){
    qDebug() << "zhixing dao  zan wei ding yi de FC zhiling: " << opcode << endl;
    abort();
    return 0;
}

void CPU::run_1cycle()
{
    if (cycles_wait == 0){
        // 上一条指令的时钟周期结束，开始执行下一条指令
        //1.根据PC读指令
        opcode = p_ram->load(reg_pc);
        reg_pc++;
        reg_sf.set_u(true);
        //2.寻址
        int cycles_add_by_addrmode = (this->*inst_table[opcode].addrmode)();
        if (Ppu2.frame_dx == 3)
            print_log();
        int cycles_add_by_operate = (this->*inst_table[opcode].operate)();
        //3.计算指令需要的时钟周期数
        cycles_wait = this->inst_table[opcode].cycle_cnt;
        if (cycles_add_by_operate < 0)
            cycles_wait += (-cycles_add_by_operate);
        else
            cycles_wait += (cycles_add_by_operate & cycles_add_by_addrmode);
        reg_sf.set_u(true);
    }

    cycles_wait--;
    clock_count++;
}

void CPU::print_log() const
{
    char addr_str[20] = {0};
    if (inst_table[opcode].addrmode == &CPU::IMP){
        // do nothing.
    }else if (inst_table[opcode].addrmode == &CPU::IMM){
        sprintf(addr_str, " #%02xH", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::ZP0){
        sprintf(addr_str, " %02xH", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::ZPX){
        sprintf(addr_str, " %02xH, X", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::ZPY){
        sprintf(addr_str, " %02xH, Y", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::REL){
        sprintf(addr_str, " %04xH", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::ABS){
        sprintf(addr_str, " %04xH", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::ABX){
        sprintf(addr_str, " %04xH, X", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::ABY){
        sprintf(addr_str, " %04xH, Y", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::IND){
        sprintf(addr_str, " (%04xH)", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::IZX){
        sprintf(addr_str, " (%04xH, X)", oprand_for_log);
    }else if (inst_table[opcode].addrmode == &CPU::IZY){
        sprintf(addr_str, " (%04xH), Y", oprand_for_log);
    }
    //qDebug() << "Program Counter = " << reg_pc << ", opcode = " << opcode << ", code = " << this->inst_table[opcode].name.c_str() << addr_str << ", A = " << this->reg_a << ", X = " << this->reg_x << ", Y = " << this->reg_y << ", stack pointer = " << reg_sp << ", NVDIZC = " << int(reg_sf.get_n()) << int(reg_sf.get_v()) << int(reg_sf.get_d()) << int(reg_sf.get_i()) << int(reg_sf.get_z()) << int(reg_sf.get_c()) << endl;
    //qDebug() << "Program Counter = " << reg_pc << ", opcode = " << opcode << ", code = " << this->inst_table[opcode].name.c_str() << addr_str << endl;
}
