#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <string>
#include "Mapper/mapper_base.h"

using namespace std;

class Cartridge
{
public:
    uint8_t rom_num;  //PRG-ROM 程序只读储存器: 存储程序代码的存储数 单位16kb
    uint8_t vrom_num; //CHR-ROM: 角色只读储存器, 基本是用来显示图像, 放入PPU地址空间 单位16kb
    uint8_t* program_data; //程序数据
    uint8_t* vrom_data; //图像相关的数据
    uint8_t mapper_id; // mapper 编号(使用哪一个Mapper)
    bool has_added_ram; //是否存在多余内存
    Mapper* mapper_ptr; //使用Mapper种类
    void read_from_file(string input_file, int fsize); 
};

#endif
