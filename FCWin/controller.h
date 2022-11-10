#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>
#include <map>
#include <Qt>

using namespace std;

#define FC_KEY_A 0
#define FC_KEY_B 1
#define FC_KEY_SELECT 2
#define FC_KEY_START 3
#define FC_KEY_UP 4
#define FC_KEY_DOWN 5
#define FC_KEY_LEFT 6
#define FC_KEY_RIGHT 7

class Controller
{
private:
    //选通寄存器
    bool strobe; //选通状态
    uint8_t keystate; //按钮状态
public:
    //供CPU调用的接口
    void write_strobe(uint8_t data);
    uint8_t output_key_states();
public:
    void init();
    void get_key_states(); //根据当前真实的按键情况，获取FC手柄中缓存的按键情况. 在MainWindow中维护
    map<int, uint8_t> key_map; //真实按键和FC模拟按键的映射
    bool cur_keystate[8]; //当前真实的按键状态
};

void SetKeyMap();

#endif
