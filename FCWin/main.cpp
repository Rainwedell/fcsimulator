#include "mainwindow.h"
#include <QApplication>
#include <string>
#include "total.h"
// #include <unistd.h>

using namespace std;

CpuBus Cpubus;
CPU Cpu;
PPU2 Ppu2;
PictureBus PpuBus;
Controller controller_left;
Controller controller_right;
Cartridge cartridge;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //1.读取NES文件
    cartridge.read_from_file("../Data/Super Mario Bros.nes", 40976);
    //cartridge.read_from_file("../Data/mla2.nes", 262160);
    //cartridge.read_from_file("../Data/Contra (U).nes", 131088);
    //2.理论上应该使用线程，559ns执行一次，以后完善
    Cpu.reset();
    Ppu2.reset();
    controller_right.init();
    controller_left.init();
    SetKeyMap();

    //3. 显示NES文件图案表
    MainWindow w;
    w.FCInit();
    w.show();

    return a.exec();
}
