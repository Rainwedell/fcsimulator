#include "total.h"
#include <string>
#include <fstream>
#include <QDebug>
#include <string.h>

using namespace std;

// 读取并解析nes文件 
void Cartridge::read_from_file(string input_file, int fsize)
{
    //1.
    uint8_t* nes_data = new uint8_t[fsize];
    ifstream stream(input_file, ios::in | ios::binary);
    if (!stream){
        qDebug() << "Read NES File Error!" << endl;
        abort();
    }
    stream.read((char*)nes_data, fsize);
    stream.close();
    // 比较开头4字节
    if (nes_data[0] != 'N' || nes_data[1] != 'E' || nes_data[2] != 'S' || nes_data[3] != '\x1A'){
        qDebug() << "First 4 bytes in file must be NES\\x1A!" << endl;
        abort();
    }
    //2.解析NES文件头部，包括ROM和vram的数量，nametable的映射方式，以及这个卡带使用了哪一种mapper
    // 第4个字节：PRG-ROM: 程序只读储存器的数据长度，单位16kb
    rom_num = nes_data[4];
    // 第5个字节：CHR-ROM: 角色只读储存器, 的数据长度，单位8kb。基本是用来显示图像, 放入PPU地址空间
    vrom_num = nes_data[5];
    // 第6个字节： NNNN FTBM,
    // N: Mapper编号低4位
    // F: 4屏标志位. (如果该位被设置, 则忽略M标志),
    // T: Trainer标志位.  1表示 $7000-$71FF加载 Trainer
    // B: SRAM标志位 $6000-$7FFF拥有电池供电的SRAM.
    // M: 镜像标志位.  0 = 水平, 1 = 垂直.
    uint8_t nametable_mirror = nes_data[6] & 0xb;  //判断镜像
    uint8_t mapper_type = ((nes_data[6] >> 4) & 0xf) | (nes_data[7] & 0xf0);  // 获取Mapper编号
    has_added_ram = bool(nes_data[6] & 0x2);
    //3.新建Mapper类
    if (mapper_type == 0){
        mapper_ptr = new Mapper0();
        mapper_ptr->nametable_mirror = nametable_mirror;
    }else if (mapper_type == 1){
        mapper_ptr = new Mapper1();
        mapper_ptr->nametable_mirror = nametable_mirror;
    }else if (mapper_type == 2){
        mapper_ptr = new Mapper2();
        mapper_ptr->nametable_mirror = nametable_mirror;
    }else{
        qDebug() << "bu zhichi Mapper = " << mapper_type << endl;
        abort();
    }
    //NES解析完成
    //4. 处理完成申请空间，从文件中获取程序指令，图案表的数据
    uint32_t rom_start_dx = 16;
    uint32_t vrom_start_dx = rom_num * 16384 + 16; //16kb后开始
    program_data = new uint8_t[16384 * rom_num];  //申请程序空间
    memcpy(program_data, &nes_data[rom_start_dx], 16384 * rom_num);
    vrom_data = new uint8_t[8192 * vrom_num];     // 申请 图像空间
    memcpy(vrom_data, &nes_data[vrom_start_dx], 8192 * vrom_num);
}
